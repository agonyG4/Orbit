#pragma once

#include <QObject>

#include "services/settings_service.h"

namespace Astrea::Explorer::Native::Backend {

class DeviceController;
class NavigationController;

class ExplorerSettingsController final : public QObject
{
    Q_OBJECT

public:
    explicit ExplorerSettingsController(
        Services::SettingsService *settingsService,
        QObject *parent = nullptr);

    const Services::ExplorerSettings &settings() const;
    bool showPreview() const;
    QString viewMode() const;
    QString sortField() const;
    bool sortAscending() const;
    bool showHidden() const;
    bool foldersFirst() const;
    bool groupingEnabled() const;
    double zoomLevel() const;
    QString currentPath() const;
    QString autoMountDeviceIdsJson() const;
    QString sidebarFavoritesJson() const;
    QString sidebarHiddenDefaultFavoritesJson() const;

    void bindNavigation(NavigationController *navigation);
    void bindDeviceController(DeviceController *devices);

    void setShowPreview(bool value);
    void setViewMode(const QString &value);
    void setSortField(const QString &value);
    void setSortAscending(bool value);
    void setShowHidden(bool value);
    void setFoldersFirst(bool value);
    void setGroupingEnabled(bool value);
    void setZoomLevel(double value);
    void setAutoMountDeviceIdsJson(const QString &json);
    void setSidebarFavoritesJson(const QString &json);
    void setSidebarHiddenDefaultFavoritesJson(const QString &json);

signals:
    void showPreviewChanged();
    void viewModeChanged();
    void sortFieldChanged();
    void sortAscendingChanged();
    void showHiddenChanged();
    void foldersFirstChanged();
    void groupingEnabledChanged();
    void zoomLevelChanged();
    void currentPathChanged();
    void autoMountDeviceIdsJsonChanged();
    void sidebarFavoritesJsonChanged();
    void sidebarHiddenDefaultFavoritesJsonChanged();

private slots:
    void handleListingOptionsChanged();
    void handleCurrentPathChanged();
    void handleAutoMountChanged();

private:
    void persist();

    Services::SettingsService *m_settingsService = nullptr;
    Services::ExplorerSettings m_settings;
    NavigationController *m_navigation = nullptr;
    DeviceController *m_devices = nullptr;
};

} // namespace Astrea::Explorer::Native::Backend
