#pragma once

#include <QVector>

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Backend {

class FakeRustBackendClient final : public IRustBackendClient
{
    Q_OBJECT

public:
    explicit FakeRustBackendClient(QObject *parent = nullptr);

    BackendRequestId list(const ListRequest &request) override;
    BackendRequestId search(const SearchRequest &request) override;
    BackendRequestId devices() override;
    BackendRequestId mount(const QString &devicePath) override;
    BackendRequestId unmount(const QString &devicePath) override;
    BackendRequestId remount(const QString &devicePath) override;
    BackendRequestId fileOperation(const FileOperationRequest &request) override;
    BackendRequestId utility(const UtilityRequest &request) override;

public slots:
    void cancel(BackendRequestId requestId) override;

public:
    void completeList(BackendRequestId requestId, const QVector<DirectoryEntry> &entries);
    void completeSearch(BackendRequestId requestId, const QVector<DirectoryEntry> &entries);
    void completeDevices(BackendRequestId requestId, const QVector<DeviceEntry> &devices);
    void completeDeviceOperation(
        BackendRequestId requestId,
        const DeviceOperationResult &result);
    void completeFileOperationProgress(
        BackendRequestId requestId,
        const FileOperationProgress &progress);
    void completeFileOperation(
        BackendRequestId requestId,
        const FileOperationResult &result);
    void completeUtility(
        BackendRequestId requestId,
        const UtilityResult &result);
    void failRequest(BackendRequestId requestId, const QString &code, const QString &message);

    const QVector<ListRequest> &listRequests() const;
    const QVector<SearchRequest> &searchRequests() const;
    const QVector<QStringList> &deviceRequests() const;
    const QVector<FileOperationRequest> &fileOperationRequests() const;
    const QVector<UtilityRequest> &utilityRequests() const;
    const QVector<BackendRequestId> &cancelledRequests() const;

private:
    BackendRequestId nextRequestId();

    BackendRequestId m_nextRequestId = 1;
    QVector<ListRequest> m_listRequests;
    QVector<SearchRequest> m_searchRequests;
    QVector<QStringList> m_deviceRequests;
    QVector<FileOperationRequest> m_fileOperationRequests;
    QVector<UtilityRequest> m_utilityRequests;
    QVector<BackendRequestId> m_cancelledRequests;
};

} // namespace Astrea::Explorer::Native::Backend
