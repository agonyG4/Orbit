#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "services/settings_service.h"

using namespace Astrea::Explorer::Native::Services;

class SettingsServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsLegacyDefaultsWhenFileIsMissing();
    void readsLegacyExplorerCategory();
    void writesLegacyExplorerCategory();
};

void SettingsServiceTest::returnsLegacyDefaultsWhenFileIsMissing()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsService service(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
    const ExplorerSettings settings = service.load();

    QCOMPARE(settings.currentPath, QString());
    QCOMPARE(settings.showPreview, false);
    QCOMPARE(settings.viewMode, QStringLiteral("list"));
    QCOMPARE(settings.sortField, QStringLiteral("name"));
    QCOMPARE(settings.sortAscending, true);
    QCOMPARE(settings.showHidden, false);
    QCOMPARE(settings.foldersFirst, true);
    QCOMPARE(settings.groupingEnabled, true);
    QCOMPARE(settings.zoomLevel, 1.0);
    QCOMPARE(settings.autoMountDeviceIdsJson, QStringLiteral("[]"));
    QCOMPARE(settings.sidebarFavoritesJson, QStringLiteral("[]"));
    QCOMPARE(settings.sidebarHiddenDefaultFavoritesJson, QStringLiteral("[]"));
}

void SettingsServiceTest::readsLegacyExplorerCategory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("explorer.conf"));

    QSettings persisted(path, QSettings::IniFormat);
    persisted.beginGroup(QStringLiteral("Explorer"));
    persisted.setValue(QStringLiteral("currentPath"), QStringLiteral("/fixture/Home"));
    persisted.setValue(QStringLiteral("showPreview"), true);
    persisted.setValue(QStringLiteral("viewMode"), QStringLiteral("icon"));
    persisted.setValue(QStringLiteral("sortField"), QStringLiteral("date"));
    persisted.setValue(QStringLiteral("sortAsc"), false);
    persisted.setValue(QStringLiteral("showHidden"), true);
    persisted.setValue(QStringLiteral("foldersFirst"), false);
    persisted.setValue(QStringLiteral("groupingEnabled"), false);
    persisted.setValue(QStringLiteral("zoomLevel"), 1.65);
    persisted.setValue(QStringLiteral("autoMountDeviceIdsJson"), QStringLiteral("[\"usb-1\"]"));
    persisted.setValue(QStringLiteral("sidebarFavoritesJson"), QStringLiteral("[\"/fixture\"]"));
    persisted.setValue(
        QStringLiteral("sidebarHiddenDefaultFavoritesJson"),
        QStringLiteral("[\"/fixture/Hidden\"]"));
    persisted.endGroup();
    QVERIFY(persisted.status() == QSettings::NoError);

    const ExplorerSettings settings = SettingsService(path).load();
    QCOMPARE(settings.currentPath, QStringLiteral("/fixture/Home"));
    QCOMPARE(settings.showPreview, true);
    QCOMPARE(settings.viewMode, QStringLiteral("icon"));
    QCOMPARE(settings.sortField, QStringLiteral("date"));
    QCOMPARE(settings.sortAscending, false);
    QCOMPARE(settings.showHidden, true);
    QCOMPARE(settings.foldersFirst, false);
    QCOMPARE(settings.groupingEnabled, false);
    QCOMPARE(settings.zoomLevel, 1.65);
    QCOMPARE(settings.autoMountDeviceIdsJson, QStringLiteral("[\"usb-1\"]"));
    QCOMPARE(settings.sidebarFavoritesJson, QStringLiteral("[\"/fixture\"]"));
    QCOMPARE(settings.sidebarHiddenDefaultFavoritesJson, QStringLiteral("[\"/fixture/Hidden\"]"));
}

void SettingsServiceTest::writesLegacyExplorerCategory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("explorer.conf"));

    ExplorerSettings expected;
    expected.currentPath = QStringLiteral("/fixture/Write");
    expected.showPreview = true;
    expected.viewMode = QStringLiteral("icon");
    expected.sortField = QStringLiteral("kind");
    expected.sortAscending = false;
    expected.showHidden = true;
    expected.foldersFirst = false;
    expected.groupingEnabled = false;
    expected.zoomLevel = 1.25;
    expected.autoMountDeviceIdsJson = QStringLiteral("[\"network-1\"]");
    expected.sidebarFavoritesJson = QStringLiteral("[\"/fixture/Favorites\"]");
    expected.sidebarHiddenDefaultFavoritesJson = QStringLiteral("[\"/fixture/Hidden\"]");

    SettingsService service(path);
    QVERIFY(service.save(expected));

    QSettings persisted(path, QSettings::IniFormat);
    persisted.beginGroup(QStringLiteral("Explorer"));
    QCOMPARE(persisted.value(QStringLiteral("currentPath")).toString(), expected.currentPath);
    QCOMPARE(persisted.value(QStringLiteral("showPreview")).toBool(), expected.showPreview);
    QCOMPARE(persisted.value(QStringLiteral("viewMode")).toString(), expected.viewMode);
    QCOMPARE(persisted.value(QStringLiteral("sortField")).toString(), expected.sortField);
    QCOMPARE(persisted.value(QStringLiteral("sortAsc")).toBool(), expected.sortAscending);
    QCOMPARE(persisted.value(QStringLiteral("showHidden")).toBool(), expected.showHidden);
    QCOMPARE(persisted.value(QStringLiteral("foldersFirst")).toBool(), expected.foldersFirst);
    QCOMPARE(
        persisted.value(QStringLiteral("groupingEnabled")).toBool(),
        expected.groupingEnabled);
    QCOMPARE(persisted.value(QStringLiteral("zoomLevel")).toDouble(), expected.zoomLevel);
    QCOMPARE(
        persisted.value(QStringLiteral("autoMountDeviceIdsJson")).toString(),
        expected.autoMountDeviceIdsJson);
    QCOMPARE(
        persisted.value(QStringLiteral("sidebarFavoritesJson")).toString(),
        expected.sidebarFavoritesJson);
    QCOMPARE(
        persisted.value(QStringLiteral("sidebarHiddenDefaultFavoritesJson")).toString(),
        expected.sidebarHiddenDefaultFavoritesJson);
}

QTEST_MAIN(SettingsServiceTest)

#include "tst_settings_service.moc"
