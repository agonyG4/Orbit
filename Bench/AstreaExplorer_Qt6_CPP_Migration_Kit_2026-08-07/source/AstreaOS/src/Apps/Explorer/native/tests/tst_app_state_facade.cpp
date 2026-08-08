#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/settings_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class AppStateFacadeTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesPersistedSettingsAndNavigationOptions();
    void writesSettingsThroughCompatibilityProperties();
};

struct FacadeFixture
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation {&client, &model, &watcher};
    SelectionController selection {&model};
};

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

QTEST_GUILESS_MAIN(AppStateFacadeTest)

#include "tst_app_state_facade.moc"
