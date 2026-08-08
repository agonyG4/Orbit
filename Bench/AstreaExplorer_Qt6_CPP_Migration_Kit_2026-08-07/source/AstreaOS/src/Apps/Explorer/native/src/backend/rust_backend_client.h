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

public slots:
    virtual void cancel(BackendRequestId requestId) = 0;

signals:
    void listReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &entries);
    void searchReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &entries);
    void failed(const Astrea::Explorer::Native::Backend::BackendError &error);
};

class RustBackendClient final : public IRustBackendClient
{
    Q_OBJECT

public:
    explicit RustBackendClient(BackendTransport *transport, QObject *parent = nullptr);

    BackendRequestId list(const ListRequest &request) override;
    BackendRequestId search(const SearchRequest &request) override;

public slots:
    void cancel(BackendRequestId requestId) override;

private:
    enum class RequestKind
    {
        List,
        Search,
    };

    struct PendingRequest
    {
        RequestKind kind = RequestKind::List;
    };

    QStringList listArguments(const ListRequest &request) const;
    QStringList searchArguments(const SearchRequest &request) const;
    QVector<DirectoryEntry> decodeEntries(
        BackendRequestId requestId,
        const QByteArray &payload,
        BackendError *error) const;
    DirectoryEntry decodeEntry(const QJsonObject &object, BackendError *error) const;
    BackendError makeDecodeError(
        BackendRequestId requestId,
        const QString &message) const;

    BackendTransport *m_transport = nullptr;
    QHash<BackendRequestId, PendingRequest> m_pendingRequests;
};

} // namespace Astrea::Explorer::Native::Backend
