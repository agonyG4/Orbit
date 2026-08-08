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
            handleFinished(requestId, exitCode, exitStatus);
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

    request->stdoutData.append(request->process->readAllStandardOutput());
    enforceCap(
        requestId,
        request->stdoutData,
        m_options.maxStdoutBytes,
        "stdout");
}

void OneShotCliTransport::handleStderrReady(BackendRequestId requestId)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr || request->process == nullptr) {
        return;
    }

    request->stderrData.append(request->process->readAllStandardError());
    enforceCap(
        requestId,
        request->stderrData,
        m_options.maxStderrBytes,
        "stderr");
}

void OneShotCliTransport::handleFinished(
    BackendRequestId requestId,
    int exitCode,
    QProcess::ExitStatus exitStatus)
{
    ActiveRequest *request = activeRequest(requestId);
    if (request == nullptr) {
        return;
    }

    if (request->process != nullptr) {
        request->stdoutData.append(request->process->readAllStandardOutput());
        request->stderrData.append(request->process->readAllStandardError());
    }

    if (request->stdoutData.size() > m_options.maxStdoutBytes) {
        BackendTransportError error;
        error.code = QStringLiteral("output_limit_exceeded");
        error.message = QStringLiteral("stdout exceeded configured limit");
        error.requestId = requestId;
        error.exitCode = exitCode;
        error.stderrData = request->stderrData;
        completeFailure(requestId, error);
        return;
    }

    if (request->stderrData.size() > m_options.maxStderrBytes) {
        BackendTransportError error;
        error.code = QStringLiteral("output_limit_exceeded");
        error.message = QStringLiteral("stderr exceeded configured limit");
        error.requestId = requestId;
        error.exitCode = exitCode;
        error.stderrData = request->stderrData;
        completeFailure(requestId, error);
        return;
    }

    if (exitStatus != QProcess::NormalExit) {
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

void OneShotCliTransport::enforceCap(
    BackendRequestId requestId,
    QByteArray &buffer,
    int limit,
    const char *streamName)
{
    if (limit <= 0 || buffer.size() <= limit) {
        return;
    }

    BackendTransportError error;
    error.code = QStringLiteral("output_limit_exceeded");
    error.message = QStringLiteral("%1 exceeded configured limit (%2 > %3 bytes)")
                        .arg(QString::fromLatin1(streamName))
                        .arg(buffer.size())
                        .arg(limit);
    error.requestId = requestId;
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
