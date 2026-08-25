#include "controllers/recent_controller.h"

#include <algorithm>
#include <utility>

#include <QDateTime>

namespace Astrea::Explorer::Native::Backend {

RecentController::RecentController(RecentStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    if (m_store == nullptr) {
        return;
    }
    connect(
        m_store,
        &RecentStore::loadReady,
        this,
        &RecentController::handleStoreLoadReady);
    connect(
        m_store,
        &RecentStore::recordsChanged,
        this,
        &RecentController::handleStoreRecordsChanged);
    connect(
        m_store,
        &RecentStore::saveFinished,
        this,
        &RecentController::handleStoreSaveFinished);
}

BackendRequestId RecentController::loadAsync()
{
    if (m_store == nullptr) {
        return 0;
    }
    if (m_activeStoreRequestId != 0) {
        m_store->cancelLoad(m_activeStoreRequestId);
        m_storeRequests.remove(m_activeStoreRequestId);
    }
    const BackendRequestId requestId = ++m_nextRequestId;
    const quint64 storeRequestId = m_store->load();
    m_storeRequests.insert(storeRequestId, requestId);
    m_activeRequestId = requestId;
    m_activeStoreRequestId = storeRequestId;
    return requestId;
}

void RecentController::cancelLoad(BackendRequestId requestId)
{
    if (requestId == 0 || requestId != m_activeRequestId) {
        return;
    }
    m_store->cancelLoad(m_activeStoreRequestId);
    m_storeRequests.remove(m_activeStoreRequestId);
    m_activeRequestId = 0;
    m_activeStoreRequestId = 0;
}

void RecentController::recordAccess(const DirectoryEntry &entry)
{
    if (m_store == nullptr || entry.filePath.isEmpty()) {
        return;
    }
    RecentRecord record;
    record.entry = entry;
    record.lastAccessed = QDateTime::currentMSecsSinceEpoch();
    record.entry.lastAccessed = record.lastAccessed;
    record.entry.fileModified = QDateTime::fromMSecsSinceEpoch(record.lastAccessed);
    record.source = QStringLiteral("finder");
    m_store->recordAccess(record);
}

QVector<DirectoryEntry> RecentController::currentEntries() const
{
    return m_currentEntries;
}

QVector<DirectoryEntry> RecentController::merge(
    const QVector<RecentRecord> &records,
    int limit) const
{
    if (limit <= 0) {
        return {};
    }

    QVector<RecentRecord> unique;
    QHash<QString, int> indexes;
    for (const RecentRecord &record : records) {
        const QString path = record.entry.filePath;
        if (path.isEmpty()) {
            continue;
        }

        const auto existing = indexes.constFind(path);
        if (existing == indexes.constEnd()) {
            indexes.insert(path, unique.size());
            unique.append(record);
        } else if (record.lastAccessed >= unique.at(existing.value()).lastAccessed) {
            unique[existing.value()] = record;
        }
    }

    std::stable_sort(unique.begin(), unique.end(), [](const RecentRecord &left, const RecentRecord &right) {
        return left.lastAccessed > right.lastAccessed;
    });

    QVector<DirectoryEntry> result;
    result.reserve(qMin(limit, unique.size()));
    for (int i = 0; i < unique.size() && i < limit; ++i) {
        DirectoryEntry entry = unique.at(i).entry;
        entry.lastAccessed = unique.at(i).lastAccessed;
        if (entry.lastAccessed > 0) {
            entry.fileModified = QDateTime::fromMSecsSinceEpoch(entry.lastAccessed);
        }
        entry.recentSource = unique.at(i).source;
        result.append(std::move(entry));
    }
    return result;
}

void RecentController::handleStoreLoadReady(
    quint64 storeRequestId,
    const QVector<RecentRecord> &records,
    quintptr workerThreadId)
{
    Q_UNUSED(workerThreadId);
    const auto requestIt = m_storeRequests.constFind(storeRequestId);
    if (requestIt == m_storeRequests.constEnd()
        || storeRequestId != m_activeStoreRequestId) {
        return;
    }
    const BackendRequestId requestId = requestIt.value();
    m_storeRequests.remove(storeRequestId);
    m_activeStoreRequestId = 0;
    m_activeRequestId = 0;
    m_currentEntries = merge(records, m_store->limit());
    emit recentReady(requestId, m_currentEntries);
    emit projectionChanged();
}

void RecentController::handleStoreRecordsChanged()
{
    if (m_store == nullptr) {
        return;
    }
    m_currentEntries = merge(m_store->records(), m_store->limit());
    emit projectionChanged();
}

void RecentController::handleStoreSaveFinished(
    quint64 generation,
    bool success,
    const QString &message,
    quintptr workerThreadId)
{
    Q_UNUSED(generation);
    Q_UNUSED(workerThreadId);
    if (!success) {
        emit persistenceFailed(message);
    }
}

void RecentController::rebuildProjection()
{
    if (m_store == nullptr) {
        m_currentEntries.clear();
        return;
    }
    m_currentEntries = merge(m_store->records(), m_store->limit());
}

} // namespace Astrea::Explorer::Native::Backend
