#include <QDir>
#include <QFile>
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
    void deduplicatesXdgPathsWithoutShiftingMetadata();
    void preservesDistinctXdgMetadata();
};

class HomeEnvironmentGuard final
{
public:
    explicit HomeEnvironmentGuard(const QString &home)
        : m_hadHome(qEnvironmentVariableIsSet("HOME"))
        , m_previousHome(qgetenv("HOME"))
    {
        qputenv("HOME", home.toUtf8());
    }

    ~HomeEnvironmentGuard()
    {
        if (m_hadHome) {
            qputenv("HOME", m_previousHome);
        } else {
            qunsetenv("HOME");
        }
    }

private:
    bool m_hadHome = false;
    QByteArray m_previousHome;
};

SettingsService settingsAt(const QTemporaryDir &directory)
{
    return SettingsService(QDir(directory.path()).filePath(QStringLiteral("explorer.conf")));
}

QString pathAt(const QVariantList &items, int index)
{
    return items.at(index).toMap().value(QStringLiteral("path")).toString();
}

QVariantMap itemAtPath(const QVariantList &items, const QString &path)
{
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("path")).toString() == path) {
            return item;
        }
    }
    return {};
}

bool writeUserDirs(const QString &home, const QByteArray &contents)
{
    if (!QDir().mkpath(QDir(home).filePath(QStringLiteral(".config")))) {
        return false;
    }
    QFile file(QDir(home).filePath(QStringLiteral(".config/user-dirs.dirs")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(contents) == contents.size();
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

void SidebarFavoritesControllerTest::deduplicatesXdgPathsWithoutShiftingMetadata()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    HomeEnvironmentGuard homeGuard(home.path());
    QVERIFY(writeUserDirs(
        home.path(),
        QByteArrayLiteral(
            "XDG_DESKTOP_DIR=\"$HOME\"\n"
            "XDG_DOCUMENTS_DIR=\"$HOME\"\n"
            "XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n"
            "XDG_PICTURES_DIR=\"$HOME/Pictures\"\n"
            "XDG_MUSIC_DIR=\"$HOME/Music\"\n"
            "XDG_VIDEOS_DIR=\"$HOME/Videos\"\n"
            "XDG_PUBLICSHARE_DIR=\"$HOME/Public\"\n"
            "XDG_TEMPLATES_DIR=\"$HOME/Templates\"\n")));

    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    SettingsService settings = settingsAt(settingsDirectory);
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    const QString homePath = QDir::cleanPath(home.path());
    const QStringList expectedPaths {
        homePath,
        QDir(homePath).filePath(QStringLiteral("Downloads")),
        QDir(homePath).filePath(QStringLiteral("Pictures")),
        QDir(homePath).filePath(QStringLiteral("Music")),
        QDir(homePath).filePath(QStringLiteral("Videos")),
        QDir(homePath).filePath(QStringLiteral("Public")),
        QDir(homePath).filePath(QStringLiteral("Templates")),
    };
    QCOMPARE(favorites.defaultFavoritePaths(), expectedPaths);

    const QVariantList items = favorites.favorites();
    QCOMPARE(items.size(), expectedPaths.size());

    const QVariantMap desktop = itemAtPath(items, homePath);
    QCOMPARE(desktop.value(QStringLiteral("label")).toString(), QStringLiteral("Desktop"));
    QCOMPARE(desktop.value(QStringLiteral("icon")).toString(), QStringLiteral("user-desktop"));
    QCOMPARE(desktop.value(QStringLiteral("id")).toString(), QStringLiteral("builtin:0"));

    const QString downloadsPath = QDir(homePath).filePath(QStringLiteral("Downloads"));
    const QVariantMap downloads = itemAtPath(items, downloadsPath);
    QCOMPARE(downloads.value(QStringLiteral("label")).toString(), QStringLiteral("Downloads"));
    QCOMPARE(downloads.value(QStringLiteral("icon")).toString(), QStringLiteral("folder-download"));
    QCOMPARE(downloads.value(QStringLiteral("id")).toString(), QStringLiteral("builtin:2"));

    const QString picturesPath = QDir(homePath).filePath(QStringLiteral("Pictures"));
    const QVariantMap pictures = itemAtPath(items, picturesPath);
    QCOMPARE(pictures.value(QStringLiteral("label")).toString(), QStringLiteral("Pictures"));
    QCOMPARE(pictures.value(QStringLiteral("icon")).toString(), QStringLiteral("folder-pictures"));
    QCOMPARE(pictures.value(QStringLiteral("id")).toString(), QStringLiteral("builtin:3"));
}

void SidebarFavoritesControllerTest::preservesDistinctXdgMetadata()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    HomeEnvironmentGuard homeGuard(home.path());
    QVERIFY(writeUserDirs(
        home.path(),
        QByteArrayLiteral(
            "XDG_DESKTOP_DIR=\"$HOME/Desktop\"\n"
            "XDG_DOCUMENTS_DIR=\"$HOME/Documents\"\n"
            "XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n"
            "XDG_PICTURES_DIR=\"$HOME/Pictures\"\n"
            "XDG_MUSIC_DIR=\"$HOME/Music\"\n"
            "XDG_VIDEOS_DIR=\"$HOME/Videos\"\n"
            "XDG_PUBLICSHARE_DIR=\"$HOME/Public\"\n"
            "XDG_TEMPLATES_DIR=\"$HOME/Templates\"\n")));

    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    SettingsService settings = settingsAt(settingsDirectory);
    ExplorerSettingsController settingsController(&settings);
    SidebarFavoritesController favorites(&settingsController);

    const QString homePath = QDir::cleanPath(home.path());
    const QStringList paths {
        QDir(homePath).filePath(QStringLiteral("Desktop")),
        QDir(homePath).filePath(QStringLiteral("Documents")),
        QDir(homePath).filePath(QStringLiteral("Downloads")),
        QDir(homePath).filePath(QStringLiteral("Pictures")),
        QDir(homePath).filePath(QStringLiteral("Music")),
        QDir(homePath).filePath(QStringLiteral("Videos")),
        QDir(homePath).filePath(QStringLiteral("Public")),
        QDir(homePath).filePath(QStringLiteral("Templates")),
    };
    const QStringList labels {
        QStringLiteral("Desktop"), QStringLiteral("Documents"), QStringLiteral("Downloads"),
        QStringLiteral("Pictures"), QStringLiteral("Music"), QStringLiteral("Videos"),
        QStringLiteral("Public"), QStringLiteral("Templates"),
    };
    const QStringList icons {
        QStringLiteral("user-desktop"), QStringLiteral("folder-documents"),
        QStringLiteral("folder-download"), QStringLiteral("folder-pictures"),
        QStringLiteral("folder-music"), QStringLiteral("folder-videos"),
        QStringLiteral("folder-publicshare"), QStringLiteral("folder-templates"),
    };

    QCOMPARE(favorites.defaultFavoritePaths(), paths);
    const QVariantList items = favorites.favorites();
    QCOMPARE(items.size(), paths.size());
    for (int index = 0; index < paths.size(); ++index) {
        const QVariantMap item = itemAtPath(items, paths.at(index));
        QCOMPARE(item.value(QStringLiteral("label")).toString(), labels.at(index));
        QCOMPARE(item.value(QStringLiteral("icon")).toString(), icons.at(index));
        QCOMPARE(item.value(QStringLiteral("id")).toString(), QStringLiteral("builtin:%1").arg(index));
    }
}

QTEST_GUILESS_MAIN(SidebarFavoritesControllerTest)

#include "tst_sidebar_favorites_controller.moc"
