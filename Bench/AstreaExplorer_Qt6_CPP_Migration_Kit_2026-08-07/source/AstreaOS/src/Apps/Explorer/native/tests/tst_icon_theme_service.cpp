#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#define private public
#include "services/icon_theme_service.h"
#undef private

using Astrea::Explorer::Native::Services::IconThemeService;

namespace {

struct ThemeSearchPathGuard final
{
    ThemeSearchPathGuard()
        : paths(QIcon::themeSearchPaths())
        , themeName(QIcon::themeName())
    {
    }

    ~ThemeSearchPathGuard()
    {
        QIcon::setThemeSearchPaths(paths);
        QIcon::setThemeName(themeName);
    }

    QStringList paths;
    QString themeName;
};

void writeFile(const QString &path, const QByteArray &contents)
{
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    QCOMPARE(file.write(contents), contents.size());
}

void writeIcon(const QString &root, const QString &directory, const QString &name, const QColor &color)
{
    const QString path = QDir(root).filePath(directory + QLatin1Char('/') + name + QStringLiteral(".png"));
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    QVERIFY2(image.save(path, "PNG"), qPrintable(path));
}

void writeAlphaIcon(const QString &root, const QString &directory, const QString &name, const QColor &color)
{
    const QString path = QDir(root).filePath(directory + QLatin1Char('/') + name + QStringLiteral(".png"));
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    for (int y = 8; y < 24; ++y) {
        for (int x = 8; x < 24; ++x) {
            image.setPixelColor(x, y, color);
        }
    }
    QVERIFY2(image.save(path, "PNG"), qPrintable(path));
}

void writeTheme(const QString &root, const QString &name, const QColor &actionColor, bool inherited)
{
    const QString themeRoot = QDir(root).filePath(name);
    const QByteArray index = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Test Theme\n"
        "Directories=16x16/actions,actions/symbolic,16x16/places,16x16/devices,16x16/mimetypes,32x32/actions,scalable/mimetypes\n"
        "Inherits=ThemeParent\n"
        "\n"
        "[16x16/actions]\nSize=16\nContext=Actions\nType=Fixed\n\n"
        "[actions/symbolic]\nSize=16\nMinSize=16\nMaxSize=512\nContext=Actions\nType=Scalable\n\n"
        "[16x16/places]\nSize=16\nContext=Places\nType=Fixed\n\n"
        "[16x16/devices]\nSize=16\nContext=Devices\nType=Fixed\n\n"
        "[16x16/mimetypes]\nSize=16\nContext=MimeTypes\nType=Fixed\n\n"
        "[32x32/actions]\nSize=32\nContext=Actions\nType=Fixed\n\n"
        "[scalable/mimetypes]\nSize=48\nMinSize=16\nMaxSize=256\nContext=MimeTypes\nType=Scalable\n");
    writeFile(QDir(themeRoot).filePath(QStringLiteral("index.theme")), index);
    writeIcon(themeRoot, QStringLiteral("16x16/actions"), QStringLiteral("test-action"), actionColor);
    writeIcon(themeRoot, QStringLiteral("32x32/actions"), QStringLiteral("test-action"), actionColor);
    writeIcon(themeRoot, QStringLiteral("16x16/places"), QStringLiteral("test-place"), actionColor);
    writeIcon(themeRoot, QStringLiteral("16x16/devices"), QStringLiteral("test-device"), actionColor);
    writeIcon(themeRoot, QStringLiteral("16x16/mimetypes"), QStringLiteral("application-pdf"), actionColor);
    writeIcon(themeRoot, QStringLiteral("16x16/mimetypes"), QStringLiteral("application-x-generic"), actionColor);
    writeIcon(themeRoot, QStringLiteral("scalable/mimetypes"), QStringLiteral("text-x-generic"), actionColor);
    if (inherited) {
        const QString parentRoot = QDir(root).filePath(QStringLiteral("ThemeParent"));
        writeFile(
            QDir(parentRoot).filePath(QStringLiteral("index.theme")),
            QByteArrayLiteral(
                "[Icon Theme]\nName=Theme Parent\nDirectories=16x16/mimetypes\n\n"
                "[16x16/mimetypes]\nSize=16\nContext=MimeTypes\nType=Fixed\n"));
        writeIcon(parentRoot, QStringLiteral("16x16/mimetypes"), QStringLiteral("inherited-icon"), QColor(0x44, 0xaa, 0x66));
    }
}

void writeThemeConfigObject(const QString &path, const QJsonObject &object)
{
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QSaveFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(data), data.size());
    QVERIFY2(file.commit(), qPrintable(path));
}

void writeThemeConfig(const QString &path, const QString &theme, const QString &marker = QString())
{
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QSaveFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    QJsonObject object {{QStringLiteral("desktop_icon_theme"), theme}};
    if (!marker.isEmpty()) {
        object.insert(QStringLiteral("test_marker"), marker);
    }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(data), data.size());
    QVERIFY2(file.commit(), qPrintable(path));
}

void writeAstreaThemeConfig(const QString &path, const QString &theme)
{
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QSaveFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    const QByteArray data = QJsonDocument(QJsonObject {
        {QStringLiteral("icon_theme"), theme}})
        .toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(data), data.size());
    QVERIFY2(file.commit(), qPrintable(path));
}

QColor centerColor(const QImage &image)
{
    return image.pixelColor(image.width() / 2, image.height() / 2);
}

} // namespace

class IconThemeServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUnsafeThemeIdentifiers();
    void resolvesConfigThemeAndMimeCandidates();
    void ignoresBorealisIconThemeForDesktopSelection();
    void desktopThemeOverridesBorealisIconTheme();
    void iconThemeOnlyDoesNotSelectDesktopTheme();
    void invalidDesktopThemeFallsBackToPlatformTheme();
    void selectsAppearanceAwareInstalledVariant();
    void appearanceVariantFallsBackToBaseTheme();
    void compatibilityDefaultUsesAppearanceVariant();
    void variantFallbacksWhenSiblingUnavailable();
    void themeProbeRestoresGlobalTheme();
    void rendersRequestedSizeAndBuiltInFallback();
    void symbolicSourceUsesActualSymbolicCandidates();
    void rendersActualSymbolicArtworkWithoutRecoloring();
    void generatesCanonicalSymbolicAliases();
    void missingSymbolicCandidateUsesSymbolicFallback();
    void reloadsCanonicalConfigAfterAtomicReplacement();
    void rendersAndReloadsAppearanceVariant();
    void environmentOverrideWinsOverCanonicalConfig();
};

void IconThemeServiceTest::rejectsUnsafeThemeIdentifiers()
{
    QVERIFY(IconThemeService::isValidThemeIdentifier(QStringLiteral("MacTahoe")));
    QVERIFY(IconThemeService::isValidThemeIdentifier(QStringLiteral("theme-1.2")));
    QVERIFY(!IconThemeService::isValidThemeIdentifier(QStringLiteral("../MacTahoe")));
    QVERIFY(!IconThemeService::isValidThemeIdentifier(QStringLiteral("theme/child")));
    QVERIFY(!IconThemeService::isValidThemeIdentifier(QStringLiteral("theme\\child")));
    QVERIFY(!IconThemeService::isValidThemeIdentifier(QStringLiteral("")));
}

void IconThemeServiceTest::resolvesConfigThemeAndMimeCandidates()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), true);
    QIcon::setThemeSearchPaths({directory.path()});

    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("config/AstreaOS/ui/theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
    QVERIFY(service.revision() > 0);

    const QMimeType pdf = QMimeDatabase().mimeTypeForFile(
        QStringLiteral("report.pdf"), QMimeDatabase::MatchExtension);
    const QStringList fileCandidates = service.iconCandidatesForFile(
        QStringLiteral("smb://server/share/report.pdf"), false, false);
    QVERIFY(!fileCandidates.isEmpty());
    QVERIFY(fileCandidates.contains(pdf.iconName()));
    QVERIFY(fileCandidates.contains(pdf.genericIconName()));

    const QString downloadPath = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation).value(0);
    QVERIFY(!downloadPath.isEmpty());
    QVERIFY(service.iconCandidatesForFile(downloadPath, true, false).contains(QStringLiteral("folder-download")));
    QVERIFY(service.iconCandidatesForFile(QDir::homePath(), true, false).contains(QStringLiteral("user-home")));
    QTemporaryDir misleadingDirectory;
    QVERIFY(misleadingDirectory.isValid());
    const QString misleadingDownloads = QDir(misleadingDirectory.path()).filePath(QStringLiteral("Downloads"));
    QVERIFY(QDir().mkpath(misleadingDownloads));
    QVERIFY(!service.iconCandidatesForFile(misleadingDownloads, true, false).contains(QStringLiteral("folder-download")));

    const QStringList inherited = service.iconCandidatesForNames({QStringLiteral("inherited-icon")});
    QVERIFY(!service.resolveIcon(inherited).isNull());
    for (const QString &contextIcon : {
             QStringLiteral("test-action"),
             QStringLiteral("test-place"),
             QStringLiteral("test-device"),
             QStringLiteral("application-pdf"),
             QStringLiteral("text-x-generic")}) {
        QVERIFY2(
            !service.resolveIcon(service.iconCandidatesForNames({contextIcon})).isNull(),
            qPrintable(contextIcon));
    }
}

void IconThemeServiceTest::ignoresBorealisIconThemeForDesktopSelection()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeAstreaThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.effectiveTheme(), QString());
}

void IconThemeServiceTest::desktopThemeOverridesBorealisIconTheme()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("icon_theme"), QStringLiteral("dark")},
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.configuredBaseTheme(), QStringLiteral("ThemeA"));
    QCOMPARE(service.appearance(), IconThemeService::AppearanceMode::Dark);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
}

void IconThemeServiceTest::iconThemeOnlyDoesNotSelectDesktopTheme()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("ThemeA"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeAstreaThemeConfig(configPath, QStringLiteral("dark"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
}

void IconThemeServiceTest::invalidDesktopThemeFallsBackToPlatformTheme()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("ThemeA"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("../ThemeA")},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
}

void IconThemeServiceTest::selectsAppearanceAwareInstalledVariant()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeTheme(directory.path(), QStringLiteral("ThemeA-dark"), QColor(0x55, 0x55, 0xdd), false);
    writeTheme(directory.path(), QStringLiteral("ThemeA-light"), QColor(0x55, 0xdd, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), 0},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService darkService(configPath);
    QCOMPARE(darkService.effectiveTheme(), QStringLiteral("ThemeA-dark"));

    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("light")},
        {QStringLiteral("theme_mode"), 1},
    });
    QTRY_COMPARE_WITH_TIMEOUT(darkService.effectiveTheme(), QStringLiteral("ThemeA-light"), 3000);

    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA-dark")},
        {QStringLiteral("theme"), QStringLiteral("light")},
        {QStringLiteral("theme_mode"), 1},
    });
    QTRY_COMPARE_WITH_TIMEOUT(darkService.effectiveTheme(), QStringLiteral("ThemeA-dark"), 3000);

    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA-light")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), 0},
    });
    QTRY_COMPARE_WITH_TIMEOUT(darkService.effectiveTheme(), QStringLiteral("ThemeA-light"), 3000);
}

void IconThemeServiceTest::appearanceVariantFallsBackToBaseTheme()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
}

void IconThemeServiceTest::compatibilityDefaultUsesAppearanceVariant()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("MacTahoe"), QColor(0xdd, 0x55, 0x55), false);
    writeTheme(directory.path(), QStringLiteral("MacTahoe-dark"), QColor(0x55, 0x55, 0xdd), false);
    writeTheme(directory.path(), QStringLiteral("MacTahoe-light"), QColor(0x55, 0xdd, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), 0},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);

    QCOMPARE(service.effectiveTheme(), QStringLiteral("MacTahoe-dark"));
    QCOMPARE(QIcon::themeName(), QStringLiteral("MacTahoe-dark"));

    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("theme"), QStringLiteral("light")},
        {QStringLiteral("theme_mode"), 1},
    });
    QTRY_COMPARE_WITH_TIMEOUT(service.effectiveTheme(), QStringLiteral("MacTahoe-light"), 3000);
    QCOMPARE(QIcon::themeName(), QStringLiteral("MacTahoe-light"));
}

void IconThemeServiceTest::variantFallbacksWhenSiblingUnavailable()
{
    ThemeSearchPathGuard guard;
    qunsetenv("ASTREA_ICON_THEME");

    auto runCase = [](const QStringList &themes, const QJsonObject &config) {
        QTemporaryDir directory;
        if (!directory.isValid()) {
            return QString();
        }
        for (const QString &theme : themes) {
            writeTheme(directory.path(), theme, QColor(0xdd, 0x55, 0x55), false);
        }
        QIcon::setThemeSearchPaths({directory.path()});
        QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
        const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
        writeThemeConfigObject(configPath, config);
        IconThemeService service(configPath);
        return service.effectiveTheme();
    };

    QCOMPARE(
        runCase(
            {QStringLiteral("ThemeA"), QStringLiteral("ThemeA-light")},
            QJsonObject {
                {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
                {QStringLiteral("theme"), QStringLiteral("dark")},
            }),
        QStringLiteral("ThemeA"));
    QCOMPARE(
        runCase(
            {QStringLiteral("ThemeA"), QStringLiteral("ThemeA-dark")},
            QJsonObject {
                {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
                {QStringLiteral("theme"), QStringLiteral("light")},
            }),
        QStringLiteral("ThemeA"));
    QCOMPARE(
        runCase(
            {QStringLiteral("ThemeA")},
            QJsonObject {
                {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
                {QStringLiteral("theme"), QStringLiteral("dark")},
            }),
        QStringLiteral("ThemeA"));
}

void IconThemeServiceTest::themeProbeRestoresGlobalTheme()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    QIcon::setThemeName(QStringLiteral("ProbeSentinel"));

    QVERIFY(service.themeIsUsable(QStringLiteral("ThemeA")));
    QCOMPARE(QIcon::themeName(), QStringLiteral("ProbeSentinel"));
    QVERIFY(!service.themeIsUsable(QStringLiteral("MissingTheme")));
    QCOMPARE(QIcon::themeName(), QStringLiteral("ProbeSentinel"));
}

void IconThemeServiceTest::rendersRequestedSizeAndBuiltInFallback()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QImage icon = service.renderIcon(
        service.iconCandidatesForNames({QStringLiteral("test-action")}), QSize(16, 16), 2.0);
    QCOMPARE(icon.size(), QSize(32, 32));
    QVERIFY(centerColor(icon).red() > centerColor(icon).green());

    const QImage fallback = service.renderIcon(
        service.iconCandidatesForNames({QStringLiteral("not-installed"), QStringLiteral("also-missing")}),
        QSize(24, 24),
        1.0);
    QVERIFY(!fallback.isNull());
    QCOMPARE(fallback.size(), QSize(24, 24));
}

void IconThemeServiceTest::symbolicSourceUsesActualSymbolicCandidates()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA/actions/symbolic"),
        QStringLiteral("test-action-symbolic"),
        QColor(0x33, 0xaa, 0x77));
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA/16x16/actions"),
        QStringLiteral("test-alpha"),
        QColor(0xdd, 0x55, 0x55, 0xff));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QStringList candidates = service.iconCandidatesForNames({QStringLiteral("test-action")});
    const QImage fullColor = service.renderIcon(candidates, QSize(16, 16));

    const QString source = service.symbolicIconSourceForNames({QStringLiteral("test-action")}, 16);
    QVERIFY(source.contains(QStringLiteral("test-action-symbolic")));
    QVERIFY(!source.contains(QStringLiteral("mode=symbolic")));
    QCOMPARE(fullColor.pixelColor(8, 8), QColor(0xdd, 0x55, 0x55));
}

void IconThemeServiceTest::rendersActualSymbolicArtworkWithoutRecoloring()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA/actions/symbolic"),
        QStringLiteral("test-action-symbolic"),
        QColor(0x33, 0xaa, 0x77));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QImage fullColor = service.renderIcon(
        service.iconCandidatesForNames({QStringLiteral("test-action")}), QSize(16, 16));
    const QImage symbolic = service.renderIcon(
        service.symbolicCandidatesForNames({QStringLiteral("test-action")}), QSize(16, 16));

    QCOMPARE(fullColor.pixelColor(8, 8), QColor(0xdd, 0x55, 0x55));
    QCOMPARE(symbolic.pixelColor(8, 8), QColor(0x33, 0xaa, 0x77));
    QCOMPARE(symbolic.pixelColor(0, 0).alpha(), 0);
}

void IconThemeServiceTest::generatesCanonicalSymbolicAliases()
{
    ThemeSearchPathGuard guard;
    IconThemeService service;

    const QStringList names {
        QStringLiteral("user-home"),
        QStringLiteral("document-open-recent"),
        QStringLiteral("folder-github"),
        QStringLiteral("user-desktop"),
        QStringLiteral("folder-documents"),
        QStringLiteral("folder-download"),
        QStringLiteral("folder-downloads"),
        QStringLiteral("folder-pictures"),
        QStringLiteral("folder-music"),
        QStringLiteral("folder-videos"),
        QStringLiteral("folder-publicshare"),
        QStringLiteral("folder-templates"),
        QStringLiteral("computer"),
        QStringLiteral("drive-harddisk"),
        QStringLiteral("drive-removable-media"),
        QStringLiteral("network-workgroup"),
        QStringLiteral("user-trash"),
        QStringLiteral("system-search"),
    };
    const QStringList candidates = service.symbolicCandidatesForNames(names);

    for (const QString &name : names) {
        QVERIFY2(candidates.contains(name + QStringLiteral("-symbolic")), qPrintable(name));
    }
    QVERIFY(candidates.contains(QStringLiteral("folder-symbolic")));
    QVERIFY(candidates.contains(QStringLiteral("image-missing-symbolic")));

    const QStringList directoryCandidates = service.symbolicCandidatesForNames({QStringLiteral("inode-directory")});
    QCOMPARE(directoryCandidates.value(0), QStringLiteral("folder-symbolic"));
    const QStringList homeCandidates = service.symbolicCandidatesForNames({QStringLiteral("folder-home")});
    QCOMPARE(homeCandidates.value(0), QStringLiteral("user-home-symbolic"));
    const QStringList desktopCandidates = service.symbolicCandidatesForNames({QStringLiteral("folder-desktop")});
    QCOMPARE(desktopCandidates.value(0), QStringLiteral("user-desktop-symbolic"));
    const QStringList downloadsCandidates = service.symbolicCandidatesForNames({QStringLiteral("folder-downloads")});
    QCOMPARE(downloadsCandidates.mid(0, 3), QStringList({
        QStringLiteral("folder-download-symbolic"),
        QStringLiteral("folder-downloads-symbolic"),
        QStringLiteral("folder-symbolic"),
    }));
}

void IconThemeServiceTest::missingSymbolicCandidateUsesSymbolicFallback()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeIcon(
        directory.path(),
        QStringLiteral("ThemeA/16x16/actions"),
        QStringLiteral("custom-normal-icon"),
        QColor(0xee, 0x33, 0x33));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QString fullColorSource = service.iconSourceForNames({QStringLiteral("custom-normal-icon")}, 16);
    const QString symbolicSource = service.symbolicIconSourceForNames({QStringLiteral("custom-normal-icon")}, 16);

    QVERIFY(fullColorSource.contains(QStringLiteral("custom-normal-icon")));
    QVERIFY(symbolicSource.contains(QStringLiteral("image-missing-symbolic")));
    QVERIFY(!symbolicSource.contains(QStringLiteral("mode=symbolic")));
}

void IconThemeServiceTest::reloadsCanonicalConfigAfterAtomicReplacement()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeTheme(directory.path(), QStringLiteral("ThemeB"), QColor(0x55, 0x55, 0xdd), false);
    QIcon::setThemeSearchPaths({directory.path()});

    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("ui/theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");
    IconThemeService service(configPath);
    const quint64 previousRevision = service.revision();

    writeThemeConfig(configPath, QStringLiteral("ThemeB"), QStringLiteral("reload-2"));
    QTRY_COMPARE_WITH_TIMEOUT(service.effectiveTheme(), QStringLiteral("ThemeB"), 3000);
    QVERIFY(service.revision() > previousRevision);
    QCOMPARE(QIcon::themeName(), QStringLiteral("ThemeB"));

    const quint64 sameThemeRevision = service.revision();
    writeThemeConfig(configPath, QStringLiteral("ThemeB"));
    QSignalSpy themeChanges(&service, &IconThemeService::themeChanged);
    QTest::qWait(300);
    QCOMPARE(service.revision(), sameThemeRevision);
    QCOMPARE(themeChanges.count(), 0);
}

void IconThemeServiceTest::rendersAndReloadsAppearanceVariant()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeTheme(directory.path(), QStringLiteral("ThemeA-dark"), QColor(0x55, 0x55, 0xdd), false);
    writeTheme(directory.path(), QStringLiteral("ThemeA-light"), QColor(0x55, 0xdd, 0x55), false);
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA/actions/symbolic"),
        QStringLiteral("user-home-symbolic"),
        QColor(0xdd, 0x55, 0x55));
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA-dark/actions/symbolic"),
        QStringLiteral("user-home-symbolic"),
        QColor(0x55, 0x55, 0xdd));
    writeAlphaIcon(
        directory.path(),
        QStringLiteral("ThemeA-light/actions/symbolic"),
        QStringLiteral("user-home-symbolic"),
        QColor(0x55, 0xdd, 0x55));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("light")},
        {QStringLiteral("theme_mode"), 1},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    QSignalSpy themeChanges(&service, &IconThemeService::themeChanged);
    const QStringList candidates = service.symbolicCandidatesForNames({QStringLiteral("user-home")});
    const quint64 lightRevision = service.revision();
    const QImage lightIcon = service.renderIcon(candidates, QSize(16, 16));
    QCOMPARE(lightIcon.pixelColor(8, 8), QColor(0x55, 0xdd, 0x55));

    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), 0},
    });
    QTRY_COMPARE_WITH_TIMEOUT(service.effectiveTheme(), QStringLiteral("ThemeA-dark"), 3000);
    QVERIFY(service.revision() > lightRevision);
    QCOMPARE(themeChanges.count(), 1);
    const QImage darkIcon = service.renderIcon(candidates, QSize(16, 16));
    QCOMPARE(darkIcon.pixelColor(8, 8), QColor(0x55, 0x55, 0xdd));

    const quint64 darkRevision = service.revision();
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), 0},
    });
    QTest::qWait(300);
    QCOMPARE(service.revision(), darkRevision);
    QCOMPARE(themeChanges.count(), 1);
}

void IconThemeServiceTest::environmentOverrideWinsOverCanonicalConfig()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeTheme(directory.path(), QStringLiteral("ThemeB"), QColor(0x55, 0x55, 0xdd), false);
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qputenv("ASTREA_ICON_THEME", QByteArrayLiteral("ThemeB"));

    IconThemeService service(configPath);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeB"));
    qunsetenv("ASTREA_ICON_THEME");
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    IconThemeServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_icon_theme_service.moc"
