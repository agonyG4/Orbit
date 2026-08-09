#include "models/directory_model.h"

#include <QDateTime>
#include <QSet>

#include <utility>

namespace Astrea::Explorer::Native::Backend {

namespace {

QUrl variantUrl(const QVariant &value)
{
    if (value.canConvert<QUrl>()) {
        const QUrl url = value.toUrl();
        if (url.isValid() && !url.isEmpty()) {
            return url;
        }
    }
    return QUrl(value.toString());
}

QDateTime variantDateTime(const QVariant &value)
{
    if (value.canConvert<QDateTime>()) {
        const QDateTime dateTime = value.toDateTime();
        if (dateTime.isValid()) {
            return dateTime;
        }
    }
    bool ok = false;
    const qint64 milliseconds = value.toLongLong(&ok);
    if (ok && milliseconds > 0) {
        return QDateTime::fromMSecsSinceEpoch(milliseconds);
    }
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

DirectoryEntry entryFromVariant(const QVariantMap &item)
{
    DirectoryEntry entry;
    entry.fileName = item.value(QStringLiteral("fileName")).toString();
    entry.filePath = item.value(QStringLiteral("filePath")).toString();
    entry.fileUrl = item.contains(QStringLiteral("fileUrl"))
        ? variantUrl(item.value(QStringLiteral("fileUrl")))
        : QUrl::fromLocalFile(entry.filePath);
    entry.fileIsDir = item.value(QStringLiteral("fileIsDir")).toBool();
    entry.fileExecutable = item.value(QStringLiteral("fileExecutable")).toBool();
    entry.fileHidden = item.value(QStringLiteral("fileHidden")).toBool();
    entry.fileSize = item.value(QStringLiteral("fileSize")).toLongLong();
    entry.fileModified = variantDateTime(item.value(QStringLiteral("fileModified")));
    entry.fileKind = item.value(QStringLiteral("fileKind")).toString();
    entry.filePreviewUrl = variantUrl(item.value(QStringLiteral("filePreviewUrl")));
    entry.fileRemote = item.value(QStringLiteral("fileRemote")).toBool();
    entry.fileMetadataLimited = item.value(QStringLiteral("fileMetadataLimited")).toBool();
    entry.fileFilesystem = item.value(QStringLiteral("fileFilesystem")).toString();
    entry.lastAccessed = item.value(QStringLiteral("lastAccessed")).toLongLong();
    entry.recentSource = item.value(QStringLiteral("recentSource")).toString();
    entry.fileIconName = item.value(QStringLiteral("fileIconName")).toString();
    return entry;
}

void assignMetadata(
    DirectoryEntry *entry,
    const QVariantMap &item,
    QVector<int> *roles)
{
    const auto assign = [roles](int role) { roles->append(role); };
    if (item.contains(QStringLiteral("fileName"))) {
        const QString value = item.value(QStringLiteral("fileName")).toString();
        if (entry->fileName != value) {
            entry->fileName = value;
            assign(DirectoryModel::FileNameRole);
        }
    }
    if (item.contains(QStringLiteral("fileUrl"))) {
        const QUrl value = variantUrl(item.value(QStringLiteral("fileUrl")));
        if (entry->fileUrl != value) {
            entry->fileUrl = value;
            assign(DirectoryModel::FileUrlRole);
        }
    }
    if (item.contains(QStringLiteral("fileIsDir"))) {
        const bool value = item.value(QStringLiteral("fileIsDir")).toBool();
        if (entry->fileIsDir != value) {
            entry->fileIsDir = value;
            assign(DirectoryModel::FileIsDirRole);
        }
    }
    if (item.contains(QStringLiteral("fileExecutable"))) {
        const bool value = item.value(QStringLiteral("fileExecutable")).toBool();
        if (entry->fileExecutable != value) {
            entry->fileExecutable = value;
            assign(DirectoryModel::FileExecutableRole);
        }
    }
    if (item.contains(QStringLiteral("fileHidden"))) {
        const bool value = item.value(QStringLiteral("fileHidden")).toBool();
        if (entry->fileHidden != value) {
            entry->fileHidden = value;
            assign(DirectoryModel::FileHiddenRole);
        }
    }
    if (item.contains(QStringLiteral("fileSize"))) {
        const qint64 value = item.value(QStringLiteral("fileSize")).toLongLong();
        if (entry->fileSize != value) {
            entry->fileSize = value;
            assign(DirectoryModel::FileSizeRole);
        }
    }
    if (item.contains(QStringLiteral("fileModified"))) {
        const QDateTime value = variantDateTime(item.value(QStringLiteral("fileModified")));
        if (entry->fileModified != value) {
            entry->fileModified = value;
            assign(DirectoryModel::FileModifiedRole);
        }
    }
    if (item.contains(QStringLiteral("fileKind"))) {
        const QString value = item.value(QStringLiteral("fileKind")).toString();
        if (entry->fileKind != value) {
            entry->fileKind = value;
            assign(DirectoryModel::FileKindRole);
        }
    }
    if (item.contains(QStringLiteral("filePreviewUrl"))) {
        const QUrl value = variantUrl(item.value(QStringLiteral("filePreviewUrl")));
        if (entry->filePreviewUrl != value) {
            entry->filePreviewUrl = value;
            assign(DirectoryModel::FilePreviewUrlRole);
        }
    }
    if (item.contains(QStringLiteral("fileRemote"))) {
        const bool value = item.value(QStringLiteral("fileRemote")).toBool();
        if (entry->fileRemote != value) {
            entry->fileRemote = value;
            assign(DirectoryModel::FileRemoteRole);
        }
    }
    if (item.contains(QStringLiteral("fileMetadataLimited"))) {
        const bool value = item.value(QStringLiteral("fileMetadataLimited")).toBool();
        if (entry->fileMetadataLimited != value) {
            entry->fileMetadataLimited = value;
            assign(DirectoryModel::FileMetadataLimitedRole);
        }
    }
    if (item.contains(QStringLiteral("fileFilesystem"))) {
        const QString value = item.value(QStringLiteral("fileFilesystem")).toString();
        if (entry->fileFilesystem != value) {
            entry->fileFilesystem = value;
            assign(DirectoryModel::FileFilesystemRole);
        }
    }
    if (item.contains(QStringLiteral("lastAccessed"))) {
        const qint64 value = item.value(QStringLiteral("lastAccessed")).toLongLong();
        if (entry->lastAccessed != value) {
            entry->lastAccessed = value;
            assign(DirectoryModel::LastAccessedRole);
        }
    }
    if (item.contains(QStringLiteral("recentSource"))) {
        const QString value = item.value(QStringLiteral("recentSource")).toString();
        if (entry->recentSource != value) {
            entry->recentSource = value;
            assign(DirectoryModel::RecentSourceRole);
        }
    }
    if (item.contains(QStringLiteral("fileIconName"))) {
        const QString value = item.value(QStringLiteral("fileIconName")).toString();
        if (entry->fileIconName != value) {
            entry->fileIconName = value;
            assign(DirectoryModel::FileIconNameRole);
        }
    }
}

} // namespace

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DirectoryModel::count() const
{
    return m_entries.size();
}

int DirectoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant DirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= m_entries.size()) {
        return {};
    }

    const DirectoryEntry &entry = m_entries.at(index.row());
    switch (role) {
    case FileNameRole:
        return entry.fileName;
    case FilePathRole:
        return entry.filePath;
    case FileUrlRole:
        return entry.fileUrl;
    case FileIsDirRole:
        return entry.fileIsDir;
    case FileExecutableRole:
        return entry.fileExecutable;
    case FileHiddenRole:
        return entry.fileHidden;
    case FileSizeRole:
        return entry.fileSize;
    case FileModifiedRole:
        return entry.fileModified;
    case FileKindRole:
        return entry.fileKind;
    case FilePreviewUrlRole:
        return entry.filePreviewUrl;
    case FileRemoteRole:
        return entry.fileRemote;
    case FileMetadataLimitedRole:
        return entry.fileMetadataLimited;
    case FileFilesystemRole:
        return entry.fileFilesystem;
    case LastAccessedRole:
        return entry.lastAccessed;
    case RecentSourceRole:
        return entry.recentSource;
    case FileIconNameRole:
        return entry.fileIconName;
    default:
        return {};
    }
}

QHash<int, QByteArray> DirectoryModel::roleNames() const
{
    return {
        {FileNameRole, QByteArrayLiteral("fileName")},
        {FilePathRole, QByteArrayLiteral("filePath")},
        {FileUrlRole, QByteArrayLiteral("fileUrl")},
        {FileIsDirRole, QByteArrayLiteral("fileIsDir")},
        {FileExecutableRole, QByteArrayLiteral("fileExecutable")},
        {FileHiddenRole, QByteArrayLiteral("fileHidden")},
        {FileSizeRole, QByteArrayLiteral("fileSize")},
        {FileModifiedRole, QByteArrayLiteral("fileModified")},
        {FileKindRole, QByteArrayLiteral("fileKind")},
        {FilePreviewUrlRole, QByteArrayLiteral("filePreviewUrl")},
        {FileRemoteRole, QByteArrayLiteral("fileRemote")},
        {FileMetadataLimitedRole, QByteArrayLiteral("fileMetadataLimited")},
        {FileFilesystemRole, QByteArrayLiteral("fileFilesystem")},
        {LastAccessedRole, QByteArrayLiteral("lastAccessed")},
        {RecentSourceRole, QByteArrayLiteral("recentSource")},
        {FileIconNameRole, QByteArrayLiteral("fileIconName")},
    };
}

QVariantMap DirectoryModel::get(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return {};
    }

    const QModelIndex modelIndex = index(row, 0);
    const QHash<int, QByteArray> roles = roleNames();
    QVariantMap result;
    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
        result.insert(
            QString::fromUtf8(role.value()),
            data(modelIndex, role.key()));
    }
    return result;
}

void DirectoryModel::setEntries(QVector<DirectoryEntry> entries, quint64 generation)
{
    applyEntries(std::move(entries), generation);
}

bool DirectoryModel::applyEntries(
    QVector<DirectoryEntry> entries,
    quint64 generation)
{
    if (generation < m_generation) {
        return false;
    }

    beginResetModel();
    m_entries = std::move(entries);
    m_generation = generation;
    endResetModel();
    emit countChanged();
    return true;
}

bool DirectoryModel::updatePreview(
    const QString &filePath,
    const QUrl &previewUrl,
    quint64 generation)
{
    if (generation != m_generation) {
        return false;
    }

    for (int row = 0; row < m_entries.size(); ++row) {
        DirectoryEntry &entry = m_entries[row];
        if (entry.filePath != filePath) {
            continue;
        }

        entry.filePreviewUrl = previewUrl;
        const QModelIndex modelIndex = index(row, 0);
        emit dataChanged(modelIndex, modelIndex, {FilePreviewUrlRole});
        return true;
    }

    return false;
}

int DirectoryModel::updateMetadata(const QVariantList &items, quint64 generation)
{
    if (generation != m_generation) {
        return 0;
    }

    int changedEntries = 0;
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        const QString filePath = item.value(QStringLiteral("filePath")).toString();
        if (filePath.isEmpty()) {
            continue;
        }

        for (int row = 0; row < m_entries.size(); ++row) {
            DirectoryEntry &entry = m_entries[row];
            if (entry.filePath != filePath) {
                continue;
            }
            QVector<int> roles;
            assignMetadata(&entry, item, &roles);
            if (!roles.isEmpty()) {
                emit dataChanged(index(row, 0), index(row, 0), roles);
                ++changedEntries;
            }
            break;
        }
    }
    return changedEntries;
}

int DirectoryModel::removePaths(const QStringList &paths, quint64 generation)
{
    if (generation != m_generation || paths.isEmpty()) {
        return 0;
    }

    const QSet<QString> removeSet(paths.cbegin(), paths.cend());
    int removed = 0;
    for (int row = m_entries.size() - 1; row >= 0; --row) {
        if (!removeSet.contains(m_entries.at(row).filePath)) {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        m_entries.removeAt(row);
        endRemoveRows();
        ++removed;
    }
    if (removed > 0) {
        emit countChanged();
    }
    return removed;
}

QVector<DirectoryEntry> DirectoryModel::entriesFromVariantList(const QVariantList &items)
{
    QVector<DirectoryEntry> entries;
    entries.reserve(items.size());
    for (const QVariant &value : items) {
        const DirectoryEntry entry = entryFromVariant(value.toMap());
        if (!entry.filePath.isEmpty()) {
            entries.append(entry);
        }
    }
    return entries;
}

QVector<QString> DirectoryModel::paths() const
{
    QVector<QString> result;
    result.reserve(m_entries.size());
    for (const DirectoryEntry &entry : m_entries) {
        result.append(entry.filePath);
    }
    return result;
}

quint64 DirectoryModel::generation() const
{
    return m_generation;
}

} // namespace Astrea::Explorer::Native::Backend
