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

public slots:
    void cancel(BackendRequestId requestId) override;

public:
    void completeList(BackendRequestId requestId, const QVector<DirectoryEntry> &entries);
    void completeSearch(BackendRequestId requestId, const QVector<DirectoryEntry> &entries);
    void failRequest(BackendRequestId requestId, const QString &code, const QString &message);

    const QVector<ListRequest> &listRequests() const;
    const QVector<SearchRequest> &searchRequests() const;
    const QVector<BackendRequestId> &cancelledRequests() const;

private:
    BackendRequestId nextRequestId();

    BackendRequestId m_nextRequestId = 1;
    QVector<ListRequest> m_listRequests;
    QVector<SearchRequest> m_searchRequests;
    QVector<BackendRequestId> m_cancelledRequests;
};

} // namespace Astrea::Explorer::Native::Backend
