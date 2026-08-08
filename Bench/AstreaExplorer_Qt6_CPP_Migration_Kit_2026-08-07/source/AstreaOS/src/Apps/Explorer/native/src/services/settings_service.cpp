#include "services/settings_service.h"

#include <utility>

#include <QSettings>

namespace Astrea::Explorer::Native::Services {

SettingsService::SettingsService(QString filePath)
    : m_filePath(std::move(filePath))
{
}

ExplorerSettings SettingsService::load() const
{
    ExplorerSettings defaults;
    QSettings settings(m_filePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Explorer"));

    ExplorerSettings result;
    result.currentPath = settings.value(QStringLiteral("currentPath"), defaults.currentPath)
                             .toString();
    result.showPreview = settings.value(QStringLiteral("showPreview"), defaults.showPreview)
                             .toBool();
    result.viewMode = settings.value(QStringLiteral("viewMode"), defaults.viewMode).toString();
    result.sortField = settings.value(QStringLiteral("sortField"), defaults.sortField).toString();
    result.sortAscending = settings.value(QStringLiteral("sortAsc"), defaults.sortAscending)
                               .toBool();
    result.showHidden = settings.value(QStringLiteral("showHidden"), defaults.showHidden)
                            .toBool();
    result.foldersFirst = settings.value(QStringLiteral("foldersFirst"), defaults.foldersFirst)
                              .toBool();
    result.groupingEnabled = settings
                                 .value(QStringLiteral("groupingEnabled"), defaults.groupingEnabled)
                                 .toBool();
    result.zoomLevel = settings.value(QStringLiteral("zoomLevel"), defaults.zoomLevel).toDouble();
    result.autoMountDeviceIdsJson = settings
                                        .value(
                                            QStringLiteral("autoMountDeviceIdsJson"),
                                            defaults.autoMountDeviceIdsJson)
                                        .toString();
    result.sidebarFavoritesJson = settings
                                      .value(
                                          QStringLiteral("sidebarFavoritesJson"),
                                          defaults.sidebarFavoritesJson)
                                      .toString();
    result.sidebarHiddenDefaultFavoritesJson = settings
                                                   .value(
                                                       QStringLiteral(
                                                           "sidebarHiddenDefaultFavoritesJson"),
                                                       defaults.sidebarHiddenDefaultFavoritesJson)
                                                   .toString();
    settings.endGroup();
    return result;
}

bool SettingsService::save(const ExplorerSettings &settingsToSave) const
{
    QSettings settings(m_filePath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Explorer"));
    settings.setValue(QStringLiteral("currentPath"), settingsToSave.currentPath);
    settings.setValue(QStringLiteral("showPreview"), settingsToSave.showPreview);
    settings.setValue(QStringLiteral("viewMode"), settingsToSave.viewMode);
    settings.setValue(QStringLiteral("sortField"), settingsToSave.sortField);
    settings.setValue(QStringLiteral("sortAsc"), settingsToSave.sortAscending);
    settings.setValue(QStringLiteral("showHidden"), settingsToSave.showHidden);
    settings.setValue(QStringLiteral("foldersFirst"), settingsToSave.foldersFirst);
    settings.setValue(QStringLiteral("groupingEnabled"), settingsToSave.groupingEnabled);
    settings.setValue(QStringLiteral("zoomLevel"), settingsToSave.zoomLevel);
    settings.setValue(
        QStringLiteral("autoMountDeviceIdsJson"),
        settingsToSave.autoMountDeviceIdsJson);
    settings.setValue(
        QStringLiteral("sidebarFavoritesJson"),
        settingsToSave.sidebarFavoritesJson);
    settings.setValue(
        QStringLiteral("sidebarHiddenDefaultFavoritesJson"),
        settingsToSave.sidebarHiddenDefaultFavoritesJson);
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString SettingsService::filePath() const
{
    return m_filePath;
}

} // namespace Astrea::Explorer::Native::Services
