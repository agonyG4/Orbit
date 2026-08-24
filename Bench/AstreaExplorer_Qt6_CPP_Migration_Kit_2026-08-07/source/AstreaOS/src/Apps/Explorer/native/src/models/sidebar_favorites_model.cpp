#include "models/sidebar_favorites_model.h"

#include <QDir>

namespace Astrea::Explorer::Native::Backend {

namespace {

QString normalizedPath(const QString &path)
{
    return path.startsWith(QLatin1Char('/')) ? QDir::cleanPath(path) : path;
}

} // namespace

SidebarFavoritesModel::SidebarFavoritesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SidebarFavoritesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant SidebarFavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.column() != 0 || index.row() < 0
        || index.row() >= m_items.size()) {
        return {};
    }

    const QVariantMap item = m_items.at(index.row()).toMap();
    switch (role) {
    case Qt::DisplayRole:
        return item;
    case PathRole:
        return item.value(QStringLiteral("path"));
    case LabelRole:
        return item.value(QStringLiteral("label"));
    case IconRole:
        return item.value(QStringLiteral("icon"));
    default:
        return {};
    }
}

QHash<int, QByteArray> SidebarFavoritesModel::roleNames() const
{
    return {
        {PathRole, QByteArrayLiteral("path")},
        {LabelRole, QByteArrayLiteral("label")},
        {IconRole, QByteArrayLiteral("icon")},
    };
}

QVariantList SidebarFavoritesModel::items() const
{
    return m_items;
}

void SidebarFavoritesModel::setItems(const QVariantList &items)
{
    if (m_items == items && !m_dragActive) {
        return;
    }
    beginResetModel();
    m_items = items;
    m_dragOriginalItems.clear();
    m_draggedPath.clear();
    endResetModel();
    setDragActive(false);
}

int SidebarFavoritesModel::indexOfPath(const QString &path) const
{
    const QString normalized = normalizedPath(path);
    for (int index = 0; index < m_items.size(); ++index) {
        if (normalizedPath(m_items.at(index).toMap().value(QStringLiteral("path")).toString())
            == normalized) {
            return index;
        }
    }
    return -1;
}

bool SidebarFavoritesModel::dragActive() const
{
    return m_dragActive;
}

bool SidebarFavoritesModel::beginDrag(const QString &path)
{
    if (m_dragActive || indexOfPath(path) < 0) {
        return false;
    }
    m_dragOriginalItems = m_items;
    m_draggedPath = normalizedPath(path);
    setDragActive(true);
    return true;
}

bool SidebarFavoritesModel::moveFavorite(const QString &path, int finalIndex)
{
    if (!m_dragActive || normalizedPath(path) != m_draggedPath) {
        return false;
    }
    const int sourceIndex = indexOfPath(path);
    if (sourceIndex < 0 || m_items.isEmpty()) {
        return false;
    }
    return moveRow(sourceIndex, qBound(0, finalIndex, m_items.size() - 1));
}

QVariantList SidebarFavoritesModel::commitDrag()
{
    const QVariantList result = m_items;
    m_dragOriginalItems.clear();
    m_draggedPath.clear();
    setDragActive(false);
    return result;
}

void SidebarFavoritesModel::cancelDrag()
{
    if (!m_dragActive) {
        return;
    }
    const QVariantList original = m_dragOriginalItems;
    setItems(original);
}

bool SidebarFavoritesModel::moveRow(int sourceIndex, int targetIndex)
{
    if (sourceIndex == targetIndex || sourceIndex < 0 || targetIndex < 0
        || sourceIndex >= m_items.size() || targetIndex >= m_items.size()) {
        return false;
    }

    const int destinationChild = targetIndex > sourceIndex ? targetIndex + 1 : targetIndex;
    if (!beginMoveRows(QModelIndex(), sourceIndex, sourceIndex, QModelIndex(), destinationChild)) {
        return false;
    }
    const QVariant item = m_items.takeAt(sourceIndex);
    m_items.insert(targetIndex, item);
    endMoveRows();
    return true;
}

void SidebarFavoritesModel::setDragActive(bool active)
{
    if (m_dragActive == active) {
        return;
    }
    m_dragActive = active;
    emit dragActiveChanged();
}

} // namespace Astrea::Explorer::Native::Backend
