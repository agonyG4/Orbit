#include "controllers/recent_controller.h"
#include "controllers/open_with_controller.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>

namespace Astrea::Explorer::Native::Backend {

namespace {

qint64 objectInteger(const QJsonObject &object, const QString &key, qint64 fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toInteger();
    }
    return value.toString().toLongLong();
}

bool objectBoolean(const QJsonObject &object, const QString &key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isUndefined() ? fallback : value.toBool(fallback);
}

} // namespace

QVector<DirectoryEntry> RecentController::load(const RecentSourcePaths &paths) const
{
    QVector<RecentRecord> records = loadFinder(paths.finderPath);
    records += loadLaunchHistory(paths.launchHistoryPath, paths.limit);
    records += loadXbel(paths.xbelPath);
    return merge(records, paths.limit);
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
            // `fileModified` is the existing Recent UI contract. It is an
            // access timestamp here, not the filesystem mtime.
            entry.fileModified = QDateTime::fromMSecsSinceEpoch(entry.lastAccessed);
        }
        entry.recentSource = unique.at(i).source;
        result.append(std::move(entry));
    }
    return result;
}

RecentRecord RecentController::recordFromPath(
    const QString &path,
    qint64 lastAccessed,
    const QString &source,
    const QString &kind)
{
    RecentRecord record;
    const QFileInfo info(path);
    if (!info.exists()) {
        return record;
    }

    const QString cleanPath = info.absoluteFilePath();
    const bool isDirectory = info.isDir();
    record.entry.fileName = info.fileName().isEmpty() ? cleanPath : info.fileName();
    record.entry.filePath = cleanPath;
    record.entry.fileUrl = QUrl::fromLocalFile(cleanPath);
    record.entry.fileIsDir = isDirectory;
    record.entry.fileExecutable = !isDirectory
        && (info.permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther));
    record.entry.fileHidden = record.entry.fileName.startsWith(QLatin1Char('.'));
    record.entry.fileSize = isDirectory ? 0 : info.size();
    record.entry.fileKind = kind.isEmpty()
        ? (isDirectory
               ? QStringLiteral("Pasta")
               : (info.suffix().isEmpty() ? QStringLiteral("Arquivo") : info.suffix().toUpper()))
        : kind;
    if (isPreviewablePath(cleanPath, isDirectory)) {
        record.entry.filePreviewUrl = record.entry.fileUrl;
    }
    record.lastAccessed = lastAccessed > 0
        ? lastAccessed
        : info.lastModified().toMSecsSinceEpoch();
    record.entry.fileModified = QDateTime::fromMSecsSinceEpoch(record.lastAccessed);
    record.source = source;
    return record;
}

RecentRecord RecentController::recordFromObject(
    const QJsonObject &object,
    const QString &source)
{
    const QString path = object.value(QStringLiteral("filePath")).toString();
    RecentRecord record = recordFromPath(
        path,
        objectInteger(object, QStringLiteral("lastAccessed"), 0),
        source,
        object.value(QStringLiteral("fileKind")).toString());
    if (record.entry.filePath.isEmpty()) {
        const QFileInfo info(path);
        if (path.trimmed().isEmpty()) {
            return record;
        }

        // Finder's legacy loader preserves serialized entries even after the
        // target disappears. Keep the identity and serialized metadata while
        // still rejecting records with no usable path.
        record.entry.filePath = info.absoluteFilePath();
        record.entry.fileName = info.fileName().isEmpty() ? path : info.fileName();
        record.entry.fileUrl = QUrl::fromLocalFile(record.entry.filePath);
        record.entry.fileIsDir = objectBoolean(
            object,
            QStringLiteral("fileIsDir"),
            false);
        record.entry.fileExecutable = objectBoolean(
            object,
            QStringLiteral("fileExecutable"),
            false);
        record.entry.fileHidden = objectBoolean(
            object,
            QStringLiteral("fileHidden"),
            record.entry.fileName.startsWith(QLatin1Char('.')));
        record.entry.fileSize = objectInteger(
            object,
            QStringLiteral("fileSize"),
            -1);
        record.entry.fileKind = object.value(QStringLiteral("fileKind")).toString();
        record.lastAccessed = objectInteger(object, QStringLiteral("lastAccessed"), 0);
        record.entry.fileModified = record.lastAccessed > 0
            ? QDateTime::fromMSecsSinceEpoch(record.lastAccessed)
            : QDateTime();
        record.source = source;
    }

    record.entry.fileName = object.value(QStringLiteral("fileName"))
                                .toString(record.entry.fileName);
    record.entry.fileUrl = QUrl(object.value(QStringLiteral("fileUrl"))
                                    .toString(record.entry.fileUrl.toString()));
    record.entry.fileExecutable = objectBoolean(
        object,
        QStringLiteral("fileExecutable"),
        record.entry.fileExecutable);
    record.entry.fileHidden = objectBoolean(
        object,
        QStringLiteral("fileHidden"),
        record.entry.fileHidden);
    if (object.contains(QStringLiteral("fileSize"))) {
        record.entry.fileSize = objectInteger(
            object,
            QStringLiteral("fileSize"),
            record.entry.fileSize);
    }
    const QString preview = object.value(QStringLiteral("filePreviewUrl")).toString();
    if (!preview.isEmpty()) {
        record.entry.filePreviewUrl = QUrl(preview);
    }
    record.entry.fileIconName = object.value(QStringLiteral("fileIconName")).toString();
    record.source = object.value(QStringLiteral("recentSource")).toString(source);
    return record;
}

RecentRecord RecentController::recordFromDesktop(
    const QString &desktopId,
    const QJsonArray &argv,
    qint64 lastAccessed,
    const QString &source,
    QHash<QString, QString> *desktopPathCache)
{
    QString desktopPath;
    for (const QJsonValue &value : argv) {
        const QString argument = value.toString();
        if (!argument.endsWith(QStringLiteral(".desktop"))) {
            continue;
        }
        if (QFileInfo(argument).isFile()) {
            desktopPath = argument;
            break;
        }
    }

    const QString lookup = desktopPath.isEmpty() ? desktopId : desktopPath;
    OpenWithApplication application;
    if (desktopPathCache != nullptr && desktopPathCache->contains(lookup)) {
        application = OpenWithController::resolveDesktopEntry(desktopPathCache->value(lookup));
    } else {
        application = OpenWithController::resolveDesktopEntry(lookup);
        if (desktopPathCache != nullptr && !application.desktopFile.isEmpty()) {
            desktopPathCache->insert(lookup, application.desktopFile);
        }
    }
    if (application.desktopFile.isEmpty()) {
        return {};
    }

    RecentRecord record = recordFromPath(
        application.desktopFile,
        lastAccessed,
        source,
        QStringLiteral("Aplicativo"));
    if (record.entry.filePath.isEmpty()) {
        return {};
    }
    record.entry.fileName = application.name;
    record.entry.fileExecutable = true;
    record.entry.filePreviewUrl = QUrl();
    record.entry.fileIconName = application.icon;
    return record;
}

bool RecentController::isPreviewablePath(const QString &path, bool isDirectory)
{
    if (isDirectory) {
        return false;
    }
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"))
        || lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".gif"))
        || lower.endsWith(QStringLiteral(".bmp")) || lower.endsWith(QStringLiteral(".webp"))
        || lower.endsWith(QStringLiteral(".svg")) || lower.endsWith(QStringLiteral(".avif"))
        || lower.endsWith(QStringLiteral(".heic")) || lower.endsWith(QStringLiteral(".heif"))
        || lower.endsWith(QStringLiteral(".tiff")) || lower.endsWith(QStringLiteral(".tif"));
}

qint64 RecentController::parseTimestamp(const QString &value)
{
    if (value.isEmpty()) {
        return 0;
    }
    return QDateTime::fromString(value, Qt::ISODate).toMSecsSinceEpoch();
}

QVector<RecentRecord> RecentController::loadFinder(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }

    QVector<RecentRecord> records;
    for (const QJsonValue &value : document.array()) {
        if (value.isObject()) {
            records.append(recordFromObject(value.toObject(), QStringLiteral("finder")));
        }
    }
    return records;
}

QVector<RecentRecord> RecentController::loadLaunchHistory(const QString &path, int limit)
{
    if (path.trimmed().isEmpty() || limit <= 0) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QVector<RecentRecord> records;
    QSet<QString> seenPaths;
    QHash<QString, QString> desktopPathCache;
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QByteArray line = it->trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("status")).toString() != QStringLiteral("ok")) {
            continue;
        }
        const QString kind = object.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("file") && kind != QStringLiteral("desktop")) {
            continue;
        }
        const qint64 timestamp = objectInteger(object, QStringLiteral("timestamp_ms"), 0);
        const RecentRecord record = kind == QStringLiteral("desktop")
            ? recordFromDesktop(
                  object.value(QStringLiteral("target")).toString(),
                  object.value(QStringLiteral("argv")).toArray(),
                  timestamp,
                  QStringLiteral("launch"),
                  &desktopPathCache)
            : recordFromPath(
                  object.value(QStringLiteral("target")).toString(),
                  timestamp,
                  QStringLiteral("launch"));
        if (record.entry.filePath.isEmpty() || seenPaths.contains(record.entry.filePath)) {
            continue;
        }
        records.append(record);
        seenPaths.insert(record.entry.filePath);
        if (seenPaths.size() >= limit) {
            break;
        }
    }
    return records;
}

QVector<RecentRecord> RecentController::loadXbel(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QXmlStreamReader reader(&file);
    QVector<RecentRecord> records;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QStringLiteral("bookmark")) {
            continue;
        }
        const QString href = reader.attributes().value(QStringLiteral("href")).toString();
        const QUrl url(href);
        if (!url.isLocalFile()) {
            continue;
        }
        const qint64 timestamp = std::max({
            parseTimestamp(reader.attributes().value(QStringLiteral("visited")).toString()),
            parseTimestamp(reader.attributes().value(QStringLiteral("modified")).toString()),
            parseTimestamp(reader.attributes().value(QStringLiteral("added")).toString()),
        });
        const RecentRecord record = recordFromPath(
            url.toLocalFile(),
            timestamp,
            QStringLiteral("xbel"));
        if (!record.entry.filePath.isEmpty()) {
            records.append(record);
        }
    }
    return records;
}

} // namespace Astrea::Explorer::Native::Backend
