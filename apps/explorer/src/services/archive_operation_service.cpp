#include "services/archive_operation_service.h"

namespace Astrea::Explorer::Native::Services {

ArchiveOperationService::ArchiveOperationService(
    Backend::IRustBackendClient *client,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    connect(
        m_client,
        &Backend::IRustBackendClient::archiveOperationProgress,
        this,
        &ArchiveOperationService::handleProgress);
    connect(
        m_client,
        &Backend::IRustBackendClient::archiveOperationReady,
        this,
        &ArchiveOperationService::handleFinished);
    connect(
        m_client,
        &Backend::IRustBackendClient::failed,
        this,
        &ArchiveOperationService::handleFailure);
}

Backend::BackendRequestId ArchiveOperationService::start(
    const Backend::ArchiveOperationRequest &request)
{
    if (m_activeRequest != 0) {
        return 0;
    }
    const Backend::BackendRequestId requestId = m_client->archiveOperation(request);
    if (requestId != 0) {
        m_activeRequest = requestId;
    }
    return requestId;
}

void ArchiveOperationService::cancel(Backend::BackendRequestId requestId)
{
    if (requestId == 0 || requestId != m_activeRequest) {
        return;
    }
    m_client->cancel(requestId);
}

Backend::BackendRequestId ArchiveOperationService::activeRequest() const
{
    return m_activeRequest;
}

void ArchiveOperationService::handleProgress(
    Backend::BackendRequestId requestId,
    const Backend::ArchiveOperationProgress &progress)
{
    if (requestId == m_activeRequest) {
        emit this->progress(requestId, progress);
    }
}

void ArchiveOperationService::handleFinished(
    Backend::BackendRequestId requestId,
    const Backend::ArchiveOperationResult &result)
{
    if (requestId != m_activeRequest) {
        return;
    }
    m_activeRequest = 0;
    emit finished(requestId, result);
}

void ArchiveOperationService::handleFailure(const Backend::BackendError &error)
{
    if (error.requestId != m_activeRequest) {
        return;
    }
    m_activeRequest = 0;
    emit failed(error);
}

} // namespace Astrea::Explorer::Native::Services
