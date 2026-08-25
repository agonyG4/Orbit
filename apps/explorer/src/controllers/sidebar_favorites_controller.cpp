#include "controllers/sidebar_favorites_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

#include "controllers/explorer_settings_controller.h"
#include "models/sidebar_favorites_model.h"

namespace Astrea::Explorer::Native::Backend {

namespace {

QString xdgUserDirectory(const QString &key, const QString &fallback, const QString &home)
{
    const QString configPath = QDir(home).filePath(QStringLiteral(".config/user-dirs.dirs"));
    QFile config(configPath);
    if (!config.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QDir(home).filePath(fallback);
    }
    const QByteArray contents = config.readAll();
    const QRegularExpression expression(
        QStringLiteral("^%1=\\\"([^\\\"]+)\\\"$").arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = expression.match(QString::fromUtf8(contents));
    if (!match.hasMatch()) {
        return QDir(home).filePath(fallback);
    }
    QString value = match.captured(1);
    value.replace(QStringLiteral("$HOME"), home);
    return QDir::cleanPath(value);
}

} // namespace

SidebarFavoritesController::SidebarFavoritesController(
    ExplorerSettingsController *settings,
    QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_model(new SidebarFavoritesModel(this))
{
    Q_ASSERT(m_settings != nullptr);
    connect(
        m_settings,
        &ExplorerSettingsController::sidebarFavoritesJsonChanged,
        this,
        &SidebarFavoritesController::syncFromSettings);
    connect(
        m_settings,
        &ExplorerSettingsController::sidebarHiddenDefaultFavoritesJsonChanged,
        this,
        &SidebarFavoritesController::syncFromSettings);
    connect(
        m_settings,
        &ExplorerSettingsController::sidebarFavoritesJsonChanged,
        this,
        &SidebarFavoritesController::favoritesJsonChanged);
    connect(
        m_settings,
        &ExplorerSettingsController::sidebarHiddenDefaultFavoritesJsonChanged,
        this,
        &SidebarFavoritesController::hiddenDefaultFavoritesJsonChanged);
    syncModel(false);
}

QAbstractItemModel *SidebarFavoritesController::model() const
{
    return m_model;
}

QVariantList SidebarFavoritesController::favorites() const
{
    QVariantList persisted;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_settings->sidebarFavoritesJson().toUtf8());
    if (document.isArray()) {
        for (const QJsonValue &value : document.array()) {
            QVariantMap item;
            if (value.isObject()) {
                item = value.toObject().toVariantMap();
            } else if (value.isString()) {
                item.insert(QStringLiteral("path"), value.toString());
            }
            const QString path = item.value(QStringLiteral("path")).toString();
            if (path.isEmpty()) {
                continue;
            }
            item.insert(QStringLiteral("path"), normalizePath(path));
            if (item.value(QStringLiteral("label")).toString().isEmpty()) {
                item.insert(
                    QStringLiteral("label"),
                    QFileInfo(item.value(QStringLiteral("path")).toString()).fileName());
            }
            if (item.value(QStringLiteral("icon")).toString().isEmpty()) {
                item.insert(QStringLiteral("icon"), QStringLiteral("inode-directory"));
            }
            persisted.append(item);
        }
    }

    const QVariantList hidden = hiddenDefaultFavorites();
    const QStringList defaults = defaultPaths();
    QVariantList result;
    QStringList seen;
    for (const QVariant &value : persisted) {
        QVariantMap item = value.toMap();
        const QString path = normalizePath(item.value(QStringLiteral("path")).toString());
        if (path.isEmpty() || seen.contains(path)) {
            continue;
        }
        if (defaults.contains(path)) {
            if (std::any_of(hidden.cbegin(), hidden.cend(), [&path](const QVariant &hiddenPath) {
                    return normalizePath(hiddenPath.toString()) == path;
                })) {
                continue;
            }
            item = defaultItem(path);
        }
        seen.append(path);
        result.append(item);
    }
    for (const QString &path : defaults) {
        if (seen.contains(path)
            || std::any_of(hidden.cbegin(), hidden.cend(), [&path](const QVariant &hiddenPath) {
                   return normalizePath(hiddenPath.toString()) == path;
               })) {
            continue;
        }
        result.append(defaultItem(path));
        seen.append(path);
    }
    return result;
}

QVariantList SidebarFavoritesController::hiddenDefaultFavorites() const
{
    QVariantList result;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_settings->sidebarHiddenDefaultFavoritesJson().toUtf8());
    if (!document.isArray()) {
        return result;
    }
    for (const QJsonValue &value : document.array()) {
        if (value.isString()) {
            result.append(normalizePath(value.toString()));
        }
    }
    return result;
}

int SidebarFavoritesController::revision() const
{
    return m_revision;
}

QStringList SidebarFavoritesController::defaultFavoritePaths() const
{
    return defaultPaths();
}

bool SidebarFavoritesController::canPin(const QString &path) const
{
    const QString normalized = normalizePath(path);
    return normalized.startsWith(QLatin1Char('/')) && !isTrashPath(normalized);
}

bool SidebarFavoritesController::isFavorite(const QString &path) const
{
    const QString normalized = normalizePath(path);
    for (const QVariant &value : favorites()) {
        if (normalizePath(value.toMap().value(QStringLiteral("path")).toString()) == normalized) {
            return true;
        }
    }
    const QVariantList hidden = hiddenDefaultFavorites();
    for (const QString &defaultPath : defaultPaths()) {
        if (defaultPath != normalized) {
            continue;
        }
        return std::none_of(
            hidden.cbegin(),
            hidden.cend(),
            [&normalized](const QVariant &value) {
                return normalizePath(value.toString()) == normalized;
            });
    }
    return false;
}

QVariantList SidebarFavoritesController::visibleDefaults(const QVariantList &items) const
{
    const QVariantList hidden = hiddenDefaultFavorites();
    QVariantList visible;
    for (const QVariant &value : items) {
        const QString path = normalizePath(value.toMap().value(QStringLiteral("path")).toString());
        const bool isHidden = std::any_of(
            hidden.cbegin(),
            hidden.cend(),
            [&path](const QVariant &hiddenPath) {
                return normalizePath(hiddenPath.toString()) == path;
            });
        if (!isHidden) {
            visible.append(value);
        }
    }
    return visible;
}

void SidebarFavoritesController::pin(const QString &path, const QString &label, const QString &icon)
{
    const QString normalized = normalizePath(path);
    if (!canPin(normalized)) {
        return;
    }
    if (defaultPaths().contains(normalized)) {
        QVariantList hidden;
        for (const QVariant &value : hiddenDefaultFavorites()) {
            if (normalizePath(value.toString()) != normalized) {
                hidden.append(value);
            }
        }
        m_settings->setSidebarHiddenDefaultFavoritesJson(
            QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                  .toJson(QJsonDocument::Compact)));
        return;
    }
    if (isFavorite(normalized)) {
        return;
    }

    QVariantList items = favorites();
    QVariantMap item;
    item.insert(QStringLiteral("label"), label.isEmpty() ? QFileInfo(normalized).fileName() : label);
    item.insert(QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("inode-directory") : icon);
    item.insert(QStringLiteral("path"), normalized);
    items.append(item);
    persistFavoriteItems(items);
}

void SidebarFavoritesController::remove(const QString &path)
{
    const QString normalized = normalizePath(path);
    if (defaultPaths().contains(normalized)) {
        QVariantList hidden = hiddenDefaultFavorites();
        if (std::none_of(
                hidden.cbegin(),
                hidden.cend(),
                [&normalized](const QVariant &value) {
                    return normalizePath(value.toString()) == normalized;
                })) {
            hidden.append(normalized);
            m_settings->setSidebarHiddenDefaultFavoritesJson(
                QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                      .toJson(QJsonDocument::Compact)));
        }
        return;
    }

    QVariantList items;
    for (const QVariant &value : favorites()) {
        if (normalizePath(value.toMap().value(QStringLiteral("path")).toString()) != normalized) {
            items.append(value);
        }
    }
    persistFavoriteItems(items);
}

void SidebarFavoritesController::move(const QString &path, int targetIndex)
{
    if (!beginDrag(path)) {
        return;
    }
    if (!previewMove(path, targetIndex)) {
        cancelDrag();
        return;
    }
    commitDrag();
}

bool SidebarFavoritesController::beginDrag(const QString &path)
{
    return m_model->beginDrag(path);
}

bool SidebarFavoritesController::previewMove(const QString &path, int finalIndex)
{
    return m_model->moveFavorite(path, finalIndex);
}

bool SidebarFavoritesController::commitDrag()
{
    if (!m_model->dragActive()) {
        return false;
    }
    const QVariantList original = favorites();
    const QVariantList committed = m_model->commitDrag();
    if (favoritePathOrder(original) == favoritePathOrder(committed)) {
        return false;
    }
    persistFavoriteItems(committed);
    return true;
}

void SidebarFavoritesController::cancelDrag()
{
    m_model->cancelDrag();
}

void SidebarFavoritesController::syncFromSettings()
{
    syncModel();
}


QString SidebarFavoritesController::normalizePath(const QString &path)
{
    if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))) {
        return path;
    }
    return QDir::cleanPath(path);
}

QStringList SidebarFavoritesController::favoritePathOrder(const QVariantList &items)
{
    QStringList paths;
    paths.reserve(items.size());
    for (const QVariant &value : items) {
        paths.append(normalizePath(value.toMap().value(QStringLiteral("path")).toString()));
    }
    return paths;
}

QStringList SidebarFavoritesController::defaultPaths() const
{
    const QString root = QDir::homePath();
    const QStringList candidates {
        xdgUserDirectory(QStringLiteral("XDG_DESKTOP_DIR"), QStringLiteral("Desktop"), root),
        xdgUserDirectory(QStringLiteral("XDG_DOCUMENTS_DIR"), QStringLiteral("Documents"), root),
        xdgUserDirectory(QStringLiteral("XDG_DOWNLOAD_DIR"), QStringLiteral("Downloads"), root),
        xdgUserDirectory(QStringLiteral("XDG_PICTURES_DIR"), QStringLiteral("Pictures"), root),
        xdgUserDirectory(QStringLiteral("XDG_MUSIC_DIR"), QStringLiteral("Music"), root),
        xdgUserDirectory(QStringLiteral("XDG_VIDEOS_DIR"), QStringLiteral("Videos"), root),
        xdgUserDirectory(QStringLiteral("XDG_PUBLICSHARE_DIR"), QStringLiteral("Public"), root),
        xdgUserDirectory(QStringLiteral("XDG_TEMPLATES_DIR"), QStringLiteral("Templates"), root),
    };
    QStringList result;
    for (const QString &path : candidates) {
        const QString normalized = normalizePath(path);
        if (!normalized.isEmpty() && !result.contains(normalized)) {
            result.append(normalized);
        }
    }
    return result;
}

QVariantMap SidebarFavoritesController::defaultItem(const QString &path) const
{
    const QStringList defaults = defaultPaths();
    const QStringList labels {
        QStringLiteral("Desktop"), QStringLiteral("Documents"), QStringLiteral("Downloads"),
        QStringLiteral("Pictures"), QStringLiteral("Music"), QStringLiteral("Videos"),
        QStringLiteral("Public"), QStringLiteral("Templates"),
    };
    const QStringList icons {
        QStringLiteral("user-desktop"), QStringLiteral("folder-documents"),
        QStringLiteral("folder-download"), QStringLiteral("folder-pictures"),
        QStringLiteral("folder-music"), QStringLiteral("folder-videos"),
        QStringLiteral("folder-publicshare"), QStringLiteral("folder-templates"),
    };
    const int index = defaults.indexOf(path);
    QVariantMap item;
    item.insert(QStringLiteral("path"), path);
    item.insert(QStringLiteral("label"), index >= 0 ? labels.at(index) : QFileInfo(path).fileName());
    item.insert(QStringLiteral("icon"), index >= 0 ? icons.at(index) : QStringLiteral("inode-directory"));
    item.insert(
        QStringLiteral("id"),
        index >= 0 ? QStringLiteral("builtin:%1").arg(index)
                   : QStringLiteral("custom:%1").arg(path));
    item.insert(QStringLiteral("builtIn"), index >= 0);
    return item;
}

bool SidebarFavoritesController::isTrashPath(const QString &path) const
{
    if (path == QStringLiteral("trash://") || path == QStringLiteral("trash:///")) {
        return true;
    }
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty() || !dataHome.startsWith(QLatin1Char('/'))) {
        dataHome = QDir(QDir::homePath()).filePath(QStringLiteral(".local/share"));
    }
    return normalizePath(path)
        == normalizePath(QDir(dataHome).filePath(QStringLiteral("Trash/files")));
}

void SidebarFavoritesController::persistFavoriteItems(const QVariantList &items)
{
    m_settings->setSidebarFavoritesJson(
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(items))
                              .toJson(QJsonDocument::Compact)));
}

void SidebarFavoritesController::syncModel(bool publishChanges)
{
    m_model->setItems(favorites());
    if (!publishChanges) {
        return;
    }
    ++m_revision;
    emit favoritesChanged();
}

} // namespace Astrea::Explorer::Native::Backend
