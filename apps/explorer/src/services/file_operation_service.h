#pragma once

#include <QSet>

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Services {

class FileOperationService final : public QObject
{
    Q_OBJECT

public:
    explicit FileOperationService(
        Backend::IRustBackendClient *client,
        QObject *parent = nullptr);

    Backend::BackendRequestId start(const Backend::FileOperationRequest &request);
    void cancel(Backend::BackendRequestId requestId);

signals:
    void progress(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::FileOperationProgress &progress);
    void finished(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::FileOperationResult &result);
    void failed(const Astrea::Explorer::Native::Backend::BackendError &error);

private slots:
    void handleProgress(
        Backend::BackendRequestId requestId,
        const Backend::FileOperationProgress &progress);
    void handleFinished(
        Backend::BackendRequestId requestId,
        const Backend::FileOperationResult &result);
    void handleFailure(const Backend::BackendError &error);

private:
    Backend::IRustBackendClient *m_client = nullptr;
    QSet<Backend::BackendRequestId> m_activeRequests;
};

} // namespace Astrea::Explorer::Native::Services
