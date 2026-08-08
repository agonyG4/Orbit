#include "controllers/recent_controller.h"

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
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

namespace Astrea::Explorer::Native::Backend {

namespace {

QString absolutePath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

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
    records += loadLaunchHistory(paths.launchHistoryPath);
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
    record.entry.fileModified = info.lastModified();
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
        return record;
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
    const qint64 modified = objectInteger(
        object,
        QStringLiteral("fileModified"),
        record.entry.fileModified.toMSecsSinceEpoch());
    if (modified > 0) {
        record.entry.fileModified = QDateTime::fromMSecsSinceEpoch(modified);
    }
    const QString preview = object.value(QStringLiteral("filePreviewUrl")).toString();
    if (!preview.isEmpty()) {
        record.entry.filePreviewUrl = QUrl(preview);
    }
    record.source = object.value(QStringLiteral("recentSource")).toString(source);
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

QVector<RecentRecord> RecentController::loadLaunchHistory(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QVector<RecentRecord> records;
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
        if (object.value(QStringLiteral("kind")).toString() != QStringLiteral("file")) {
            continue;
        }
        const qint64 timestamp = objectInteger(object, QStringLiteral("timestamp_ms"), 0);
        const RecentRecord record = recordFromPath(
            object.value(QStringLiteral("target")).toString(),
            timestamp,
            QStringLiteral("launch"));
        if (!record.entry.filePath.isEmpty()) {
            records.append(record);
        }
    }
    return records;
}

QVector<RecentRecord> RecentController::loadXbel(const QString &path)
{
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
