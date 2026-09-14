#include "services/directory_metrics_service.h"

namespace Astrea::Explorer::Native::Services {

DirectoryMetricsService::DirectoryMetricsService(
    Backend::IRustBackendClient *client,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    qRegisterMetaType<Backend::DirectoryMetricsProgress>();
    qRegisterMetaType<Backend::DirectoryMetricsResult>();
    connect(
        m_client,
        &Backend::IRustBackendClient::directoryMetricsProgress,
        this,
        &DirectoryMetricsService::handleProgress);
    connect(
        m_client,
        &Backend::IRustBackendClient::directoryMetricsReady,
        this,
        &DirectoryMetricsService::handleFinished);
    connect(
        m_client,
        &Backend::IRustBackendClient::failed,
        this,
        &DirectoryMetricsService::handleFailure);
}

Backend::BackendRequestId DirectoryMetricsService::start(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return 0;
    }
    if (m_activeRequest != 0) {
        const Backend::BackendRequestId oldRequest = m_activeRequest;
        m_activeRequest = 0;
        m_client->cancel(oldRequest);
    }
    Backend::DirectoryMetricsRequest request;
    request.paths = paths;
    const Backend::BackendRequestId requestId = m_client->directoryMetrics(request);
    if (requestId != 0) {
        m_activeRequest = requestId;
    }
    return requestId;
}

void DirectoryMetricsService::cancel(Backend::BackendRequestId requestId)
{
    if (requestId == 0 || requestId != m_activeRequest) {
        return;
    }
    m_client->cancel(requestId);
}

Backend::BackendRequestId DirectoryMetricsService::activeRequest() const
{
    return m_activeRequest;
}

void DirectoryMetricsService::handleProgress(
    Backend::BackendRequestId requestId,
    const Backend::DirectoryMetricsProgress &progress)
{
    if (requestId == m_activeRequest) {
        emit this->progress(requestId, progress);
    }
}

void DirectoryMetricsService::handleFinished(
    Backend::BackendRequestId requestId,
    const Backend::DirectoryMetricsResult &result)
{
    if (requestId != m_activeRequest) {
        return;
    }
    m_activeRequest = 0;
    emit finished(requestId, result);
}

void DirectoryMetricsService::handleFailure(const Backend::BackendError &error)
{
    if (error.requestId != m_activeRequest) {
        return;
    }
    m_activeRequest = 0;
    if (error.code == QStringLiteral("cancelled")) {
        Backend::DirectoryMetricsResult result;
        result.requestId = error.requestId;
        result.operation = QStringLiteral("directory-metrics");
        result.state = QStringLiteral("cancelled");
        result.errorCode = error.code;
        result.errorMessage = error.message;
        emit finished(error.requestId, result);
        return;
    }
    emit failed(error);
}

} // namespace Astrea::Explorer::Native::Services
