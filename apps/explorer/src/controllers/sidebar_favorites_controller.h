#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QVector>

namespace Astrea::Explorer::Native::Backend {

class ExplorerSettingsController;
class SidebarFavoritesModel;

class SidebarFavoritesController final : public QObject
{
    Q_OBJECT

public:
    explicit SidebarFavoritesController(
        ExplorerSettingsController *settings,
        QObject *parent = nullptr);

    QAbstractItemModel *model() const;
    QVariantList favorites() const;
    QVariantList hiddenDefaultFavorites() const;
    int revision() const;
    QStringList defaultFavoritePaths() const;

    bool canPin(const QString &path) const;
    bool isFavorite(const QString &path) const;
    QVariantList visibleDefaults(const QVariantList &items) const;
    void pin(const QString &path, const QString &label, const QString &icon);
    void remove(const QString &path);
    void move(const QString &path, int targetIndex);
    bool beginDrag(const QString &path);
    bool previewMove(const QString &path, int finalIndex);
    bool commitDrag();
    void cancelDrag();

signals:
    void favoritesJsonChanged();
    void hiddenDefaultFavoritesJsonChanged();
    void favoritesChanged();

private slots:
    void syncFromSettings();

private:
    struct DefaultFavorite
    {
        QString id;
        QString path;
        QString label;
        QString icon;
    };

    static QString normalizePath(const QString &path);
    static QStringList favoritePathOrder(const QVariantList &items);
    QVector<DefaultFavorite> defaultFavorites() const;
    QStringList defaultPaths() const;
    QVariantMap defaultItem(const QString &path) const;
    bool isTrashPath(const QString &path) const;
    void persistFavoriteItems(const QVariantList &items);
    void syncModel(bool publishChanges = true);

    ExplorerSettingsController *m_settings = nullptr;
    SidebarFavoritesModel *m_model = nullptr;
    int m_revision = 0;
};

} // namespace Astrea::Explorer::Native::Backend
