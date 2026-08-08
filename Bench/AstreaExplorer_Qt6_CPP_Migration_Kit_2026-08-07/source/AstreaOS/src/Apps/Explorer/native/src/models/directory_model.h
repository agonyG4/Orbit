#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Backend {

class DirectoryModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole,
        FileUrlRole,
        FileIsDirRole,
        FileExecutableRole,
        FileHiddenRole,
        FileSizeRole,
        FileModifiedRole,
        FileKindRole,
        FilePreviewUrlRole,
        FileRemoteRole,
        FileMetadataLimitedRole,
        FileFilesystemRole,
        LastAccessedRole,
        RecentSourceRole,
    };
    Q_ENUM(Role)

    explicit DirectoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<DirectoryEntry> entries, quint64 generation);
    bool applyEntries(QVector<DirectoryEntry> entries, quint64 generation);
    bool updatePreview(
        const QString &filePath,
        const QUrl &previewUrl,
        quint64 generation);
    QVector<QString> paths() const;

    quint64 generation() const;

private:
    QVector<DirectoryEntry> m_entries;
    quint64 m_generation = 0;
};

} // namespace Astrea::Explorer::Native::Backend
