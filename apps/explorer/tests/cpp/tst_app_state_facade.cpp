#include <functional>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/device_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/recent_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "models/sidebar_favorites_model.h"
#include "services/directory_watch_service.h"
#include "services/filesystem_service.h"
#include "services/recent_store.h"
#include "services/settings_service.h"
#include "controllers/explorer_settings_controller.h"
#include "controllers/sidebar_favorites_controller.h"

#include "controllers/archive_controller.h"
#include "controllers/app_state_facade.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class AppStateFacadeTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesPersistedSettingsAndNavigationOptions();
    void doesNotComposeSettingsDomainBindings();
    void writesSettingsThroughCompatibilityProperties();
    void persistsUnifiedFavoriteOrder();
    void exposesTransactionalFavoriteModel();
    void exposesCoreQmlContract();
    void locksPublicQmlContract();
    void exposesResolverAndDialogCompatibility();
    void propagatesNavigationAndBackendFailure();
    void preservesSelectionAcrossModelRefresh();
    void delegatesQmlModelMutationsToNativeBoundary();
    void routesRecentOperationsToNativeBoundary();
    void projectsArchiveCompletionThroughFilesystemActionFinished();
    void resetsArchivePresentationStateAcrossOperations();
    void retainsSelectionWhenDeleteFails();
};

struct FacadeFixture
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation {&client, &model, &watcher};
    SelectionController selection {&model};
};

AppStateFacadeDependencies facadeDependencies(
    FacadeFixture &fixture,
    ExplorerSettingsController *settings = nullptr,
    SidebarFavoritesController *sidebarFavorites = nullptr,
    ArchiveController *archive = nullptr,
    FileOperationsController *fileOperations = nullptr,
    DeviceController *devices = nullptr,
    RecentController *recent = nullptr,
    FilesystemService *filesystem = nullptr,
    OpenWithController *openWith = nullptr,
    LaunchService *launch = nullptr,
    WallpaperService *wallpaper = nullptr,
    MimeAppsService *mimeApps = nullptr,
    IconThemeService *iconTheme = nullptr,
    Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths runtimePaths = {})
{
    AppStateFacadeDependencies dependencies;
    dependencies.navigation = &fixture.navigation;
    dependencies.selection = &fixture.selection;
    dependencies.model = &fixture.model;
    dependencies.settings = settings;
    dependencies.sidebarFavorites = sidebarFavorites;
    dependencies.archive = archive;
    dependencies.fileOperations = fileOperations;
    dependencies.devices = devices;
    dependencies.recent = recent;
    dependencies.filesystem = filesystem;
    dependencies.openWith = openWith;
    dependencies.launch = launch;
    dependencies.wallpaper = wallpaper;
    dependencies.mimeApps = mimeApps;
    dependencies.iconTheme = iconTheme;
    dependencies.runtimePaths = runtimePaths;
    return dependencies;
}

DirectoryEntry facadeSelectionEntry(const QString &name, const QString &path)
{
    DirectoryEntry entry;
    entry.fileName = name;
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    return entry;
}

void AppStateFacadeTest::exposesPersistedSettingsAndNavigationOptions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("explorer.conf"));
    SettingsService settings(path);
    ExplorerSettings expected;
    expected.showPreview = true;
    expected.viewMode = QStringLiteral("icon");
    expected.sortField = QStringLiteral("date");
    expected.sortAscending = false;
    expected.showHidden = true;
    expected.foldersFirst = false;
    expected.groupingEnabled = false;
    expected.zoomLevel = 1.5;
    QVERIFY(settings.save(expected));

    FacadeFixture fixture;
    ExplorerSettingsController settingsController(&settings);
    settingsController.bindNavigation(&fixture.navigation);
    AppStateFacade facade(facadeDependencies(fixture, &settingsController));

    QCOMPARE(facade.showPreview(), true);
    QCOMPARE(facade.viewMode(), QStringLiteral("icon"));
    QCOMPARE(facade.sortField(), QStringLiteral("date"));
    QCOMPARE(facade.sortAsc(), false);
    QCOMPARE(facade.showHidden(), true);
    QCOMPARE(facade.foldersFirst(), false);
    QCOMPARE(facade.groupingEnabled(), false);
    QCOMPARE(facade.zoomLevel(), 1.5);
    QCOMPARE(fixture.navigation.showHidden(), true);
    QCOMPARE(fixture.navigation.sortField(), QStringLiteral("date"));
    QCOMPARE(fixture.navigation.sortAscending(), false);
    QCOMPARE(fixture.navigation.foldersFirst(), false);
}

void AppStateFacadeTest::doesNotComposeSettingsDomainBindings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));

    FacadeFixture fixture;
    DeviceController devices(&fixture.client);
    const bool navigationShowHiddenBefore = fixture.navigation.showHidden();
    const QString autoMountIdsBefore = devices.autoMountDeviceIdsJson();

    ExplorerSettings persisted;
    persisted.showHidden = !navigationShowHiddenBefore;
    persisted.autoMountDeviceIdsJson = QStringLiteral("[\"fixture-device\"]");
    QVERIFY(settings.save(persisted));

    ExplorerSettingsController settingsController(&settings);
    AppStateFacade facade(facadeDependencies(
        fixture,
        &settingsController,
        nullptr,
        nullptr,
        nullptr,
        &devices));

    QCOMPARE(fixture.navigation.showHidden(), navigationShowHiddenBefore);
    QCOMPARE(devices.autoMountDeviceIdsJson(), autoMountIdsBefore);

    settingsController.setShowHidden(navigationShowHiddenBefore);
    settingsController.setShowHidden(!navigationShowHiddenBefore);
    settingsController.setAutoMountDeviceIdsJson(QStringLiteral("[\"second-device\"]"));

    QCOMPARE(fixture.navigation.showHidden(), navigationShowHiddenBefore);
    QCOMPARE(devices.autoMountDeviceIdsJson(), autoMountIdsBefore);
}

void AppStateFacadeTest::writesSettingsThroughCompatibilityProperties()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
    FacadeFixture fixture;
    ExplorerSettingsController settingsController(&settings);
    AppStateFacade facade(facadeDependencies(fixture, &settingsController));

    facade.setShowPreview(true);
    facade.setViewMode(QStringLiteral("icon"));
    facade.setSortField(QStringLiteral("kind"));
    facade.setSortAsc(false);
    facade.setShowHidden(true);
    facade.setFoldersFirst(false);
    facade.setGroupingEnabled(false);
    facade.setZoomLevel(1.75);
    facade.setAutoMountDeviceIdsJson(QStringLiteral("[\"usb-1\"]"));
    facade.setSidebarFavoritesJson(QStringLiteral("[\"/fixture\"]"));
    facade.setSidebarHiddenDefaultFavoritesJson(QStringLiteral("[\"/fixture/Hidden\"]"));

    const ExplorerSettings loaded = settings.load();
    QCOMPARE(loaded.showPreview, true);
    QCOMPARE(loaded.viewMode, QStringLiteral("icon"));
    QCOMPARE(loaded.sortField, QStringLiteral("kind"));
    QCOMPARE(loaded.sortAscending, false);
    QCOMPARE(loaded.showHidden, true);
    QCOMPARE(loaded.foldersFirst, false);
    QCOMPARE(loaded.groupingEnabled, false);
    QCOMPARE(loaded.zoomLevel, 1.75);
    QCOMPARE(loaded.autoMountDeviceIdsJson, QStringLiteral("[\"usb-1\"]"));
    QCOMPARE(loaded.sidebarFavoritesJson, QStringLiteral("[\"/fixture\"]"));
    QCOMPARE(loaded.sidebarHiddenDefaultFavoritesJson, QStringLiteral("[\"/fixture/Hidden\"]"));
}

void AppStateFacadeTest::persistsUnifiedFavoriteOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
    FacadeFixture fixture;
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController sidebarFavorites(&settingsController);
    AppStateFacade facade(
        facadeDependencies(fixture, &settingsController, &sidebarFavorites));

    facade.setSidebarFavoritesJson(QStringLiteral(
        "[{\"path\":\"/fixture/one\",\"label\":\"One\"},"
        "{\"path\":\"/fixture/two\",\"label\":\"Two\"}]"));
    QCOMPARE(
        facade.sidebarFavorites().constFirst().toMap().value(QStringLiteral("path")).toString(),
        QStringLiteral("/fixture/one"));

    facade.moveSidebarFavorite(QStringLiteral("/fixture/two"), 0);

    const QJsonDocument persisted = QJsonDocument::fromJson(
        settings.load().sidebarFavoritesJson.toUtf8());
    QVERIFY(persisted.isArray());
    QCOMPARE(
        persisted.array().at(0).toObject().value(QStringLiteral("path")).toString(),
        QStringLiteral("/fixture/two"));
    QCOMPARE(
        facade.sidebarFavorites().constFirst().toMap().value(QStringLiteral("path")).toString(),
        QStringLiteral("/fixture/two"));
}

void AppStateFacadeTest::exposesTransactionalFavoriteModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
    FacadeFixture fixture;
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController sidebarFavorites(&settingsController);
    AppStateFacade facade(
        facadeDependencies(fixture, &settingsController, &sidebarFavorites));

    facade.setSidebarFavoritesJson(QStringLiteral(
        "[{\"path\":\"/fixture/one\",\"label\":\"One\"},"
        "{\"path\":\"/fixture/two\",\"label\":\"Two\"}]"));
    QAbstractItemModel *model = facade.sidebarFavoritesModel();
    QVERIFY(model != nullptr);
    const int twoIndex = [&]() {
        for (int index = 0; index < model->rowCount(); ++index) {
            if (model->data(model->index(index, 0), SidebarFavoritesModel::PathRole).toString()
                == QStringLiteral("/fixture/two")) {
                return index;
            }
        }
        return -1;
    }();
    QVERIFY(twoIndex > 0);

    QVERIFY(facade.beginSidebarFavoriteDrag(QStringLiteral("/fixture/two")));
    QVERIFY(facade.previewSidebarFavoriteMove(QStringLiteral("/fixture/two"), 0));
    QCOMPARE(
        model->data(model->index(0, 0), SidebarFavoritesModel::PathRole).toString(),
        QStringLiteral("/fixture/two"));
    QCOMPARE(
        settings.load().sidebarFavoritesJson,
        QStringLiteral("[{\"path\":\"/fixture/one\",\"label\":\"One\"},"
                       "{\"path\":\"/fixture/two\",\"label\":\"Two\"}]"));
    QVERIFY(facade.commitSidebarFavoriteDrag());
    QCOMPARE(
        QJsonDocument::fromJson(settings.load().sidebarFavoritesJson.toUtf8())
            .array()
            .at(0)
            .toObject()
            .value(QStringLiteral("path"))
            .toString(),
        QStringLiteral("/fixture/two"));

    QVERIFY(facade.beginSidebarFavoriteDrag(QStringLiteral("/fixture/two")));
    QVERIFY(facade.previewSidebarFavoriteMove(QStringLiteral("/fixture/two"), 1));
    facade.cancelSidebarFavoriteDrag();
    QCOMPARE(
        model->data(model->index(0, 0), SidebarFavoritesModel::PathRole).toString(),
        QStringLiteral("/fixture/two"));
}

void AppStateFacadeTest::exposesCoreQmlContract()
{
    FacadeFixture fixture;
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model);

    const QMetaObject &metaObject = AppStateFacade::staticMetaObject;
    for (const char *propertyName : {
             "fileModel", "currentPath", "history", "historyIdx", "tabs",
             "breadcrumbParts", "activeTabIndex", "loadingDir", "loadError",
             "searchActive", "searchQuery", "selectedFile", "selectedFiles",
             "fileModelRevision", "showPreview", "viewMode", "sortField",
             "sortAsc", "showHidden", "foldersFirst", "groupingEnabled", "zoomLevel",
             "homePath", "runtimeRoot", "backendPath", "helperPath", "dialogActive",
             "sidebarFavoritesModel",
             "dialogMode", "dialogFilePatterns", "inTrashView", "recentVirtualPath"}) {
        QVERIFY2(
            metaObject.indexOfProperty(propertyName) >= 0,
            qPrintable(QStringLiteral("missing AppState property %1").arg(propertyName)));
    }
    QCOMPARE(facade.fileModel(), &fixture.model);
    QVERIFY(facade.tabs().isEmpty());
    QVERIFY(facade.breadcrumbParts().isEmpty());
}

void AppStateFacadeTest::locksPublicQmlContract()
{
    struct PropertyContract
    {
        const char *name;
        const char *type;
        bool writable;
    };

    const QList<PropertyContract> properties {
        {"fileModel", "QAbstractItemModel*", false},
        {"sidebarFavoritesModel", "QAbstractItemModel*", false},
        {"homePath", "QString", false},
        {"runtimeRoot", "QString", false},
        {"backendPath", "QString", false},
        {"helperPath", "QString", false},
        {"wallpaperManagerPath", "QString", false},
        {"astreaLaunch", "QString", false},
        {"windowsRun", "QString", false},
        {"networkRootPath", "QString", false},
        {"trashFilesPath", "QString", false},
        {"trashInfoPath", "QString", false},
        {"trashVirtualPath", "QString", false},
        {"recentVirtualPath", "QString", false},
        {"effectiveIconTheme", "QString", false},
        {"iconThemeRevision", "qulonglong", false},
        {"isPortalDialog", "bool", false},
        {"currentPath", "QString", false},
        {"history", "QStringList", false},
        {"historyIdx", "int", false},
        {"tabs", "QVariantList", false},
        {"breadcrumbParts", "QVariantList", false},
        {"activeTabIndex", "int", false},
        {"loadingDir", "bool", false},
        {"loadError", "QString", false},
        {"remoteDirectoryActive", "bool", false},
        {"searchActive", "bool", false},
        {"searchVisible", "bool", false},
        {"searchQuery", "QString", true},
        {"selectedFile", "QString", true},
        {"selectedFiles", "QStringList", false},
        {"selectedPaths", "QStringList", false},
        {"lastSelectedIndex", "int", false},
        {"fileModelRevision", "int", false},
        {"fileModelFilling", "bool", false},
        {"showPreview", "bool", true},
        {"previewsEnabled", "bool", true},
        {"viewMode", "QString", true},
        {"sortField", "QString", true},
        {"sortAsc", "bool", true},
        {"showHidden", "bool", true},
        {"foldersFirst", "bool", true},
        {"groupingEnabled", "bool", true},
        {"zoomLevel", "double", true},
        {"autoMountDeviceIdsJson", "QString", true},
        {"sidebarFavoritesJson", "QString", true},
        {"sidebarHiddenDefaultFavoritesJson", "QString", true},
        {"sidebarFavorites", "QVariantList", false},
        {"sidebarHiddenDefaultFavorites", "QVariantList", false},
        {"sidebarFavoritesRevision", "int", false},
        {"defaultSidebarFavoritePaths", "QStringList", false},
        {"inTrashView", "bool", false},
        {"dialogActive", "bool", true},
        {"dialogMode", "QString", true},
        {"dialogFilePatterns", "QStringList", true},
        {"clipboardFiles", "QStringList", false},
        {"clipboardMode", "QString", false},
        {"fileOperationRunning", "bool", false},
        {"fileOperationProgress", "double", false},
        {"fileOperationPercent", "int", false},
        {"fileOperationFileName", "QString", false},
        {"fileOperationStatus", "QString", false},
        {"fileOperationError", "QString", false},
        {"fileOperationDestination", "QString", false},
        {"fileOperationDoneCount", "int", false},
        {"fileOperationTotalCount", "int", false},
        {"fileOperationMode", "QString", false},
        {"fileOperationState", "QString", false},
        {"fileOperationItems", "QVariantList", false},
        {"pasteConflictVisible", "bool", false},
        {"pasteConflictItems", "QVariantList", false},
        {"pendingPasteRename", "QString", true},
        {"deviceModel", "QVariantList", false},
        {"deviceError", "QString", false},
        {"deviceOperationPath", "QString", false},
        {"deviceOperationType", "QString", false},
        {"deviceOperationTargetMountPath", "QString", false},
        {"deviceOperationOpenAfterMount", "bool", false},
        {"lastUnmountedMountPath", "QString", false},
        {"archiveExtractionRunning", "bool", false},
        {"archiveExtractionProgress", "double", false},
        {"archiveExtractionPercent", "int", false},
        {"archiveExtractionFileName", "QString", false},
        {"archiveExtractionStatus", "QString", false},
        {"archiveExtractionError", "QString", false},
        {"archiveExtractionDestination", "QString", false},
        {"archiveExtractionDoneCount", "int", false},
        {"archiveExtractionTotalCount", "int", false},
        {"archiveExtractionRemainingText", "QString", false},
        {"archivePasswordPromptVisible", "bool", false},
        {"archivePasswordError", "QString", false},
        {"archiveConflictVisible", "bool", false},
        {"archiveConflictDestination", "QString", false},
        {"archiveConflictName", "QString", false},
        {"appImageInstallRunning", "bool", false},
        {"wallpaperApplyRunning", "bool", false},
    };

    const QMetaObject &metaObject = AppStateFacade::staticMetaObject;
    QCOMPARE(metaObject.propertyCount() - metaObject.propertyOffset(), properties.size());
    for (const PropertyContract &expected : properties) {
        const int index = metaObject.indexOfProperty(expected.name);
        QVERIFY2(index >= 0, qPrintable(QStringLiteral("missing property %1").arg(expected.name)));
        const QMetaProperty property = metaObject.property(index);
        QCOMPARE(property.typeName(), expected.type);
        QCOMPARE(property.isWritable(), expected.writable);
    }

    struct MethodContract
    {
        const char *signature;
        const char *returnType;
    };
    const QList<MethodContract> methods {
        {"navigateTo(QString)", "qulonglong"},
        {"submitSearch(QString,QString)", "qulonglong"},
        {"startSearch()", "void"},
        {"hideSearch()", "void"},
        {"clearSearch()", "void"},
        {"goBack()", "void"},
        {"goForward()", "void"},
        {"createTab(QString)", "void"},
        {"closeTab(int)", "void"},
        {"switchTab(int)", "void"},
        {"closeTabById(int)", "void"},
        {"switchTabById(int)", "void"},
        {"tabIndexById(int)", "int"},
        {"moveTab(int,int)", "void"},
        {"refreshCurrentFolder()", "qulonglong"},
        {"loadRecent()", "void"},
        {"recordRecentAccess(QString,bool,QString)", "void"},
        {"createFolder(QString,QString)", "qulonglong"},
        {"renamePath(QString,QString)", "qulonglong"},
        {"requestDirectorySuggestions(QString,QString)", "qulonglong"},
        {"checkExecutable(QString)", "qulonglong"},
        {"requestProperties(QString)", "qulonglong"},
        {"createDesktopShortcut(QString)", "qulonglong"},
        {"requestNetworkMountProbe(QString)", "qulonglong"},
        {"connectToNetwork(QString)", "qulonglong"},
        {"refreshDevices()", "void"},
        {"ensureAutoMountDevices()", "void"},
        {"replaceFileModel(QVariantList)", "bool"},
        {"updateFileModelMetadata(QVariantList)", "int"},
        {"removePathsFromFileModel(QStringList)", "int"},
        {"increaseZoom()", "void"},
        {"decreaseZoom()", "void"},
        {"resetZoom()", "void"},
        {"setZoom(double)", "void"},
        {"selectedItem()", "QVariant"},
        {"fileUrlForPath(QString)", "QString"},
        {"joinPath(QString,QString)", "QString"},
        {"selectedPathsInCurrentFolder()", "QStringList"},
        {"selectedUriListInCurrentFolder()", "QString"},
        {"fileMatchesDialogFilter(QString,bool)", "bool"},
        {"dropFilePaths(QStringList,QString,QString)", "void"},
        {"dropFiles(QVariantList,QString,QString)", "void"},
        {"copySelected()", "void"},
        {"cutSelected()", "void"},
        {"pasteFiles()", "qulonglong"},
        {"isCutPending(QString)", "bool"},
        {"isCutPathPending(QString)", "bool"},
        {"resolvePasteConflict(QString)", "void"},
        {"renamePasteConflict(QString)", "void"},
        {"cancelPasteConflict()", "void"},
        {"deleteSelected()", "void"},
        {"restoreSelected()", "void"},
        {"emptyTrash()", "void"},
        {"startArchiveExtraction(QString,QString)", "void"},
        {"submitArchivePassword(QString)", "void"},
        {"cancelArchivePassword()", "void"},
        {"submitArchiveConflict(QString)", "void"},
        {"cancelArchiveConflict()", "void"},
        {"startFolderCompression(QString,QString)", "void"},
        {"installAppImage(QString)", "void"},
        {"setAsWallpaper(QString)", "void"},
        {"openWithApplications(QString)", "QVariantList"},
        {"launchOpenWith(QString,QString)", "bool"},
        {"setDefaultOpenWith(QString,QString)", "bool"},
        {"openItem(QString,bool,QString)", "void"},
        {"openFile(QString)", "void"},
        {"refreshPreviewMetadata()", "void"},
        {"requestThumbnailWarm(QString,int,int)", "void"},
        {"themedIconSource(QString,int,QString)", "QString"},
        {"sidebarIconSource(QString,int)", "QString"},
        {"fileIconName(QString,bool,bool)", "QString"},
        {"fileIconSource(QString,bool,bool,int,QString)", "QString"},
        {"writePortalResult(QString)", "bool"},
        {"requestMountDevice(QString,bool,bool)", "qulonglong"},
        {"requestUnmountDevice(QString,QString)", "qulonglong"},
        {"requestRemountDevice(QString,QString,bool)", "qulonglong"},
        {"toggleDeviceAutoMount(QString,bool)", "void"},
        {"isRecentPath(QString)", "bool"},
        {"isTrashPath(QString)", "bool"},
        {"canPinSidebarFavorite(QString)", "bool"},
        {"isSidebarFavorite(QString)", "bool"},
        {"visibleDefaultSidebarFavorites(QVariantList)", "QVariantList"},
        {"pinSidebarFavorite(QString,QString,QString)", "void"},
        {"removeSidebarFavorite(QString)", "void"},
        {"moveSidebarFavorite(QString,int)", "void"},
        {"beginSidebarFavoriteDrag(QString)", "bool"},
        {"previewSidebarFavoriteMove(QString,int)", "bool"},
        {"commitSidebarFavoriteDrag()", "bool"},
        {"cancelSidebarFavoriteDrag()", "void"},
        {"announceContextMenuOpening(QString)", "void"},
        {"isSelected(QString)", "bool"},
        {"isPathSelected(QString)", "bool"},
        {"clearSelection()", "void"},
        {"selectAll()", "void"},
        {"selectByName(QString)", "void"},
        {"selectByPath(QString)", "void"},
        {"prepareSelectionForDrag(QString,int)", "void"},
        {"handleSelection(QString,int,bool,bool,bool)", "void"},
    };

    for (const MethodContract &expected : methods) {
        const int index = metaObject.indexOfMethod(expected.signature);
        QVERIFY2(index >= 0, qPrintable(QStringLiteral("missing method %1").arg(expected.signature)));
        QCOMPARE(metaObject.method(index).returnMetaType().name(), expected.returnType);
    }

    for (const char *signalSignature : {
             "openWithReady(QString,QVariantList)",
             "currentPathChanged()",
             "historyChanged()",
             "tabsChanged()",
             "activeTabIndexChanged()",
             "loadingDirChanged()",
             "loadErrorChanged()",
             "remoteDirectoryActiveChanged()",
             "searchStateChanged()",
             "selectedFileChanged()",
             "selectedFilesChanged()",
             "selectedPathsChanged()",
             "lastSelectedIndexChanged()",
             "fileModelRevisionChanged()",
             "showPreviewChanged()",
             "viewModeChanged()",
             "sortFieldChanged()",
             "sortAscChanged()",
             "showHiddenChanged()",
             "foldersFirstChanged()",
             "groupingEnabledChanged()",
             "zoomLevelChanged()",
             "autoMountDeviceIdsJsonChanged()",
             "sidebarFavoritesJsonChanged()",
             "sidebarHiddenDefaultFavoritesJsonChanged()",
             "sidebarFavoritesChanged()",
             "dialogStateChanged()",
             "contextMenuOpening(QString)",
             "clipboardStateChanged()",
             "fileOperationStateChanged()",
             "pasteConflictStateChanged()",
             "deviceStateChanged()",
             "archiveStateChanged()",
             "wallpaperStateChanged()",
             "iconThemeChanged()",
             "filesystemActionFinished(quint64,QString,bool,QVariantMap,QString)",
         }) {
        QVERIFY2(
            metaObject.indexOfSignal(signalSignature) >= 0,
            qPrintable(QStringLiteral("missing signal %1").arg(signalSignature)));
    }
}

void AppStateFacadeTest::exposesResolverAndDialogCompatibility()
{
    FacadeFixture fixture;
    Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths runtimePaths;
    runtimePaths.root = QStringLiteral("/fixture/astrea");
    runtimePaths.backendProgram = QStringLiteral("/fixture/astrea/backend");
    runtimePaths.helperProgram.clear();
    runtimePaths.launcherProgram = QStringLiteral("/fixture/astrea/astrea-launch");
    runtimePaths.windowsRunnerProgram = QStringLiteral("/fixture/astrea/windows-run");

    ExplorerSettingsController settingsController(nullptr);
    AppStateFacade facade(facadeDependencies(fixture, &settingsController, nullptr, nullptr, nullptr, nullptr,
                                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                              nullptr, runtimePaths));

    QCOMPARE(facade.runtimeRoot(), QStringLiteral("/fixture/astrea"));
    QCOMPARE(facade.backendPath(), runtimePaths.backendProgram);
    QCOMPARE(facade.helperPath(), QString());
    QCOMPARE(facade.astreaLaunch(), runtimePaths.launcherProgram);
    QCOMPARE(facade.windowsRun(), runtimePaths.windowsRunnerProgram);
    QCOMPARE(facade.recentVirtualPath(), QStringLiteral("recent://"));
    QVERIFY(facade.trashFilesPath().endsWith(QStringLiteral("/.local/share/Trash/files")));

    facade.setDialogActive(true);
    facade.setDialogMode(QStringLiteral("open-file"));
    facade.setDialogFilePatterns({QStringLiteral("*.txt")});
    QVERIFY(facade.dialogActive());
    QCOMPARE(facade.dialogMode(), QStringLiteral("open-file"));
    QCOMPARE(facade.dialogFilePatterns(), QStringList {QStringLiteral("*.txt")});
    QVERIFY(facade.fileMatchesDialogFilter(QStringLiteral("notes.txt"), false));
    QVERIFY(!facade.fileMatchesDialogFilter(QStringLiteral("notes.png"), false));
    QVERIFY(facade.fileMatchesDialogFilter(QStringLiteral("folder"), true));

    const double originalZoom = facade.zoomLevel();
    facade.setZoomLevel(99.0);
    QCOMPARE(facade.zoomLevel(), 2.0);
    facade.resetZoom();
    QCOMPARE(facade.zoomLevel(), 1.0);
    QVERIFY(originalZoom >= 0.75);
}

void AppStateFacadeTest::propagatesNavigationAndBackendFailure()
{
    FacadeFixture fixture;
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model);
    QSignalSpy pathSpy(&facade, &AppStateFacade::currentPathChanged);
    QSignalSpy errorSpy(&facade, &AppStateFacade::loadErrorChanged);

    const BackendRequestId requestId = facade.navigateTo(QStringLiteral("/fixture"));
    QVERIFY(requestId != 0);
    QCOMPARE(facade.currentPath(), QStringLiteral("/fixture"));
    QVERIFY(pathSpy.count() > 0);

    fixture.client.failRequest(requestId, QStringLiteral("io"), QStringLiteral("fixture failure"));
    QTRY_COMPARE(facade.loadError(), QStringLiteral("fixture failure"));
    QVERIFY(errorSpy.count() > 0);
}

void AppStateFacadeTest::preservesSelectionAcrossModelRefresh()
{
    FacadeFixture fixture;
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model);

    DirectoryEntry first;
    first.fileName = QStringLiteral("first.txt");
    first.filePath = QStringLiteral("/fixture/first.txt");
    DirectoryEntry second;
    second.fileName = QStringLiteral("second.txt");
    second.filePath = QStringLiteral("/fixture/second.txt");
    fixture.model.setEntries({first, second}, 1);
    facade.selectByName(QStringLiteral("second.txt"));
    QCOMPARE(facade.selectedFile(), QStringLiteral("second.txt"));

    DirectoryEntry refreshed = second;
    refreshed.fileSize = 42;
    fixture.model.setEntries({refreshed, first}, 2);
    QCOMPARE(facade.selectedFile(), QStringLiteral("second.txt"));
    QCOMPARE(facade.selectedFiles(), QStringList {QStringLiteral("second.txt")});
}

void AppStateFacadeTest::delegatesQmlModelMutationsToNativeBoundary()
{
    FacadeFixture fixture;
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model);

    QVariantMap first;
    first.insert(QStringLiteral("fileName"), QStringLiteral("first.txt"));
    first.insert(QStringLiteral("filePath"), QStringLiteral("/fixture/first.txt"));
    first.insert(QStringLiteral("fileKind"), QStringLiteral("TXT"));
    QVariantMap second;
    second.insert(QStringLiteral("fileName"), QStringLiteral("second.txt"));
    second.insert(QStringLiteral("filePath"), QStringLiteral("/fixture/second.txt"));
    second.insert(QStringLiteral("fileKind"), QStringLiteral("TXT"));

    QSignalSpy revisionSpy(&facade, &AppStateFacade::fileModelRevisionChanged);
    QVERIFY(facade.replaceFileModel({first, second}));
    QCOMPARE(fixture.model.count(), 2);
    facade.selectByName(QStringLiteral("second.txt"));

    QVariantMap update;
    update.insert(QStringLiteral("filePath"), QStringLiteral("/fixture/first.txt"));
    update.insert(QStringLiteral("fileKind"), QStringLiteral("IMAGE"));
    QCOMPARE(facade.updateFileModelMetadata({update}), 1);
    QCOMPARE(
        fixture.model.data(fixture.model.index(0, 0), DirectoryModel::FileKindRole).toString(),
        QStringLiteral("IMAGE"));
    QVERIFY(revisionSpy.count() >= 2);

    QCOMPARE(
        facade.removePathsFromFileModel({QStringLiteral("/fixture/second.txt")}),
        1);
    QCOMPARE(fixture.model.count(), 1);
    QVERIFY(facade.selectedFiles().isEmpty());
    QCOMPARE(facade.selectedFile(), QString());
}

void AppStateFacadeTest::routesRecentOperationsToNativeBoundary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    RecentSourcePaths paths;
    paths.finderPath = QDir(directory.path()).filePath(QStringLiteral("finder.json"));
    paths.launchHistoryPath = QDir(directory.path()).filePath(QStringLiteral("history.jsonl"));
    paths.xbelPath = QDir(directory.path()).filePath(QStringLiteral("recent.xbel"));

    QVector<std::function<void()>> jobs;
    RecentStore store(
        paths,
        nullptr,
        [&jobs](std::function<void()> job) { jobs.append(std::move(job)); });
    RecentController recent(&store);
    FacadeFixture fixture;
    AppStateFacade facade(facadeDependencies(fixture, nullptr, nullptr, nullptr, nullptr, nullptr,
                                              &recent));

    DirectoryEntry entry;
    entry.fileName = QStringLiteral("notes.txt");
    entry.filePath = QStringLiteral("/fixture/notes.txt");
    entry.fileUrl = QUrl::fromLocalFile(entry.filePath);
    entry.fileSize = 42;
    entry.fileKind = QStringLiteral("TXT");
    QVERIFY(fixture.model.applyEntries({entry}, 1));

    facade.loadRecent();
    QCOMPARE(jobs.size(), 1);

    facade.recordRecentAccess(entry.filePath, false, entry.fileUrl.toString());
    QCOMPARE(recent.currentEntries().size(), 1);
    QCOMPARE(recent.currentEntries().constFirst().fileName, entry.fileName);
    QCOMPARE(recent.currentEntries().constFirst().fileSize, entry.fileSize);

    facade.recordRecentAccess(QStringLiteral("recent://"), false, QString());
    QCOMPARE(recent.currentEntries().size(), 1);
    facade.setDialogActive(true);
    facade.recordRecentAccess(QStringLiteral("/fixture/other.txt"), false, QString());
    QCOMPARE(recent.currentEntries().size(), 1);
}

void AppStateFacadeTest::projectsArchiveCompletionThroughFilesystemActionFinished()
{
    FacadeFixture fixture;
    FilesystemService filesystem(&fixture.client);
    ArchiveController archive(&filesystem, &fixture.navigation);
    AppStateFacade facade(facadeDependencies(
        fixture, nullptr, nullptr, &archive, nullptr, nullptr, nullptr, &filesystem));
    QSignalSpy actionSpy(&facade, &AppStateFacade::filesystemActionFinished);

    facade.startArchiveExtraction(QStringLiteral("/tmp/first.zip"), QStringLiteral("first"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    UtilityResult completed;
    completed.operation = QStringLiteral("archive-extract");
    completed.ok = true;
    completed.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/actual-first"));
    fixture.client.completeUtility(1, completed);

    QTRY_COMPARE(actionSpy.count(), 1);
    const QList<QVariant> arguments = actionSpy.constFirst();
    QCOMPARE(arguments.at(0).toULongLong(), quint64(1));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("archive-extract"));
    QCOMPARE(arguments.at(2).toBool(), true);
    QCOMPARE(
        arguments.at(3).toMap().value(QStringLiteral("destination")).toString(),
        QStringLiteral("/tmp/actual-first"));
    QCOMPARE(arguments.at(4).toString(), QString());
}

void AppStateFacadeTest::retainsSelectionWhenDeleteFails()
{
    FacadeFixture fixture;
    QVERIFY(fixture.model.applyEntries(
        {facadeSelectionEntry(QStringLiteral("item.txt"), QStringLiteral("/fixture/item.txt"))},
        1));
    fixture.selection.selectByPath(QStringLiteral("/fixture/item.txt"));
    FilesystemService filesystem(&fixture.client);
    AppStateFacade facade(facadeDependencies(
        fixture, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &filesystem));

    facade.deleteSelected();
    fixture.client.failRequest(1, QStringLiteral("permission_denied"), QStringLiteral("denied"));
    QTRY_COMPARE(fixture.selection.selectedPaths(), QStringList({QStringLiteral("/fixture/item.txt")}));

    facade.deleteSelected();
    UtilityResult result;
    result.requestId = 2;
    result.operation = QStringLiteral("trash");
    result.ok = true;
    fixture.client.completeUtility(2, result);
    QTRY_VERIFY(fixture.selection.selectedPaths().isEmpty());
}

void AppStateFacadeTest::resetsArchivePresentationStateAcrossOperations()
{
    FacadeFixture fixture;
    FilesystemService filesystem(&fixture.client);
    ArchiveController archive(&filesystem, &fixture.navigation);
    AppStateFacade facade(facadeDependencies(
        fixture, nullptr, nullptr, &archive, nullptr, nullptr, nullptr, &filesystem));

    facade.startArchiveExtraction(
        QStringLiteral("/fixture/archive.tar"), QStringLiteral("Extracted"));
    QCOMPARE(facade.archiveExtractionRunning(), true);
    QCOMPARE(facade.archiveExtractionProgress(), 0.0);
    QCOMPARE(facade.archiveExtractionPercent(), 0);
    QCOMPARE(facade.archiveExtractionDoneCount(), 0);
    QCOMPARE(facade.archiveExtractionTotalCount(), 0);
    QVERIFY(facade.archiveExtractionDestination().endsWith(QStringLiteral("/Extracted")));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    QCOMPARE(
        fixture.client.utilityRequests().constLast().operation,
        QStringLiteral("archive-extract"));

    UtilityResult extracted;
    extracted.operation = QStringLiteral("archive-extract");
    extracted.ok = true;
    extracted.data.insert(QStringLiteral("destination"), QStringLiteral("/fixture/Extracted-1"));
    fixture.client.completeUtility(1, extracted);
    QTRY_COMPARE(facade.archiveExtractionRunning(), false);
    QCOMPARE(facade.archiveExtractionPercent(), 100);
    QCOMPARE(facade.archiveExtractionProgress(), 1.0);
    QCOMPARE(facade.archiveExtractionDoneCount(), 1);
    QCOMPARE(facade.archiveExtractionTotalCount(), 1);
    QCOMPARE(facade.archiveExtractionDestination(), QStringLiteral("/fixture/Extracted-1"));

    facade.startFolderCompression(QStringLiteral("/fixture/folder"), QStringLiteral("zip"));
    QCOMPARE(facade.archiveExtractionRunning(), true);
    QCOMPARE(facade.archiveExtractionProgress(), 0.0);
    QCOMPARE(facade.archiveExtractionPercent(), 0);
    QCOMPARE(facade.archiveExtractionDoneCount(), 0);
    QCOMPARE(facade.archiveExtractionTotalCount(), 0);
    QVERIFY(facade.archiveExtractionDestination().endsWith(QStringLiteral("/folder.zip")));
    QCOMPARE(facade.archiveExtractionError(), QString());
    QCOMPARE(fixture.client.utilityRequests().size(), 2);
    QCOMPARE(
        fixture.client.utilityRequests().constLast().operation,
        QStringLiteral("archive-compress"));

    UtilityResult compressed;
    compressed.operation = QStringLiteral("archive-compress");
    compressed.ok = false;
    compressed.errorMessage = QStringLiteral("archive failed");
    // Successful extraction navigates to the result and consumes request id 2;
    // compression therefore owns the next utility request.
    fixture.client.completeUtility(3, compressed);
    QTRY_COMPARE(facade.archiveExtractionRunning(), false);
    QCOMPARE(facade.archiveExtractionError(), QStringLiteral("archive failed"));
    QCOMPARE(facade.archiveExtractionTotalCount(), 0);
    QVERIFY(facade.archiveExtractionDestination().endsWith(QStringLiteral("/folder.zip")));
}

QTEST_GUILESS_MAIN(AppStateFacadeTest)

#include "tst_app_state_facade.moc"
