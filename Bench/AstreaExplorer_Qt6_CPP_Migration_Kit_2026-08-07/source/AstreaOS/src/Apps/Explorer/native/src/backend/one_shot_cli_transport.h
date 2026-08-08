#pragma once

#include <QHash>

#include "backend/backend_transport.h"

class QProcess;
class QTimer;

namespace Astrea::Explorer::Native::Backend {

struct OneShotCliTransportOptions
{
    QString backendProgram;
    int timeoutMs = 10000;
    int maxStdoutBytes = 1024 * 1024;
    int maxStderrBytes = 128 * 1024;
};

class OneShotCliTransport final : public BackendTransport
{
    Q_OBJECT

public:
    explicit OneShotCliTransport(
        const OneShotCliTransportOptions &options = {},
        QObject *parent = nullptr);
    ~OneShotCliTransport() override;

    BackendRequestId start(const QStringList &arguments) override;
    void cancel(BackendRequestId requestId) override;

private:
    struct ActiveRequest
    {
        QStringList arguments;
        QByteArray stdoutData;
        QByteArray stderrData;
        QProcess *process = nullptr;
        QTimer *timer = nullptr;
    };

    QString resolveBackendProgram() const;
    void handleStdoutReady(BackendRequestId requestId);
    void handleStderrReady(BackendRequestId requestId);
    void handleFinished(
        BackendRequestId requestId,
        int exitCode,
        bool normalExit);
    void handleErrorOccurred(BackendRequestId requestId, const QString &message);
    void handleTimeout(BackendRequestId requestId);
    QByteArray readChunk(QProcess *process,
                         int currentSize,
                         int limit,
                         bool standardError) const;
    bool appendBounded(QByteArray &buffer, const QByteArray &chunk, int limit) const;
    void failOutputLimit(BackendRequestId requestId, const char *streamName);
    void completeSuccess(BackendRequestId requestId, const QByteArray &payload);
    void completeFailure(BackendRequestId requestId, const BackendTransportError &error);
    void terminateProcess(QProcess *process) const;
    ActiveRequest *activeRequest(BackendRequestId requestId);

    OneShotCliTransportOptions m_options;
    QHash<BackendRequestId, ActiveRequest> m_activeRequests;
};

} // namespace Astrea::Explorer::Native::Backend
