#pragma once

#include <QHash>

#include "backend/backend_transport.h"

class QJsonObject;

namespace Astrea::Explorer::Native::Backend {

class IRustBackendClient : public QObject
{
    Q_OBJECT

public:
    explicit IRustBackendClient(QObject *parent = nullptr);
    ~IRustBackendClient() override;

    virtual BackendRequestId list(const ListRequest &request) = 0;
    virtual BackendRequestId search(const SearchRequest &request) = 0;
    virtual BackendRequestId devices() = 0;
    virtual BackendRequestId mount(const QString &devicePath) = 0;
    virtual BackendRequestId unmount(const QString &devicePath) = 0;
    virtual BackendRequestId remount(const QString &devicePath) = 0;
    virtual BackendRequestId fileOperation(const FileOperationRequest &request) = 0;

public slots:
    virtual void cancel(BackendRequestId requestId) = 0;

signals:
    void listReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &entries);
    void searchReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &entries);
    void devicesReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DeviceEntry> &devices);
    void deviceOperationReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::DeviceOperationResult &result);
    void fileOperationProgress(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::FileOperationProgress &progress);
    void fileOperationReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::FileOperationResult &result);
    void failed(const Astrea::Explorer::Native::Backend::BackendError &error);
};

class RustBackendClient final : public IRustBackendClient
{
    Q_OBJECT

public:
    explicit RustBackendClient(BackendTransport *transport, QObject *parent = nullptr);

    BackendRequestId list(const ListRequest &request) override;
    BackendRequestId search(const SearchRequest &request) override;
    BackendRequestId devices() override;
    BackendRequestId mount(const QString &devicePath) override;
    BackendRequestId unmount(const QString &devicePath) override;
    BackendRequestId remount(const QString &devicePath) override;
    BackendRequestId fileOperation(const FileOperationRequest &request) override;

public slots:
    void cancel(BackendRequestId requestId) override;

private:
    enum class RequestKind
    {
        List,
        Search,
        Devices,
        DeviceOperation,
        FileOperation,
    };

    struct PendingRequest
    {
        RequestKind kind = RequestKind::List;
    };

    QStringList listArguments(const ListRequest &request) const;
    QStringList searchArguments(const SearchRequest &request) const;
    QStringList fileOperationArguments(const FileOperationRequest &request) const;
    QVector<DirectoryEntry> decodeEntries(
        BackendRequestId requestId,
        const QByteArray &payload,
        BackendError *error) const;
    QVector<DeviceEntry> decodeDevices(
        BackendRequestId requestId,
        const QByteArray &payload,
        BackendError *error) const;
    DeviceOperationResult decodeDeviceOperation(
        BackendRequestId requestId,
        const QByteArray &payload,
        BackendError *error) const;
    FileOperationResult decodeFileOperation(
        BackendRequestId requestId,
        const QByteArray &payload,
        BackendError *error,
        QVector<FileOperationProgress> *progresses) const;
    DirectoryEntry decodeEntry(const QJsonObject &object, BackendError *error) const;
    BackendError makeDecodeError(
        BackendRequestId requestId,
        const QString &message) const;

    BackendTransport *m_transport = nullptr;
    QHash<BackendRequestId, PendingRequest> m_pendingRequests;
};

} // namespace Astrea::Explorer::Native::Backend
