#include "services/file_operation_service.h"

namespace Astrea::Explorer::Native::Services {

FileOperationService::FileOperationService(
    Backend::IRustBackendClient *client,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    connect(
        m_client,
        &Backend::IRustBackendClient::fileOperationProgress,
        this,
        &FileOperationService::handleProgress);
    connect(
        m_client,
        &Backend::IRustBackendClient::fileOperationReady,
        this,
        &FileOperationService::handleFinished);
    connect(
        m_client,
        &Backend::IRustBackendClient::failed,
        this,
        &FileOperationService::handleFailure);
}

Backend::BackendRequestId FileOperationService::start(
    const Backend::FileOperationRequest &request)
{
    const Backend::BackendRequestId requestId = m_client->fileOperation(request);
    m_activeRequests.insert(requestId);
    return requestId;
}

void FileOperationService::cancel(Backend::BackendRequestId requestId)
{
    if (!m_activeRequests.contains(requestId)) {
        return;
    }
    m_client->cancel(requestId);
}

void FileOperationService::handleProgress(
    Backend::BackendRequestId requestId,
    const Backend::FileOperationProgress &progress)
{
    if (m_activeRequests.contains(requestId)) {
        emit this->progress(requestId, progress);
    }
}

void FileOperationService::handleFinished(
    Backend::BackendRequestId requestId,
    const Backend::FileOperationResult &result)
{
    if (!m_activeRequests.remove(requestId)) {
        return;
    }
    emit finished(requestId, result);
}

void FileOperationService::handleFailure(const Backend::BackendError &error)
{
    if (!m_activeRequests.remove(error.requestId)) {
        return;
    }
    emit failed(error);
}

} // namespace Astrea::Explorer::Native::Services
