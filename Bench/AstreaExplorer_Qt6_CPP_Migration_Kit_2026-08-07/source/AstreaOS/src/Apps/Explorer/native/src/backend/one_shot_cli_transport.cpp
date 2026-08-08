#include "backend/one_shot_cli_transport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

namespace Astrea::Explorer::Native::Backend {

BackendTransport::BackendTransport(QObject *parent)
    : QObject(parent)
{
}

BackendTransport::~BackendTransport() = default;

BackendRequestId BackendTransport::allocateRequestId()
{
    const BackendRequestId requestId = m_nextRequestId;
    ++m_nextRequestId;
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return requestId;
}

void BackendTransport::emitCompleted(BackendRequestId requestId, const QByteArray &payload)
{
    emit completed(requestId, payload);
}

void BackendTransport::emitFailed(
    BackendRequestId requestId,
    const BackendTransportError &error)
{
    emit failed(requestId, error);
}

OneShotCliTransport::OneShotCliTransport(
    const OneShotCliTransportOptions &options,
    QObject *parent)
    : BackendTransport(parent)
    , m_options(options)
{
}

OneShotCliTransport::~OneShotCliTransport()
{
    const QList<BackendRequestId> activeIds = m_activeRequests.keys();
    for (const BackendRequestId requestId : activeIds) {
        cancel(requestId);
    }
}

BackendRequestId OneShotCliTransport::start(const QStringList &arguments)
{
    const BackendRequestId requestId = allocateRequestId();

    ActiveRequest request;
    request.arguments = arguments;
    request.process = new QProcess(this);
    request.timer = new QTimer(this);
    request.timer->setSingleShot(true);

    m_activeRequests.insert(requestId, request);
    ActiveRequest *active = activeRequest(requestId);
    Q_ASSERT(active != nullptr);

    active->process->setProgram(resolveBackendProgram());
    active->process->setArguments(arguments);
    active->process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(
        active->process,
        &QProcess::readyReadStandardOutput,
        this,
        [this, requestId]() { handleStdoutReady(requestId); });
    connect(
        active->process,
        &QProcess::readyReadStandardError,
        this,
        [this, requestId]() { handleStderrReady(requestId); });
    connect(
        active->process,
        &QProcess::errorOccurred,
        this,
        [this, requestId](QProcess::ProcessError) {
            ActiveRequest *current = activeRequest(requestId);
            if (current == nullptr || current->process == nullptr) {
                return;
            }
            handleErrorOccurred(requestId, current->process->errorString());
        });
    connect(
        active->process,
        &QProcess::finished,
        this,
        [this, requestId](int exitCode, QProcess::ExitStatus exitStatus) {
            handleFinished(requestId, exitCode, exitStatus == QProcess::NormalExit);
        });
    connect(
        active->timer,
        &QTimer::timeout,
        this,
        [this, requestId]() { handleTimeout(requestId); });

    active->process->start();
    active->timer->start(m_options.timeoutMs);

    return requestId;
}

void OneShotCliTransport::cancel(BackendRequestId requestId)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    BackendTransportError error;
    error.code = QStringLiteral("cancelled");
    error.message = QStringLiteral("request cancelled");
    error.requestId = requestId;
    completeFailure(requestId, error);
}

QString OneShotCliTransport::resolveBackendProgram() const
{
    if (!m_options.backendProgram.isEmpty()) {
        return m_options.backendProgram;
    }

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        applicationDir.filePath(QStringLiteral("../libexec/explorer_backend")),
        applicationDir.filePath(QStringLiteral("../libexec/astrea/explorer_backend")),
        applicationDir.filePath(QStringLiteral("explorer_backend")),
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }

    return QDir::cleanPath(candidates.constFirst());
}

void OneShotCliTransport::handleStdoutReady(BackendRequestId requestId)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr || request->process == nullptr) {
        return;
    }

    const QByteArray chunk = readChunk(
        request->process,
        request->stdoutData.size(),
        m_options.maxStdoutBytes,
        false);
    if (!appendBounded(request->stdoutData, chunk, m_options.maxStdoutBytes)) {
        failOutputLimit(requestId, "stdout");
    }
}

void OneShotCliTransport::handleStderrReady(BackendRequestId requestId)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr || request->process == nullptr) {
        return;
    }

    const QByteArray chunk = readChunk(
        request->process,
        request->stderrData.size(),
        m_options.maxStderrBytes,
        true);
    if (!appendBounded(request->stderrData, chunk, m_options.maxStderrBytes)) {
        failOutputLimit(requestId, "stderr");
    }
}

void OneShotCliTransport::handleFinished(
    BackendRequestId requestId,
    int exitCode,
    bool normalExit)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    if (request->process != nullptr) {
        const QByteArray stdoutChunk = readChunk(
            request->process,
            request->stdoutData.size(),
            m_options.maxStdoutBytes,
            false);
        if (!appendBounded(request->stdoutData, stdoutChunk, m_options.maxStdoutBytes)) {
            failOutputLimit(requestId, "stdout");
            return;
        }

        const QByteArray stderrChunk = readChunk(
            request->process,
            request->stderrData.size(),
            m_options.maxStderrBytes,
            true);
        if (!appendBounded(request->stderrData, stderrChunk, m_options.maxStderrBytes)) {
            failOutputLimit(requestId, "stderr");
            return;
        }
    }

    if (!normalExit) {
        BackendTransportError error;
        error.code = QStringLiteral("process_crashed");
        error.message = QStringLiteral("backend process crashed");
        error.requestId = requestId;
        error.exitCode = exitCode;
        error.stderrData = request->stderrData;
        completeFailure(requestId, error);
        return;
    }

    if (exitCode != 0) {
        BackendTransportError error;
        error.code = QStringLiteral("backend_exit");
        error.message = QStringLiteral("backend exited with code %1: %2")
                            .arg(exitCode)
                            .arg(QString::fromUtf8(request->stderrData).trimmed());
        error.requestId = requestId;
        error.exitCode = exitCode;
        error.stderrData = request->stderrData;
        completeFailure(requestId, error);
        return;
    }

    completeSuccess(requestId, request->stdoutData);
}

void OneShotCliTransport::handleErrorOccurred(
    BackendRequestId requestId,
    const QString &message)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    BackendTransportError error;
    error.code = QStringLiteral("process_error");
    error.message = message;
    error.requestId = requestId;
    error.stderrData = request->stderrData;
    completeFailure(requestId, error);
}

void OneShotCliTransport::handleTimeout(BackendRequestId requestId)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    BackendTransportError error;
    error.code = QStringLiteral("timeout");
    error.message = QStringLiteral("backend request timed out after %1 ms")
                        .arg(m_options.timeoutMs);
    error.requestId = requestId;
    error.stderrData = request->stderrData;
    completeFailure(requestId, error);
}

QByteArray OneShotCliTransport::readChunk(
    QProcess *process,
    int currentSize,
    int limit,
    bool standardError) const
{
    if (process == nullptr) {
        return {};
    }

    if (limit <= 0) {
        return standardError ? process->readAllStandardError()
                             : process->readAllStandardOutput();
    }

    const qint64 remaining = qMax(0, limit - currentSize);
    process->setReadChannel(standardError ? QProcess::StandardError
                                          : QProcess::StandardOutput);
    return process->read(remaining + 1);
}

bool OneShotCliTransport::appendBounded(
    QByteArray &buffer,
    const QByteArray &chunk,
    int limit) const
{
    if (limit <= 0) {
        buffer.append(chunk);
        return true;
    }

    const qint64 remaining = qMax(0, limit - buffer.size());
    if (chunk.size() > remaining) {
        if (remaining > 0) {
            buffer.append(chunk.constData(), remaining);
        }
        return false;
    }

    buffer.append(chunk);
    return true;
}

void OneShotCliTransport::failOutputLimit(
    BackendRequestId requestId,
    const char *streamName)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    BackendTransportError error;
    error.code = QStringLiteral("output_limit_exceeded");
    error.message = QStringLiteral("%1 exceeded configured limit")
                        .arg(QString::fromLatin1(streamName));
    error.requestId = requestId;
    if (m_options.maxStderrBytes > 0) {
        error.stderrData = request->stderrData.left(m_options.maxStderrBytes);
    } else {
        error.stderrData = request->stderrData;
    }
    completeFailure(requestId, error);
}

void OneShotCliTransport::completeSuccess(
    BackendRequestId requestId,
    const QByteArray &payload)
{
    ActiveRequest request = m_activeRequests.take(requestId);
    if (request.timer != nullptr) {
        request.timer->stop();
        request.timer->deleteLater();
    }
    if (request.process != nullptr) {
        request.process->deleteLater();
    }
    emitCompleted(requestId, payload);
}

void OneShotCliTransport::completeFailure(
    BackendRequestId requestId,
    const BackendTransportError &error)
{
    ActiveRequest request = m_activeRequests.take(requestId);
    if (request.timer != nullptr) {
        request.timer->stop();
        request.timer->deleteLater();
    }
    if (request.process != nullptr) {
        terminateProcess(request.process);
        request.process->deleteLater();
    }
    emitFailed(requestId, error);
}

void OneShotCliTransport::terminateProcess(QProcess *process) const
{
    if (process == nullptr || process->state() == QProcess::NotRunning) {
        return;
    }

    process->terminate();
    if (!process->waitForFinished(250)) {
        process->kill();
        process->waitForFinished(250);
    }
}

OneShotCliTransport::ActiveRequest *OneShotCliTransport::activeRequest(
    BackendRequestId requestId)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end()) {
        return nullptr;
    }
    return &it.value();
}

} // namespace Astrea::Explorer::Native::Backend
