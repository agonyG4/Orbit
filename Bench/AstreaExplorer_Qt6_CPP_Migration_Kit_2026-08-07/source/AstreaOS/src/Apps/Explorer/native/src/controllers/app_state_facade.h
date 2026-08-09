#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVariantList>
#include <QStringList>

#include "controllers/device_controller.h"
#include "controllers/file_operations_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "runtime/explorer_runtime_paths.h"
#include "services/settings_service.h"

namespace Astrea::Explorer::Native::Backend {

class AppStateFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *fileModel READ fileModel CONSTANT)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(QString runtimeRoot READ runtimeRoot CONSTANT)
    Q_PROPERTY(QString backendPath READ backendPath CONSTANT)
    Q_PROPERTY(QString helperPath READ helperPath CONSTANT)
    Q_PROPERTY(QString wallpaperManagerPath READ wallpaperManagerPath CONSTANT)
    Q_PROPERTY(QString astreaLaunch READ astreaLaunch CONSTANT)
    Q_PROPERTY(QString windowsRun READ windowsRun CONSTANT)
    Q_PROPERTY(QString networkRootPath READ networkRootPath CONSTANT)
    Q_PROPERTY(QString trashFilesPath READ trashFilesPath CONSTANT)
    Q_PROPERTY(QString trashInfoPath READ trashInfoPath CONSTANT)
    Q_PROPERTY(QString recentVirtualPath READ recentVirtualPath CONSTANT)
    Q_PROPERTY(bool isPortalDialog READ isPortalDialog CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIdx READ historyIdx NOTIFY historyChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(QVariantList breadcrumbParts READ breadcrumbParts NOTIFY currentPathChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex NOTIFY activeTabIndexChanged)
    Q_PROPERTY(bool loadingDir READ loadingDir NOTIFY loadingDirChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)
    Q_PROPERTY(bool remoteDirectoryActive READ remoteDirectoryActive NOTIFY remoteDirectoryActiveChanged)
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchStateChanged)
    Q_PROPERTY(bool searchVisible READ searchVisible NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile WRITE setSelectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QStringList selectedFiles READ selectedFiles NOTIFY selectedFilesChanged)
    Q_PROPERTY(int lastSelectedIndex READ lastSelectedIndex NOTIFY lastSelectedIndexChanged)
    Q_PROPERTY(int fileModelRevision READ fileModelRevision NOTIFY fileModelRevisionChanged)
    Q_PROPERTY(bool fileModelFilling READ fileModelFilling NOTIFY loadingDirChanged)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)
    Q_PROPERTY(bool previewsEnabled READ previewsEnabled WRITE setPreviewsEnabled NOTIFY showPreviewChanged)
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
    Q_PROPERTY(QVariantList sidebarFavorites READ sidebarFavorites NOTIFY sidebarFavoritesChanged)
    Q_PROPERTY(QVariantList sidebarHiddenDefaultFavorites READ sidebarHiddenDefaultFavorites NOTIFY sidebarFavoritesChanged)
    Q_PROPERTY(int sidebarFavoritesRevision READ sidebarFavoritesRevision NOTIFY sidebarFavoritesChanged)
    Q_PROPERTY(QStringList defaultSidebarFavoritePaths READ defaultSidebarFavoritePaths CONSTANT)
    Q_PROPERTY(bool inTrashView READ inTrashView NOTIFY currentPathChanged)
    Q_PROPERTY(bool dialogActive READ dialogActive WRITE setDialogActive NOTIFY dialogStateChanged)
    Q_PROPERTY(QString dialogMode READ dialogMode WRITE setDialogMode NOTIFY dialogStateChanged)
    Q_PROPERTY(QStringList dialogFilePatterns READ dialogFilePatterns WRITE setDialogFilePatterns NOTIFY dialogStateChanged)
    Q_PROPERTY(QStringList clipboardFiles READ clipboardFiles NOTIFY clipboardStateChanged)
    Q_PROPERTY(QString clipboardMode READ clipboardMode NOTIFY clipboardStateChanged)
    Q_PROPERTY(bool fileOperationRunning READ fileOperationRunning NOTIFY fileOperationStateChanged)
    Q_PROPERTY(double fileOperationProgress READ fileOperationProgress NOTIFY fileOperationStateChanged)
    Q_PROPERTY(int fileOperationPercent READ fileOperationPercent NOTIFY fileOperationStateChanged)
    Q_PROPERTY(QString fileOperationFileName READ fileOperationFileName NOTIFY fileOperationStateChanged)
    Q_PROPERTY(QString fileOperationStatus READ fileOperationStatus NOTIFY fileOperationStateChanged)
    Q_PROPERTY(QString fileOperationError READ fileOperationError NOTIFY fileOperationStateChanged)
    Q_PROPERTY(QString fileOperationDestination READ fileOperationDestination NOTIFY fileOperationStateChanged)
    Q_PROPERTY(int fileOperationDoneCount READ fileOperationDoneCount NOTIFY fileOperationStateChanged)
    Q_PROPERTY(int fileOperationTotalCount READ fileOperationTotalCount NOTIFY fileOperationStateChanged)
    Q_PROPERTY(QString fileOperationMode READ fileOperationMode NOTIFY fileOperationStateChanged)
    Q_PROPERTY(bool pasteConflictVisible READ pasteConflictVisible NOTIFY pasteConflictStateChanged)
    Q_PROPERTY(QVariantList pasteConflictItems READ pasteConflictItems NOTIFY pasteConflictStateChanged)
    Q_PROPERTY(QString pendingPasteRename READ pendingPasteRename WRITE setPendingPasteRename NOTIFY pasteConflictStateChanged)
    Q_PROPERTY(QVariantList deviceModel READ deviceModel NOTIFY deviceStateChanged)
    Q_PROPERTY(QString deviceError READ deviceError NOTIFY deviceStateChanged)

public:
    AppStateFacade(
        NavigationController *navigation,
        SelectionController *selection,
        DirectoryModel *model,
        QObject *parent = nullptr,
        Services::SettingsService *settingsService = nullptr,
        FileOperationsController *fileOperations = nullptr,
        DeviceController *devices = nullptr,
        Runtime::ExplorerRuntimePaths runtimePaths = {});

    QAbstractItemModel *fileModel() const;
    QString homePath() const;
    QString runtimeRoot() const;
    QString backendPath() const;
    QString helperPath() const;
    QString wallpaperManagerPath() const;
    QString astreaLaunch() const;
    QString windowsRun() const;
    QString networkRootPath() const;
    QString trashFilesPath() const;
    QString trashInfoPath() const;
    QString recentVirtualPath() const;
    bool isPortalDialog() const;
    QString currentPath() const;
    QStringList history() const;
    int historyIdx() const;
    QVariantList tabs() const;
    QVariantList breadcrumbParts() const;
    int activeTabIndex() const;
    bool loadingDir() const;
    QString loadError() const;
    bool remoteDirectoryActive() const;
    bool searchActive() const;
    bool searchVisible() const;
    QString searchQuery() const;
    QString selectedFile() const;
    QStringList selectedFiles() const;
    int lastSelectedIndex() const;
    int fileModelRevision() const;
    bool fileModelFilling() const;
    bool showPreview() const;
    bool previewsEnabled() const;
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
    QVariantList sidebarFavorites() const;
    QVariantList sidebarHiddenDefaultFavorites() const;
    int sidebarFavoritesRevision() const;
    QStringList defaultSidebarFavoritePaths() const;
    bool inTrashView() const;
    bool dialogActive() const;
    QString dialogMode() const;
    QStringList dialogFilePatterns() const;
    QStringList clipboardFiles() const;
    QString clipboardMode() const;
    bool fileOperationRunning() const;
    double fileOperationProgress() const;
    int fileOperationPercent() const;
    QString fileOperationFileName() const;
    QString fileOperationStatus() const;
    QString fileOperationError() const;
    QString fileOperationDestination() const;
    int fileOperationDoneCount() const;
    int fileOperationTotalCount() const;
    QString fileOperationMode() const;
    bool pasteConflictVisible() const;
    QVariantList pasteConflictItems() const;
    QString pendingPasteRename() const;
    QVariantList deviceModel() const;
    QString deviceError() const;

    void setShowPreview(bool showPreview);
    void setPreviewsEnabled(bool enabled);
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
    void setDialogActive(bool active);
    void setDialogMode(const QString &mode);
    void setDialogFilePatterns(const QStringList &patterns);
    void setSearchQuery(const QString &query);
    void setSelectedFile(const QString &name);

    Q_INVOKABLE BackendRequestId navigateTo(const QString &path);
    Q_INVOKABLE BackendRequestId submitSearch(
        const QString &root,
        const QString &query = QString());
    Q_INVOKABLE void startSearch();
    Q_INVOKABLE void hideSearch();
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void createTab(const QString &path = QString());
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE void closeTabById(int tabId);
    Q_INVOKABLE void switchTabById(int tabId);
    Q_INVOKABLE int tabIndexById(int tabId) const;
    Q_INVOKABLE void moveTab(int fromIndex, int toIndex);
    Q_INVOKABLE BackendRequestId refreshCurrentFolder();
    Q_INVOKABLE void increaseZoom();
    Q_INVOKABLE void decreaseZoom();
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void setZoom(double level);

    Q_INVOKABLE QVariant selectedItem() const;
    Q_INVOKABLE QString fileUrlForPath(const QString &path) const;
    Q_INVOKABLE QString joinPath(
        const QString &directory,
        const QString &fileName) const;

    Q_INVOKABLE QStringList selectedPathsInCurrentFolder() const;
    Q_INVOKABLE QString selectedUriListInCurrentFolder() const;
    Q_INVOKABLE bool fileMatchesDialogFilter(
        const QString &fileName,
        bool isDirectory) const;
    Q_INVOKABLE void dropFilePaths(
        const QStringList &paths,
        const QString &destination,
        const QString &mode = QStringLiteral("copy"));
    Q_INVOKABLE void dropFiles(
        const QVariantList &urls,
        const QString &destination,
        const QString &mode = QStringLiteral("copy"));
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void cutSelected();
    Q_INVOKABLE BackendRequestId pasteFiles();
    Q_INVOKABLE bool isCutPending(const QString &name) const;
    Q_INVOKABLE void resolvePasteConflict(const QString &policy);
    Q_INVOKABLE void renamePasteConflict(const QString &name);
    Q_INVOKABLE void cancelPasteConflict();
    Q_INVOKABLE BackendRequestId requestMountDevice(
        const QString &devicePath,
        bool fromAutoMount = false,
        bool openAfterMount = false);
    Q_INVOKABLE BackendRequestId requestUnmountDevice(
        const QString &devicePath,
        const QString &mountPath);
    Q_INVOKABLE BackendRequestId requestRemountDevice(
        const QString &devicePath,
        const QString &mountPath,
        bool openAfterMount = false);
    Q_INVOKABLE void toggleDeviceAutoMount(const QString &deviceId, bool enabled);
    Q_INVOKABLE bool isRecentPath(const QString &path) const;
    Q_INVOKABLE bool isTrashPath(const QString &path) const;
    Q_INVOKABLE bool canPinSidebarFavorite(const QString &path) const;
    Q_INVOKABLE bool isSidebarFavorite(const QString &path) const;
    Q_INVOKABLE QVariantList visibleDefaultSidebarFavorites(
        const QVariantList &items) const;
    Q_INVOKABLE void pinSidebarFavorite(
        const QString &path,
        const QString &label = QString(),
        const QString &icon = QString());
    Q_INVOKABLE void removeSidebarFavorite(const QString &path);
    Q_INVOKABLE void announceContextMenuOpening(const QString &owner);

    void setPendingPasteRename(const QString &name);

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
    void tabsChanged();
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
    void sidebarFavoritesChanged();
    void dialogStateChanged();
    void contextMenuOpening(const QString &owner);
    void clipboardStateChanged();
    void fileOperationStateChanged();
    void pasteConflictStateChanged();
    void deviceStateChanged();

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
    FileOperationsController *m_fileOperations = nullptr;
    DeviceController *m_devices = nullptr;
    Runtime::ExplorerRuntimePaths m_runtimePaths;
    Services::ExplorerSettings m_settings;
    int m_fileModelRevision = 0;
    int m_sidebarFavoritesRevision = 0;
    bool m_dialogActive = false;
    QString m_dialogMode {QStringLiteral("browse")};
    QStringList m_dialogFilePatterns;
};

} // namespace Astrea::Explorer::Native::Backend
