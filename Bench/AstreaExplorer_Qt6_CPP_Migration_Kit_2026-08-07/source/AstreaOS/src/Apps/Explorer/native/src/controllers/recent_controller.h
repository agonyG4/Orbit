#pragma once

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>

#include "backend/backend_types.h"
#include "controllers/open_with_controller.h"

namespace Astrea::Explorer::Native::Backend {

struct RecentRecord
{
    DirectoryEntry entry;
    qint64 lastAccessed = 0;
    QString source;
};

struct RecentSourcePaths
{
    QString finderPath;
    QString launchHistoryPath;
    QString xbelPath;
    int limit = 60;
};

class RecentController final
{
public:
    QVector<DirectoryEntry> load(const RecentSourcePaths &paths) const;
    QVector<DirectoryEntry> merge(
        const QVector<RecentRecord> &records,
        int limit = 60) const;

private:
    static RecentRecord recordFromPath(
        const QString &path,
        qint64 lastAccessed,
        const QString &source,
        const QString &kind = QString());
    static RecentRecord recordFromObject(
        const QJsonObject &object,
        const QString &source);
    static RecentRecord recordFromDesktop(
        const QString &desktopId,
        const QJsonArray &argv,
        qint64 lastAccessed,
        const QString &source,
        QHash<QString, QString> *desktopPathCache = nullptr);
    static bool isPreviewablePath(const QString &path, bool isDirectory);
    static qint64 parseTimestamp(const QString &value);
    static QVector<RecentRecord> loadFinder(const QString &path);
    static QVector<RecentRecord> loadLaunchHistory(const QString &path, int limit);
    static QVector<RecentRecord> loadXbel(const QString &path);
};

} // namespace Astrea::Explorer::Native::Backend
