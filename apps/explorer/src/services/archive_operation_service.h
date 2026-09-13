#pragma once

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Services {

class ArchiveOperationService final : public QObject
{
    Q_OBJECT

public:
    explicit ArchiveOperationService(
        Backend::IRustBackendClient *client,
        QObject *parent = nullptr);

    Backend::BackendRequestId start(const Backend::ArchiveOperationRequest &request);
    void cancel(Backend::BackendRequestId requestId);
    Backend::BackendRequestId activeRequest() const;

signals:
    void progress(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::ArchiveOperationProgress &progress);
    void finished(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::ArchiveOperationResult &result);
    void failed(const Astrea::Explorer::Native::Backend::BackendError &error);

private slots:
    void handleProgress(
        Backend::BackendRequestId requestId,
        const Backend::ArchiveOperationProgress &progress);
    void handleFinished(
        Backend::BackendRequestId requestId,
        const Backend::ArchiveOperationResult &result);
    void handleFailure(const Backend::BackendError &error);

private:
    Backend::IRustBackendClient *m_client = nullptr;
    Backend::BackendRequestId m_activeRequest = 0;
};

} // namespace Astrea::Explorer::Native::Services
