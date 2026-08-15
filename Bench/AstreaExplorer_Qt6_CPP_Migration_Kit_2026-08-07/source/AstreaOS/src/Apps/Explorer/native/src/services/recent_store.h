#pragma once

#include <functional>
#include <optional>

#include <QDateTime>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>

#include "backend/backend_types.h"
#include "services/desktop_application_catalog.h"

namespace Astrea::Explorer::Native::Backend {

struct RecentSourcePaths
{
    QString finderPath;
    QString launchHistoryPath;
    QString xbelPath;
    int limit = 60;
};

struct RecentRecord
{
    DirectoryEntry entry;
    qint64 lastAccessed = 0;
    QString source;
};

class RecentStore final : public QObject
{
    Q_OBJECT

public:
    using Dispatch = std::function<void(std::function<void()>)>;

    explicit RecentStore(
        RecentSourcePaths paths,
        QObject *parent = nullptr,
        Dispatch dispatch = {},
        Services::DesktopApplicationCatalog *catalog = nullptr);

    quint64 load();
    void cancelLoad(quint64 requestId);
    void recordAccess(const RecentRecord &record);

    QVector<RecentRecord> records() const;
    int limit() const;
    quint64 persistenceGeneration() const;

signals:
    void loadReady(
        quint64 requestId,
        const QVector<Astrea::Explorer::Native::Backend::RecentRecord> &records,
        quintptr workerThreadId);
    void loadFailed(quint64 requestId, const QString &message);
    void saveFinished(
        quint64 generation,
        bool success,
        const QString &message,
        quintptr workerThreadId);
    void recordsChanged();

private:
    struct PendingSave
    {
        quint64 generation = 0;
        QVector<RecentRecord> records;
    };

    static QVector<RecentRecord> loadSources(
        const RecentSourcePaths &paths,
        const Services::DesktopApplicationCatalog::Snapshot &catalog);
    static QVector<RecentRecord> mergeRecords(
        const QVector<RecentRecord> &records,
        int limit);
    static QVector<RecentRecord> persistedRecords(
        const QVector<RecentRecord> &records,
        int limit);
    static bool saveFinderRecords(
        const QString &path,
        const QVector<RecentRecord> &records,
        QString *error);

    void scheduleSave();
    void startSave(PendingSave save);
    void acceptLoadedRecords(
        quint64 requestId,
        QVector<RecentRecord> records,
        quintptr workerThreadId);
    void finishSave(
        quint64 generation,
        bool success,
        const QString &message,
        quintptr workerThreadId);

    RecentSourcePaths m_paths;
    Dispatch m_dispatch;
    Services::DesktopApplicationCatalog *m_catalog = nullptr;
    QVector<RecentRecord> m_records;
    QHash<QString, RecentRecord> m_localRecords;
    quint64 m_nextLoadRequest = 0;
    quint64 m_activeLoadRequest = 0;
    quint64 m_persistenceGeneration = 0;
    bool m_saveRunning = false;
    std::optional<PendingSave> m_pendingSave;
};

} // namespace Astrea::Explorer::Native::Backend

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::RecentRecord)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::RecentRecord>)
