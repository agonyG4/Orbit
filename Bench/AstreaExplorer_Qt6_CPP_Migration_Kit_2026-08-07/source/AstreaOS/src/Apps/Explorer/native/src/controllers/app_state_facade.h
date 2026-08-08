#pragma once

#include <QAbstractItemModel>
#include <QStringList>

#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "services/settings_service.h"

namespace Astrea::Explorer::Native::Backend {

class AppStateFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *fileModel READ fileModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIdx READ historyIdx NOTIFY historyChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex NOTIFY activeTabIndexChanged)
    Q_PROPERTY(bool loadingDir READ loadingDir NOTIFY loadingDirChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)
    Q_PROPERTY(bool remoteDirectoryActive READ remoteDirectoryActive NOTIFY remoteDirectoryActiveChanged)
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QStringList selectedFiles READ selectedFiles NOTIFY selectedFilesChanged)
    Q_PROPERTY(int lastSelectedIndex READ lastSelectedIndex NOTIFY lastSelectedIndexChanged)
    Q_PROPERTY(int fileModelRevision READ fileModelRevision NOTIFY fileModelRevisionChanged)
    Q_PROPERTY(bool fileModelFilling READ fileModelFilling CONSTANT)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString sortField READ sortField WRITE setSortField NOTIFY sortFieldChanged)
    Q_PROPERTY(bool sortAsc READ sortAsc WRITE setSortAsc NOTIFY sortAscChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool foldersFirst READ foldersFirst WRITE setFoldersFirst NOTIFY foldersFirstChanged)
    Q_PROPERTY(bool groupingEnabled READ groupingEnabled WRITE setGroupingEnabled NOTIFY groupingEnabledChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(QString autoMountDeviceIdsJson READ autoMountDeviceIdsJson WRITE setAutoMountDeviceIdsJson NOTIFY autoMountDeviceIdsJsonChanged)
    Q_PROPERTY(QString sidebarFavoritesJson READ sidebarFavoritesJson WRITE setSidebarFavoritesJson NOTIFY sidebarFavoritesJsonChanged)
    Q_PROPERTY(QString sidebarHiddenDefaultFavoritesJson READ sidebarHiddenDefaultFavoritesJson WRITE setSidebarHiddenDefaultFavoritesJson NOTIFY sidebarHiddenDefaultFavoritesJsonChanged)

public:
    AppStateFacade(
        NavigationController *navigation,
        SelectionController *selection,
        DirectoryModel *model,
        QObject *parent = nullptr,
        Services::SettingsService *settingsService = nullptr);

    QAbstractItemModel *fileModel() const;
    QString currentPath() const;
    QStringList history() const;
    int historyIdx() const;
    int activeTabIndex() const;
    bool loadingDir() const;
    QString loadError() const;
    bool remoteDirectoryActive() const;
    bool searchActive() const;
    QString searchQuery() const;
    QString selectedFile() const;
    QStringList selectedFiles() const;
    int lastSelectedIndex() const;
    int fileModelRevision() const;
    bool fileModelFilling() const;
    bool showPreview() const;
    QString viewMode() const;
    QString sortField() const;
    bool sortAsc() const;
    bool showHidden() const;
    bool foldersFirst() const;
    bool groupingEnabled() const;
    double zoomLevel() const;
    QString autoMountDeviceIdsJson() const;
    QString sidebarFavoritesJson() const;
    QString sidebarHiddenDefaultFavoritesJson() const;

    void setShowPreview(bool showPreview);
    void setViewMode(const QString &viewMode);
    void setSortField(const QString &sortField);
    void setSortAsc(bool sortAscending);
    void setShowHidden(bool showHidden);
    void setFoldersFirst(bool foldersFirst);
    void setGroupingEnabled(bool groupingEnabled);
    void setZoomLevel(double zoomLevel);
    void setAutoMountDeviceIdsJson(const QString &json);
    void setSidebarFavoritesJson(const QString &json);
    void setSidebarHiddenDefaultFavoritesJson(const QString &json);

    Q_INVOKABLE BackendRequestId navigateTo(const QString &path);
    Q_INVOKABLE BackendRequestId submitSearch(const QString &root, const QString &query);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void createTab(const QString &path = QString());
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE BackendRequestId refreshCurrentFolder();

    Q_INVOKABLE bool isSelected(const QString &name) const;
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectByName(const QString &name);
    Q_INVOKABLE void handleSelection(
        const QString &name,
        int index,
        bool ctrlMode,
        bool shiftMode,
        bool preserveCurrentSelection);

signals:
    void currentPathChanged();
    void historyChanged();
    void activeTabIndexChanged();
    void loadingDirChanged();
    void loadErrorChanged();
    void remoteDirectoryActiveChanged();
    void searchStateChanged();
    void selectedFileChanged();
    void selectedFilesChanged();
    void lastSelectedIndexChanged();
    void fileModelRevisionChanged();
    void showPreviewChanged();
    void viewModeChanged();
    void sortFieldChanged();
    void sortAscChanged();
    void showHiddenChanged();
    void foldersFirstChanged();
    void groupingEnabledChanged();
    void zoomLevelChanged();
    void autoMountDeviceIdsJsonChanged();
    void sidebarFavoritesJsonChanged();
    void sidebarHiddenDefaultFavoritesJsonChanged();

private slots:
    void handleModelReset();
    void handleListingOptionsChanged();
    void persistCurrentPath();

private:
    void persistSettings();

    NavigationController *m_navigation = nullptr;
    SelectionController *m_selection = nullptr;
    DirectoryModel *m_model = nullptr;
    Services::SettingsService *m_settingsService = nullptr;
    Services::ExplorerSettings m_settings;
    int m_fileModelRevision = 0;
};

} // namespace Astrea::Explorer::Native::Backend
