#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/device_controller.h"
#include "controllers/explorer_settings_controller.h"
#include "controllers/navigation_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/settings_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class ExplorerSettingsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void startupSettingsApplyToNavigationAndDevice();
    void navigationChangesPersist();
    void controllerSettingsWritesPersist();
    void zoomLevelIsClamped();
    void currentPathChangesPersist();
    void runtimeAutoMountChangesPersist();
    void compatibilityJsonUpdatesRuntimeAndCanonicalizesInvalidInput();
    void reloadingSettingsRestoresCanonicalAutoMountState();
};

struct SettingsFixture
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation {&client, &model, &watcher};
    DeviceController devices {&client};
};

SettingsService settingsAt(const QTemporaryDir &directory)
{
    return SettingsService(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
}

void ExplorerSettingsControllerTest::startupSettingsApplyToNavigationAndDevice()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings expected;
    expected.showPreview = true;
    expected.viewMode = QStringLiteral("icon");
    expected.sortField = QStringLiteral("date");
    expected.sortAscending = false;
    expected.showHidden = true;
    expected.foldersFirst = false;
    expected.groupingEnabled = false;
    expected.zoomLevel = 1.5;
    expected.autoMountDeviceIdsJson = QStringLiteral("[\"usb-1\"]");
    QVERIFY(settings.save(expected));

    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindNavigation(&fixture.navigation);
    controller.bindDeviceController(&fixture.devices);

    QCOMPARE(controller.showPreview(), true);
    QCOMPARE(controller.viewMode(), QStringLiteral("icon"));
    QCOMPARE(controller.groupingEnabled(), false);
    QCOMPARE(controller.zoomLevel(), 1.5);
    QCOMPARE(fixture.navigation.previews(), true);
    QCOMPARE(fixture.navigation.sortField(), QStringLiteral("date"));
    QCOMPARE(fixture.navigation.sortAscending(), false);
    QCOMPARE(fixture.navigation.showHidden(), true);
    QCOMPARE(fixture.navigation.foldersFirst(), false);
    QVERIFY(fixture.devices.isAutoMount(QStringLiteral("usb-1")));
    QCOMPARE(fixture.devices.autoMountDeviceIdsJson(), QStringLiteral("[\"usb-1\"]"));
}

void ExplorerSettingsControllerTest::navigationChangesPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindNavigation(&fixture.navigation);

    fixture.navigation.setShowHidden(true);
    fixture.navigation.setSortField(QStringLiteral("kind"));
    fixture.navigation.setSortAscending(false);
    fixture.navigation.setFoldersFirst(false);

    const ExplorerSettings loaded = settings.load();
    QCOMPARE(loaded.showHidden, true);
    QCOMPARE(loaded.sortField, QStringLiteral("kind"));
    QCOMPARE(loaded.sortAscending, false);
    QCOMPARE(loaded.foldersFirst, false);
}

void ExplorerSettingsControllerTest::controllerSettingsWritesPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindNavigation(&fixture.navigation);

    controller.setShowPreview(true);
    controller.setViewMode(QStringLiteral("icon"));
    controller.setGroupingEnabled(false);
    controller.setSidebarFavoritesJson(QStringLiteral("[\"/fixture\"]"));
    controller.setSidebarHiddenDefaultFavoritesJson(QStringLiteral("[\"/fixture/Hidden\"]"));

    const ExplorerSettings loaded = settings.load();
    QCOMPARE(loaded.showPreview, true);
    QCOMPARE(loaded.viewMode, QStringLiteral("icon"));
    QCOMPARE(loaded.groupingEnabled, false);
    QCOMPARE(loaded.sidebarFavoritesJson, QStringLiteral("[\"/fixture\"]"));
    QCOMPARE(
        loaded.sidebarHiddenDefaultFavoritesJson,
        QStringLiteral("[\"/fixture/Hidden\"]"));
}

void ExplorerSettingsControllerTest::zoomLevelIsClamped()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettingsController controller(&settings);

    controller.setZoomLevel(0.1);
    QCOMPARE(controller.zoomLevel(), 0.75);
    controller.setZoomLevel(3.0);
    QCOMPARE(controller.zoomLevel(), 2.0);
}

void ExplorerSettingsControllerTest::currentPathChangesPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindNavigation(&fixture.navigation);

    fixture.navigation.navigateTo(QStringLiteral("/fixture"));

    QCOMPARE(settings.load().currentPath, QStringLiteral("/fixture"));
}

void ExplorerSettingsControllerTest::runtimeAutoMountChangesPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindNavigation(&fixture.navigation);
    controller.bindDeviceController(&fixture.devices);

    fixture.devices.setAutoMount(QStringLiteral("usb-1"), true);
    QCOMPARE(settings.load().autoMountDeviceIdsJson, QStringLiteral("[\"usb-1\"]"));
    fixture.devices.setAutoMount(QStringLiteral("usb-1"), false);
    QCOMPARE(settings.load().autoMountDeviceIdsJson, QStringLiteral("[]"));
}

void ExplorerSettingsControllerTest::compatibilityJsonUpdatesRuntimeAndCanonicalizesInvalidInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture fixture;
    ExplorerSettingsController controller(&settings);
    controller.bindDeviceController(&fixture.devices);

    controller.setAutoMountDeviceIdsJson(QStringLiteral("[\"compat\", 7, \"\"]"));

    QVERIFY(fixture.devices.isAutoMount(QStringLiteral("compat")));
    QCOMPARE(fixture.devices.autoMountDeviceIdsJson(), QStringLiteral("[\"compat\"]"));
    QCOMPARE(settings.load().autoMountDeviceIdsJson, QStringLiteral("[\"compat\"]"));

    controller.setAutoMountDeviceIdsJson(QStringLiteral("not-json"));

    QVERIFY(!fixture.devices.isAutoMount(QStringLiteral("compat")));
    QCOMPARE(fixture.devices.autoMountDeviceIdsJson(), QStringLiteral("[]"));
    QCOMPARE(settings.load().autoMountDeviceIdsJson, QStringLiteral("[]"));
}

void ExplorerSettingsControllerTest::reloadingSettingsRestoresCanonicalAutoMountState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    SettingsFixture firstFixture;
    ExplorerSettingsController firstController(&settings);
    firstController.bindDeviceController(&firstFixture.devices);
    firstFixture.devices.setAutoMount(QStringLiteral("usb-2"), true);

    SettingsFixture secondFixture;
    ExplorerSettingsController secondController(&settings);
    secondController.bindDeviceController(&secondFixture.devices);

    QVERIFY(secondFixture.devices.isAutoMount(QStringLiteral("usb-2")));
    QCOMPARE(secondFixture.devices.autoMountDeviceIdsJson(), QStringLiteral("[\"usb-2\"]"));
}

QTEST_GUILESS_MAIN(ExplorerSettingsControllerTest)

#include "tst_explorer_settings_controller.moc"
