#pragma once

#include <QAbstractListModel>
#include <QVariantList>

namespace Astrea::Explorer::Native::Backend {

class SidebarFavoritesModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool dragActive READ dragActive NOTIFY dragActiveChanged)

public:
    enum Role
    {
        PathRole = Qt::UserRole + 1,
        LabelRole,
        IconRole,
    };
    Q_ENUM(Role)

    explicit SidebarFavoritesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantList items() const;
    void setItems(const QVariantList &items);
    int indexOfPath(const QString &path) const;

    bool dragActive() const;
    bool beginDrag(const QString &path);
    bool moveFavorite(const QString &path, int finalIndex);
    QVariantList commitDrag();
    void cancelDrag();

signals:
    void dragActiveChanged();

private:
    bool moveRow(int sourceIndex, int targetIndex);
    void setDragActive(bool active);

    QVariantList m_items;
    QVariantList m_dragOriginalItems;
    QString m_draggedPath;
    bool m_dragActive = false;
};

} // namespace Astrea::Explorer::Native::Backend
