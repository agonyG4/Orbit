#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "controllers/explorer_settings_controller.h"
#include "controllers/sidebar_favorites_controller.h"
#include "models/sidebar_favorites_model.h"
#include "services/settings_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class SidebarFavoritesControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsPersistedCustomFavoritesAndHiddenDefaults();
    void hidesAndRestoresDefaultFavorites();
    void rejectsDuplicateAndInvalidPins();
    void removesCustomFavoritesAndPreservesOrder();
    void dragCancelRestoresOriginalOrder();
    void dragCommitPersistsExactlyOnce();
    void filtersHiddenDefaultsFromProvidedItems();
};

SettingsService settingsAt(const QTemporaryDir &directory)
{
    return SettingsService(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
}

QString pathAt(const QVariantList &items, int index)
{
    return items.at(index).toMap().value(QStringLiteral("path")).toString();
}

void SidebarFavoritesControllerTest::loadsPersistedCustomFavoritesAndHiddenDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings persisted;
    persisted.sidebarFavoritesJson = QStringLiteral(
        "[{\"path\":\"/custom/one\",\"label\":\"One\",\"icon\":\"folder\"}]");
    persisted.sidebarHiddenDefaultFavoritesJson = QStringLiteral("[\"%1\"]").arg(
        QDir::home().filePath(QStringLiteral("Desktop")));
    QVERIFY(settings.save(persisted));

    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    QVERIFY(favorites.isFavorite(QStringLiteral("/custom/one")));
    QVERIFY(!favorites.favorites().isEmpty());
    QVERIFY(!favorites.isFavorite(QDir::home().filePath(QStringLiteral("Desktop"))));
}

void SidebarFavoritesControllerTest::hidesAndRestoresDefaultFavorites()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);
    const QString defaultPath = favorites.defaultFavoritePaths().constFirst();

    QVERIFY(favorites.isFavorite(defaultPath));
    favorites.remove(defaultPath);
    QVERIFY(!favorites.isFavorite(defaultPath));
    favorites.pin(defaultPath, QString(), QString());
    QVERIFY(favorites.isFavorite(defaultPath));
}

void SidebarFavoritesControllerTest::rejectsDuplicateAndInvalidPins()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    QVERIFY(!favorites.canPin(QStringLiteral("relative/path")));
    QVERIFY(!favorites.canPin(QStringLiteral("trash://")));
    favorites.pin(QStringLiteral("/custom/one"), QStringLiteral("One"), QStringLiteral("folder"));
    const int revision = favorites.revision();
    favorites.pin(QStringLiteral("/custom/one"), QStringLiteral("Duplicate"), QStringLiteral("other"));
    QCOMPARE(favorites.revision(), revision);
    const int customIndex = favorites.favorites().indexOf(
        QVariantMap {{QStringLiteral("path"), QStringLiteral("/custom/one")},
                     {QStringLiteral("label"), QStringLiteral("One")},
                     {QStringLiteral("icon"), QStringLiteral("folder")}});
    QVERIFY(customIndex >= 0);
}

void SidebarFavoritesControllerTest::removesCustomFavoritesAndPreservesOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings persisted;
    persisted.sidebarFavoritesJson = QStringLiteral(
        "[{\"path\":\"/custom/one\"},{\"path\":\"/custom/two\"}]");
    QVERIFY(settings.save(persisted));
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    QCOMPARE(pathAt(favorites.favorites(), 0), QStringLiteral("/custom/one"));
    QCOMPARE(pathAt(favorites.favorites(), 1), QStringLiteral("/custom/two"));
    favorites.remove(QStringLiteral("/custom/one"));
    QCOMPARE(pathAt(favorites.favorites(), 0), QStringLiteral("/custom/two"));
}

void SidebarFavoritesControllerTest::dragCancelRestoresOriginalOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings persisted;
    persisted.sidebarFavoritesJson = QStringLiteral(
        "[{\"path\":\"/custom/one\"},{\"path\":\"/custom/two\"},{\"path\":\"/custom/three\"}]");
    QVERIFY(settings.save(persisted));
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    QVERIFY(favorites.beginDrag(QStringLiteral("/custom/one")));
    QVERIFY(favorites.previewMove(QStringLiteral("/custom/one"), 2));
    favorites.cancelDrag();
    QCOMPARE(pathAt(favorites.favorites(), 0), QStringLiteral("/custom/one"));
    QCOMPARE(pathAt(favorites.favorites(), 1), QStringLiteral("/custom/two"));
    QCOMPARE(pathAt(favorites.favorites(), 2), QStringLiteral("/custom/three"));
}

void SidebarFavoritesControllerTest::dragCommitPersistsExactlyOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings persisted;
    persisted.sidebarFavoritesJson = QStringLiteral(
        "[{\"path\":\"/custom/one\"},{\"path\":\"/custom/two\"},{\"path\":\"/custom/three\"}]");
    QVERIFY(settings.save(persisted));
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);
    QSignalSpy settingsSpy(
        &settingsController,
        &ExplorerSettingsController::sidebarFavoritesJsonChanged);

    QVERIFY(favorites.beginDrag(QStringLiteral("/custom/one")));
    QVERIFY(favorites.previewMove(QStringLiteral("/custom/one"), 2));
    QVERIFY(favorites.commitDrag());
    QCOMPARE(settingsSpy.count(), 1);
    QCOMPARE(pathAt(favorites.favorites(), 0), QStringLiteral("/custom/two"));
    QCOMPARE(pathAt(favorites.favorites(), 2), QStringLiteral("/custom/one"));
}

void SidebarFavoritesControllerTest::filtersHiddenDefaultsFromProvidedItems()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SettingsService settings = settingsAt(directory);
    ExplorerSettings persisted;
    const QString hiddenPath = QDir::home().filePath(QStringLiteral("Desktop"));
    persisted.sidebarHiddenDefaultFavoritesJson = QStringLiteral("[\"%1\"]").arg(hiddenPath);
    QVERIFY(settings.save(persisted));

    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);
    QVariantMap hiddenItem;
    hiddenItem.insert(QStringLiteral("path"), hiddenPath);
    QVariantMap visibleItem;
    visibleItem.insert(QStringLiteral("path"), QStringLiteral("/custom/visible"));

    const QVariantList filtered = favorites.visibleDefaults({hiddenItem, visibleItem});
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(pathAt(filtered, 0), QStringLiteral("/custom/visible"));
}

QTEST_GUILESS_MAIN(SidebarFavoritesControllerTest)

#include "tst_sidebar_favorites_controller.moc"
