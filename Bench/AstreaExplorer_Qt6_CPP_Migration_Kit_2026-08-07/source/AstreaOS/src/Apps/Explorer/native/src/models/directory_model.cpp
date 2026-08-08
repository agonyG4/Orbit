#include "models/directory_model.h"

#include <utility>

namespace Astrea::Explorer::Native::Backend {

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
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
    };
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
