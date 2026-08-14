#pragma once

#include <QHash>
#include <QProcess>
#include <QTimer>

#include "backend/backend_transport.h"

namespace Astrea::Explorer::Native::Backend {

struct PersistentWorkerTransportOptions
{
    QString backendProgram;
    int requestTimeoutMs = 30000;
    int maxLineBytes = 4 * 1024 * 1024;
};

class PersistentWorkerTransport final : public BackendTransport
{
    Q_OBJECT

public:
    explicit PersistentWorkerTransport(
        const PersistentWorkerTransportOptions &options = {},
        QObject *parent = nullptr);
    ~PersistentWorkerTransport() override;

    BackendRequestId start(const QStringList &arguments) override;
    void cancel(BackendRequestId requestId) override;

private:
    struct PendingRequest
    {
        QStringList arguments;
        QTimer *timeout = nullptr;
    };

    QString resolveBackendProgram() const;
    bool ensureWorker();
    void handleWorkerStarted();
    void sendRequest(BackendRequestId requestId);
    void handleReadyRead();
    void handleWorkerError(QProcess::ProcessError error);
    void handleWorkerFinished(int exitCode, QProcess::ExitStatus status);
    void handleTimeout(BackendRequestId requestId);
    void failAll(const QString &code, const QString &message);

    PersistentWorkerTransportOptions m_options;
    QProcess *m_worker = nullptr;
    QByteArray m_readBuffer;
    QHash<BackendRequestId, PendingRequest> m_pending;
    QVector<BackendRequestId> m_waitingRequests;
};

} // namespace Astrea::Explorer::Native::Backend
