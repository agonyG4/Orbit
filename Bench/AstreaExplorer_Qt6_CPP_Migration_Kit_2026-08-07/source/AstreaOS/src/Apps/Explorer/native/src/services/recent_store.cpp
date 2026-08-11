#include "services/recent_store.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QXmlStreamReader>

#include "controllers/open_with_controller.h"

namespace Astrea::Explorer::Native::Backend {

namespace {

constexpr qint64 kReverseChunkSize = 64 * 1024;
constexpr qsizetype kMaximumRecordBytes = 1024 * 1024;

qint64 objectInteger(const QJsonObject &object, const QString &key, qint64 fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) {
        return value.toInteger();
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : fallback;
    }
    return fallback;
}

bool objectBoolean(const QJsonObject &object, const QString &key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isUndefined() ? fallback : value.toBool(fallback);
}

qint64 parseTimestamp(const QString &value)
{
    if (value.isEmpty()) {
        return 0;
    }
    return QDateTime::fromString(value, Qt::ISODate).toMSecsSinceEpoch();
}

bool isPreviewablePath(const QString &path, bool isDirectory)
{
    if (isDirectory) {
        return false;
    }
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".jpg"))
        || lower.endsWith(QStringLiteral(".jpeg"))
        || lower.endsWith(QStringLiteral(".png"))
        || lower.endsWith(QStringLiteral(".gif"))
        || lower.endsWith(QStringLiteral(".bmp"))
        || lower.endsWith(QStringLiteral(".webp"))
        || lower.endsWith(QStringLiteral(".svg"))
        || lower.endsWith(QStringLiteral(".avif"))
        || lower.endsWith(QStringLiteral(".heic"))
        || lower.endsWith(QStringLiteral(".heif"))
        || lower.endsWith(QStringLiteral(".tiff"))
        || lower.endsWith(QStringLiteral(".tif"));
}

RecentRecord recordFromPath(
    const QString &path,
    qint64 lastAccessed,
    const QString &source,
    const QString &kind = QString())
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

RecentRecord recordFromObject(const QJsonObject &object, const QString &source)
{
    const QString path = object.value(QStringLiteral("filePath")).toString();
    RecentRecord record = recordFromPath(
        path,
        objectInteger(object, QStringLiteral("lastAccessed"), 0),
        source,
        object.value(QStringLiteral("fileKind")).toString());
    if (record.entry.filePath.isEmpty()) {
        if (path.trimmed().isEmpty()) {
            return record;
        }

        const QFileInfo info(path);
        record.entry.filePath = info.absoluteFilePath();
        record.entry.fileName = info.fileName().isEmpty() ? path : info.fileName();
        record.entry.fileUrl = QUrl::fromLocalFile(record.entry.filePath);
        record.entry.fileIsDir = objectBoolean(object, QStringLiteral("fileIsDir"), false);
        record.entry.fileExecutable = objectBoolean(
            object,
            QStringLiteral("fileExecutable"),
            false);
        record.entry.fileHidden = objectBoolean(
            object,
            QStringLiteral("fileHidden"),
            record.entry.fileName.startsWith(QLatin1Char('.')));
        record.entry.fileSize = objectInteger(object, QStringLiteral("fileSize"), -1);
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

RecentRecord recordFromDesktop(
    const QString &desktopId,
    const QJsonArray &argv,
    qint64 lastAccessed,
    const QString &source,
    const OpenWithController::DesktopCatalog *desktopCatalog)
{
    QString desktopPath;
    for (const QJsonValue &value : argv) {
        const QString argument = value.toString();
        if (argument.endsWith(QStringLiteral(".desktop")) && QFileInfo(argument).isFile()) {
            desktopPath = argument;
            break;
        }
    }

    const QString lookup = desktopPath.isEmpty() ? desktopId : desktopPath;
    const OpenWithApplication application =
        OpenWithController::resolveDesktopEntry(lookup, desktopCatalog);
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

template<typename Callback>
void scanFinderObjects(QFile &file, Callback callback)
{
    QByteArray object;
    bool inString = false;
    bool escaped = false;
    bool discarding = false;
    int depth = 0;
    const QByteArray payload = file.read(kReverseChunkSize);
    QByteArray chunk = payload;
    while (true) {
        for (const char byte : chunk) {
            if (depth == 0) {
                if (byte == '{') {
                    depth = 1;
                    object.clear();
                    object.append(byte);
                    discarding = false;
                }
                continue;
            }

            if (!discarding) {
                object.append(byte);
                if (object.size() > kMaximumRecordBytes) {
                    discarding = true;
                }
            }

            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (byte == '\\') {
                    escaped = true;
                } else if (byte == '"') {
                    inString = false;
                }
                continue;
            }
            if (byte == '"') {
                inString = true;
            } else if (byte == '{') {
                ++depth;
            } else if (byte == '}') {
                --depth;
                if (depth == 0) {
                    if (!discarding) {
                        callback(object);
                    }
                    object.clear();
                    inString = false;
                    escaped = false;
                    discarding = false;
                }
            }
        }
        if (file.atEnd()) {
            break;
        }
        chunk = file.read(kReverseChunkSize);
        if (chunk.isEmpty()) {
            break;
        }
    }
}

template<typename Callback>
bool scanLaunchLinesReverse(QFile &file, Callback callback)
{
    qint64 position = file.size();
    QByteArray carry;
    bool oversized = false;
    while (position > 0) {
        const qint64 start = qMax<qint64>(0, position - kReverseChunkSize);
        if (!file.seek(start)) {
            return false;
        }
        const QByteArray chunk = file.read(position - start);
        position = start;
        carry.prepend(chunk);

        while (true) {
            const qsizetype newline = carry.lastIndexOf('\n');
            if (newline < 0) {
                break;
            }
            const QByteArray line = carry.sliced(newline + 1).trimmed();
            carry.truncate(newline);
            if (!oversized) {
                if (callback(line)) {
                    return true;
                }
            }
            oversized = false;
        }
        if (carry.size() > kMaximumRecordBytes) {
            carry.clear();
            oversized = true;
        }
    }
    if (!carry.isEmpty() && !oversized) {
        return callback(carry.trimmed());
    }
    return false;
}

void retainCandidate(QVector<RecentRecord> *records, RecentRecord record, int limit)
{
    if (record.entry.filePath.isEmpty() || limit <= 0) {
        return;
    }
    records->append(std::move(record));
    std::stable_sort(records->begin(), records->end(), [](const RecentRecord &left, const RecentRecord &right) {
        return left.lastAccessed > right.lastAccessed;
    });
    if (records->size() > limit) {
        records->removeLast();
    }
}

QVector<RecentRecord> loadFinder(const QString &path, int limit)
{
    if (path.trimmed().isEmpty() || limit <= 0) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QVector<RecentRecord> records;
    scanFinderObjects(file, [&](const QByteArray &objectBytes) {
        const QJsonDocument document = QJsonDocument::fromJson(objectBytes);
        if (document.isObject()) {
            retainCandidate(
                &records,
                recordFromObject(document.object(), QStringLiteral("finder")),
                limit);
        }
    });
    return records;
}

QVector<RecentRecord> loadLaunchHistory(const QString &path, int limit)
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
    std::optional<OpenWithController::DesktopCatalog> desktopCatalog;
    scanLaunchLinesReverse(file, [&](const QByteArray &line) -> bool {
        if (seenPaths.size() >= limit || line.isEmpty()) {
            return seenPaths.size() >= limit;
        }
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject()) {
            return false;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("status")).toString() != QStringLiteral("ok")) {
            return false;
        }
        const QString kind = object.value(QStringLiteral("kind")).toString();
        if (kind != QStringLiteral("file") && kind != QStringLiteral("desktop")) {
            return false;
        }
        const qint64 timestamp = objectInteger(object, QStringLiteral("timestamp_ms"), 0);
        RecentRecord record;
        if (kind == QStringLiteral("desktop")) {
            if (!desktopCatalog.has_value()) {
                desktopCatalog = OpenWithController::buildDesktopCatalog();
            }
            record = recordFromDesktop(
                object.value(QStringLiteral("target")).toString(),
                object.value(QStringLiteral("argv")).toArray(),
                timestamp,
                QStringLiteral("launch"),
                &*desktopCatalog);
        } else {
            record = recordFromPath(
                object.value(QStringLiteral("target")).toString(),
                timestamp,
                QStringLiteral("launch"));
        }
        if (record.entry.filePath.isEmpty() || seenPaths.contains(record.entry.filePath)) {
            return false;
        }
        records.append(record);
        seenPaths.insert(record.entry.filePath);
        return seenPaths.size() >= limit;
    });
    return records;
}

QVector<RecentRecord> loadXbel(const QString &path, int limit)
{
    if (path.trimmed().isEmpty() || limit <= 0) {
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
        const QUrl url(reader.attributes().value(QStringLiteral("href")).toString());
        if (!url.isLocalFile()) {
            continue;
        }
        const qint64 timestamp = std::max({
            parseTimestamp(reader.attributes().value(QStringLiteral("visited")).toString()),
            parseTimestamp(reader.attributes().value(QStringLiteral("modified")).toString()),
            parseTimestamp(reader.attributes().value(QStringLiteral("added")).toString()),
        });
        retainCandidate(
            &records,
            recordFromPath(url.toLocalFile(), timestamp, QStringLiteral("xbel")),
            limit);
    }
    return records;
}

QJsonObject recordToObject(const RecentRecord &record)
{
    QJsonObject object {
        {QStringLiteral("fileName"), record.entry.fileName},
        {QStringLiteral("filePath"), record.entry.filePath},
        {QStringLiteral("fileUrl"), record.entry.fileUrl.toString()},
        {QStringLiteral("fileIsDir"), record.entry.fileIsDir},
        {QStringLiteral("fileExecutable"), record.entry.fileExecutable},
        {QStringLiteral("fileHidden"), record.entry.fileHidden},
        {QStringLiteral("fileSize"), record.entry.fileSize},
        {QStringLiteral("fileKind"), record.entry.fileKind},
        {QStringLiteral("filePreviewUrl"), record.entry.filePreviewUrl.toString()},
        {QStringLiteral("lastAccessed"), record.lastAccessed},
        {QStringLiteral("recentSource"), QStringLiteral("finder")},
        {QStringLiteral("fileIconName"), record.entry.fileIconName},
    };
    if (record.entry.fileModified.isValid()) {
        object.insert(
            QStringLiteral("fileModified"),
            record.entry.fileModified.toString(Qt::ISODateWithMs));
    }
    return object;
}

} // namespace

RecentStore::RecentStore(RecentSourcePaths paths, QObject *parent, Dispatch dispatch)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_dispatch(std::move(dispatch))
{
    qRegisterMetaType<RecentRecord>();
    qRegisterMetaType<QVector<RecentRecord>>();
    if (!m_dispatch) {
        m_dispatch = [](std::function<void()> job) {
            QThreadPool::globalInstance()->start([job = std::move(job)]() mutable {
                job();
            });
        };
    }
}

quint64 RecentStore::load()
{
    const quint64 requestId = ++m_nextLoadRequest;
    m_activeLoadRequest = requestId;
    m_localRecords.clear();
    const RecentSourcePaths paths = m_paths;
    const QPointer<RecentStore> target(this);
    m_dispatch([target, requestId, paths]() {
        if (!target) {
            return;
        }
        QVector<RecentRecord> records = loadSources(paths);
        const quintptr workerThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
        QMetaObject::invokeMethod(
            target.data(),
            [target, requestId, records = std::move(records), workerThreadId]() mutable {
                if (!target) {
                    return;
                }
                target->acceptLoadedRecords(requestId, std::move(records), workerThreadId);
            },
            Qt::QueuedConnection);
    });
    return requestId;
}

void RecentStore::cancelLoad(quint64 requestId)
{
    if (requestId == m_activeLoadRequest) {
        m_activeLoadRequest = 0;
        m_localRecords.clear();
    }
}

void RecentStore::recordAccess(const RecentRecord &record)
{
    if (record.entry.filePath.isEmpty()) {
        return;
    }
    RecentRecord normalized = record;
    normalized.source = QStringLiteral("finder");
    if (normalized.lastAccessed <= 0) {
        normalized.lastAccessed = QDateTime::currentMSecsSinceEpoch();
    }
    normalized.entry.fileModified = QDateTime::fromMSecsSinceEpoch(normalized.lastAccessed);

    if (m_activeLoadRequest != 0) {
        m_localRecords.insert(normalized.entry.filePath, normalized);
    }
    QVector<RecentRecord> next = m_records;
    next.append(normalized);
    m_records = mergeRecords(next, m_paths.limit);
    emit recordsChanged();
    scheduleSave();
}

QVector<RecentRecord> RecentStore::records() const
{
    return m_records;
}

int RecentStore::limit() const
{
    return m_paths.limit;
}

quint64 RecentStore::persistenceGeneration() const
{
    return m_persistenceGeneration;
}

QVector<RecentRecord> RecentStore::loadSources(const RecentSourcePaths &paths)
{
    QVector<RecentRecord> records = loadFinder(paths.finderPath, paths.limit);
    records += loadLaunchHistory(paths.launchHistoryPath, paths.limit);
    records += loadXbel(paths.xbelPath, paths.limit);
    return mergeRecords(records, paths.limit);
}

QVector<RecentRecord> RecentStore::mergeRecords(
    const QVector<RecentRecord> &records,
    int limit)
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
    if (unique.size() > limit) {
        unique.resize(limit);
    }
    return unique;
}

QVector<RecentRecord> RecentStore::persistedRecords(
    const QVector<RecentRecord> &records,
    int limit)
{
    QVector<RecentRecord> finderRecords;
    for (const RecentRecord &record : records) {
        if (record.source == QStringLiteral("finder")) {
            finderRecords.append(record);
        }
    }
    return mergeRecords(finderRecords, limit);
}

bool RecentStore::saveFinderRecords(
    const QString &path,
    const QVector<RecentRecord> &records,
    QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("cannot create Recent storage directory");
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }

    QJsonArray array;
    for (const RecentRecord &record : records) {
        array.append(recordToObject(record));
    }
    const QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error != nullptr) {
            *error = file.errorString().isEmpty()
                ? QStringLiteral("atomic Recent save failed")
                : file.errorString();
        }
        return false;
    }
    return true;
}

void RecentStore::scheduleSave()
{
    PendingSave save;
    save.generation = ++m_persistenceGeneration;
    save.records = persistedRecords(m_records, m_paths.limit);
    if (m_saveRunning) {
        m_pendingSave = std::move(save);
        return;
    }
    startSave(std::move(save));
}

void RecentStore::startSave(PendingSave save)
{
    m_saveRunning = true;
    const QString path = m_paths.finderPath;
    const QPointer<RecentStore> target(this);
    m_dispatch([target, path, save = std::move(save)]() mutable {
        if (!target) {
            return;
        }
        QString error;
        const bool success = saveFinderRecords(path, save.records, &error);
        const quintptr workerThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
        QMetaObject::invokeMethod(
            target.data(),
            [target, generation = save.generation, success, error, workerThreadId]() {
                if (target) {
                    target->finishSave(generation, success, error, workerThreadId);
                }
            },
            Qt::QueuedConnection);
    });
}

void RecentStore::acceptLoadedRecords(
    quint64 requestId,
    QVector<RecentRecord> records,
    quintptr workerThreadId)
{
    if (requestId != m_activeLoadRequest) {
        return;
    }
    m_activeLoadRequest = 0;
    for (auto it = m_localRecords.cbegin(); it != m_localRecords.cend(); ++it) {
        records.append(it.value());
    }
    m_localRecords.clear();
    m_records = mergeRecords(records, m_paths.limit);
    emit loadReady(requestId, m_records, workerThreadId);
    emit recordsChanged();
}

void RecentStore::finishSave(
    quint64 generation,
    bool success,
    const QString &message,
    quintptr workerThreadId)
{
    if (!m_saveRunning) {
        return;
    }
    m_saveRunning = false;
    emit saveFinished(generation, success, message, workerThreadId);
    if (m_pendingSave.has_value()) {
        PendingSave pending = std::move(*m_pendingSave);
        m_pendingSave.reset();
        startSave(std::move(pending));
    }
}

} // namespace Astrea::Explorer::Native::Backend
