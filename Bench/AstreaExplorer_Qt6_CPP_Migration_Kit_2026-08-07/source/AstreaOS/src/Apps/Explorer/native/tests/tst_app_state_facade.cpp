#include <functional>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/navigation_controller.h"
#include "controllers/recent_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/filesystem_service.h"
#include "services/recent_store.h"
#include "services/settings_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class AppStateFacadeTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesPersistedSettingsAndNavigationOptions();
    void writesSettingsThroughCompatibilityProperties();
    void persistsUnifiedFavoriteOrder();
    void exposesCoreQmlContract();
    void exposesResolverAndDialogCompatibility();
    void propagatesNavigationAndBackendFailure();
    void preservesSelectionAcrossModelRefresh();
    void delegatesQmlModelMutationsToNativeBoundary();
    void routesRecentOperationsToNativeBoundary();
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
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        &settings);

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

void AppStateFacadeTest::writesSettingsThroughCompatibilityProperties()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
    FacadeFixture fixture;
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        &settings);

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
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        &settings);

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
             "dialogMode", "dialogFilePatterns", "inTrashView", "recentVirtualPath"}) {
        QVERIFY2(
            metaObject.indexOfProperty(propertyName) >= 0,
            qPrintable(QStringLiteral("missing AppState property %1").arg(propertyName)));
    }
    QCOMPARE(facade.fileModel(), &fixture.model);
    QVERIFY(facade.tabs().isEmpty());
    QVERIFY(facade.breadcrumbParts().isEmpty());
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

    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        runtimePaths);

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
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        {},
        &recent);

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

void AppStateFacadeTest::retainsSelectionWhenDeleteFails()
{
    FacadeFixture fixture;
    QVERIFY(fixture.model.applyEntries(
        {facadeSelectionEntry(QStringLiteral("item.txt"), QStringLiteral("/fixture/item.txt"))},
        1));
    fixture.selection.selectByPath(QStringLiteral("/fixture/item.txt"));
    FilesystemService filesystem(&fixture.client);
    AppStateFacade facade(
        &fixture.navigation,
        &fixture.selection,
        &fixture.model,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        {},
        nullptr,
        &filesystem);

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

QTEST_GUILESS_MAIN(AppStateFacadeTest)

#include "tst_app_state_facade.moc"
