#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Backend {

class DirectoryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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
        FileIconNameRole,
    };
    Q_ENUM(Role)

    explicit DirectoryModel(QObject *parent = nullptr);

    int count() const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;
    bool entryForPath(const QString &filePath, DirectoryEntry *entry) const;

    void setEntries(QVector<DirectoryEntry> entries, quint64 generation);
    bool applyEntries(QVector<DirectoryEntry> entries, quint64 generation);
    bool updatePreview(
        const QString &filePath,
        const QUrl &previewUrl,
        quint64 generation);
    int updateMetadata(const QVariantList &items, quint64 generation);
    int removePaths(const QStringList &paths, quint64 generation);
    static QVector<DirectoryEntry> entriesFromVariantList(const QVariantList &items);
    QVector<QString> paths() const;

    quint64 generation() const;

signals:
    void countChanged();

private:
    QVector<DirectoryEntry> m_entries;
    quint64 m_generation = 0;
};

} // namespace Astrea::Explorer::Native::Backend
