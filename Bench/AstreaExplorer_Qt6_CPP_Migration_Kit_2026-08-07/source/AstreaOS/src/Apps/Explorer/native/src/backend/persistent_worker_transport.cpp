#include "backend/persistent_worker_transport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Astrea::Explorer::Native::Backend {

namespace {

void writeCancellation(QProcess *worker, BackendRequestId requestId)
{
    if (worker == nullptr || worker->state() != QProcess::Running) {
        return;
    }
    const QJsonObject request {
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), QStringLiteral("cancel-%1").arg(requestId)},
        {QStringLiteral("arguments"), QJsonArray {
            QStringLiteral("cancel"),
            QString::number(requestId),
        }},
    };
    worker->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
}

} // namespace

PersistentWorkerTransport::PersistentWorkerTransport(
    const PersistentWorkerTransportOptions &options,
    QObject *parent)
    : BackendTransport(parent)
    , m_options(options)
    , m_worker(new QProcess(this))
{
    m_worker->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_worker, &QProcess::readyReadStandardOutput, this, &PersistentWorkerTransport::handleReadyRead);
    connect(m_worker, &QProcess::errorOccurred, this, &PersistentWorkerTransport::handleWorkerError);
    connect(m_worker, &QProcess::finished, this, &PersistentWorkerTransport::handleWorkerFinished);
}

PersistentWorkerTransport::~PersistentWorkerTransport()
{
    failAll(QStringLiteral("worker_shutdown"), QStringLiteral("backend worker shut down"));
    if (m_worker->state() != QProcess::NotRunning) {
        m_worker->terminate();
        if (!m_worker->waitForFinished(250)) {
            m_worker->kill();
            m_worker->waitForFinished(250);
        }
    }
}

BackendRequestId PersistentWorkerTransport::start(const QStringList &arguments)
{
    const BackendRequestId requestId = allocateRequestId();
    if (!ensureWorker()) {
        BackendTransportError error;
        error.code = QStringLiteral("worker_start_failed");
        error.message = QStringLiteral("could not start persistent Explorer backend worker");
        error.requestId = requestId;
        emitFailed(requestId, error);
        return requestId;
    }

    QJsonArray encodedArguments;
    for (const QString &argument : arguments) {
        encodedArguments.append(argument);
    }
    const QJsonObject request {
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), QString::number(requestId)},
        {QStringLiteral("arguments"), encodedArguments},
    };
    m_worker->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');

    PendingRequest pending;
    pending.timeout = new QTimer(this);
    pending.timeout->setSingleShot(true);
    connect(pending.timeout, &QTimer::timeout, this, [this, requestId]() {
        handleTimeout(requestId);
    });
    m_pending.insert(requestId, pending);
    pending.timeout->start(m_options.requestTimeoutMs);
    return requestId;
}

void PersistentWorkerTransport::cancel(BackendRequestId requestId)
{
    auto it = m_pending.find(requestId);
    if (it == m_pending.end()) {
        return;
    }
    BackendTransportError error;
    error.code = QStringLiteral("cancelled");
    error.message = QStringLiteral("request cancelled");
    error.requestId = requestId;
    writeCancellation(m_worker, requestId);
    emitFailed(requestId, error);
    if (it->timeout != nullptr) {
        it->timeout->deleteLater();
    }
    m_pending.erase(it);
}

QString PersistentWorkerTransport::resolveBackendProgram() const
{
    if (!m_options.backendProgram.isEmpty()) {
        return m_options.backendProgram;
    }
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates {
        applicationDir.filePath(QStringLiteral("../libexec/explorer_backend")),
        applicationDir.filePath(QStringLiteral("../libexec/astrea/explorer_backend")),
        applicationDir.filePath(QStringLiteral("explorer_backend")),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo(candidate).isFile()) {
            return QDir::cleanPath(candidate);
        }
    }
    return QDir::cleanPath(candidates.constLast());
}

bool PersistentWorkerTransport::ensureWorker()
{
    if (m_worker->state() == QProcess::Running) {
        return true;
    }
    m_worker->setProgram(resolveBackendProgram());
    m_worker->setArguments({QStringLiteral("serve")});
    m_worker->start();
    return m_worker->waitForStarted(2000);
}

void PersistentWorkerTransport::handleReadyRead()
{
    m_readBuffer.append(m_worker->readAllStandardOutput());
    if (m_options.maxLineBytes > 0 && m_readBuffer.size() > m_options.maxLineBytes) {
        failAll(QStringLiteral("output_limit_exceeded"), QStringLiteral("worker response exceeded line limit"));
        m_worker->kill();
        return;
    }

    while (true) {
        const qsizetype newline = m_readBuffer.indexOf('\n');
        if (newline < 0) {
            return;
        }
        const QByteArray line = m_readBuffer.left(newline).trimmed();
        m_readBuffer.remove(0, newline + 1);
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            failAll(QStringLiteral("protocol_error"), QStringLiteral("invalid persistent worker response"));
            m_worker->kill();
            return;
        }
        const QJsonObject response = document.object();
        bool okId = false;
        const BackendRequestId requestId = response.value(QStringLiteral("id")).toString().toULongLong(&okId);
        if (!okId || !m_pending.contains(requestId)) {
            continue;
        }
        PendingRequest pending = m_pending.take(requestId);
        if (pending.timeout != nullptr) {
            pending.timeout->stop();
            pending.timeout->deleteLater();
        }
        if (response.value(QStringLiteral("ok")).toBool(false)) {
            emitCompleted(requestId, response.value(QStringLiteral("payload")).toString().toUtf8());
        } else {
            BackendTransportError error;
            error.code = response.value(QStringLiteral("errorCode")).toString(QStringLiteral("worker_error"));
            error.message = response.value(QStringLiteral("error")).toString(QStringLiteral("backend worker request failed"));
            error.requestId = requestId;
            emitFailed(requestId, error);
        }
    }
}

void PersistentWorkerTransport::handleWorkerError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        failAll(QStringLiteral("worker_start_failed"), m_worker->errorString());
    }
}

void PersistentWorkerTransport::handleWorkerFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    if (!m_pending.isEmpty()) {
        failAll(QStringLiteral("worker_crashed"), QStringLiteral("backend worker exited unexpectedly"));
    }
    m_readBuffer.clear();
}

void PersistentWorkerTransport::handleTimeout(BackendRequestId requestId)
{
    auto it = m_pending.find(requestId);
    if (it == m_pending.end()) {
        return;
    }
    BackendTransportError error;
    error.code = QStringLiteral("timeout");
    error.message = QStringLiteral("persistent backend request timed out");
    error.requestId = requestId;
    writeCancellation(m_worker, requestId);
    emitFailed(requestId, error);
    it->timeout->deleteLater();
    m_pending.erase(it);
}

void PersistentWorkerTransport::failAll(const QString &code, const QString &message)
{
    const auto requests = m_pending.keys();
    for (const BackendRequestId requestId : requests) {
        auto it = m_pending.find(requestId);
        if (it == m_pending.end()) {
            continue;
        }
        BackendTransportError error;
        error.code = code;
        error.message = message;
        error.requestId = requestId;
        emitFailed(requestId, error);
        if (it->timeout != nullptr) {
            it->timeout->deleteLater();
        }
        m_pending.erase(it);
    }
}

} // namespace Astrea::Explorer::Native::Backend
