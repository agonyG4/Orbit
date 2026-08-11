#pragma once

#include <QHash>
#include <QObject>
#include <QVector>

#include "backend/backend_types.h"
#include "services/recent_store.h"

namespace Astrea::Explorer::Native::Backend {

class RecentController final : public QObject
{
    Q_OBJECT

public:
    explicit RecentController(
        RecentStore *store = nullptr,
        QObject *parent = nullptr);

    BackendRequestId loadAsync();
    void cancelLoad(BackendRequestId requestId);
    void recordAccess(const DirectoryEntry &entry);

    QVector<DirectoryEntry> currentEntries() const;
    QVector<DirectoryEntry> merge(
        const QVector<RecentRecord> &records,
        int limit = 60) const;

signals:
    void recentReady(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &entries);
    void recentFailed(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QString &message);
    void projectionChanged();
    void persistenceFailed(const QString &message);

private slots:
    void handleStoreLoadReady(
        quint64 storeRequestId,
        const QVector<RecentRecord> &records,
        quintptr workerThreadId);
    void handleStoreRecordsChanged();
    void handleStoreSaveFinished(
        quint64 generation,
        bool success,
        const QString &message,
        quintptr workerThreadId);

private:
    void rebuildProjection();

    RecentStore *m_store = nullptr;
    QHash<quint64, BackendRequestId> m_storeRequests;
    BackendRequestId m_nextRequestId = 0;
    BackendRequestId m_activeRequestId = 0;
    quint64 m_activeStoreRequestId = 0;
    QVector<DirectoryEntry> m_currentEntries;
};

} // namespace Astrea::Explorer::Native::Backend
