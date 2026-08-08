#include "backend/fake_backend_client.h"

namespace Astrea::Explorer::Native::Backend {

FakeRustBackendClient::FakeRustBackendClient(QObject *parent)
    : IRustBackendClient(parent)
{
}

BackendRequestId FakeRustBackendClient::list(const ListRequest &request)
{
    m_listRequests.append(request);
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::search(const SearchRequest &request)
{
    m_searchRequests.append(request);
    return nextRequestId();
}

void FakeRustBackendClient::cancel(BackendRequestId requestId)
{
    m_cancelledRequests.append(requestId);
}

void FakeRustBackendClient::completeList(
    BackendRequestId requestId,
    const QVector<DirectoryEntry> &entries)
{
    emit listReady(requestId, entries);
}

void FakeRustBackendClient::completeSearch(
    BackendRequestId requestId,
    const QVector<DirectoryEntry> &entries)
{
    emit searchReady(requestId, entries);
}

void FakeRustBackendClient::failRequest(
    BackendRequestId requestId,
    const QString &code,
    const QString &message)
{
    BackendError error;
    error.code = code;
    error.message = message;
    error.requestId = requestId;
    emit failed(error);
}

const QVector<ListRequest> &FakeRustBackendClient::listRequests() const
{
    return m_listRequests;
}

const QVector<SearchRequest> &FakeRustBackendClient::searchRequests() const
{
    return m_searchRequests;
}

const QVector<BackendRequestId> &FakeRustBackendClient::cancelledRequests() const
{
    return m_cancelledRequests;
}

BackendRequestId FakeRustBackendClient::nextRequestId()
{
    const BackendRequestId requestId = m_nextRequestId;
    ++m_nextRequestId;
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    return requestId;
}

} // namespace Astrea::Explorer::Native::Backend
