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

BackendRequestId FakeRustBackendClient::devices()
{
    m_deviceRequests.append({QStringLiteral("devices")});
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::mount(const QString &devicePath)
{
    m_deviceRequests.append({QStringLiteral("mount"), devicePath});
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::unmount(const QString &devicePath)
{
    m_deviceRequests.append({QStringLiteral("unmount"), devicePath});
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::remount(const QString &devicePath)
{
    m_deviceRequests.append({QStringLiteral("remount"), devicePath});
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::fileOperation(const FileOperationRequest &request)
{
    m_fileOperationRequests.append(request);
    return nextRequestId();
}

BackendRequestId FakeRustBackendClient::utility(const UtilityRequest &request)
{
    const BackendRequestId requestId = nextRequestId();
    m_utilityRequests.append(request);
    return requestId;
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

void FakeRustBackendClient::completeDevices(
    BackendRequestId requestId,
    const QVector<DeviceEntry> &devices)
{
    emit devicesReady(requestId, devices);
}

void FakeRustBackendClient::completeDeviceOperation(
    BackendRequestId requestId,
    const DeviceOperationResult &result)
{
    emit deviceOperationReady(requestId, result);
}

void FakeRustBackendClient::completeFileOperationProgress(
    BackendRequestId requestId,
    const FileOperationProgress &progress)
{
    emit fileOperationProgress(requestId, progress);
}

void FakeRustBackendClient::completeFileOperation(
    BackendRequestId requestId,
    const FileOperationResult &result)
{
    emit fileOperationReady(requestId, result);
}

void FakeRustBackendClient::completeUtility(
    BackendRequestId requestId,
    const UtilityResult &result)
{
    UtilityResult completed = result;
    completed.requestId = requestId;
    emit utilityReady(requestId, completed);
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

const QVector<QStringList> &FakeRustBackendClient::deviceRequests() const
{
    return m_deviceRequests;
}

const QVector<FileOperationRequest> &FakeRustBackendClient::fileOperationRequests() const
{
    return m_fileOperationRequests;
}

const QVector<UtilityRequest> &FakeRustBackendClient::utilityRequests() const
{
    return m_utilityRequests;
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
