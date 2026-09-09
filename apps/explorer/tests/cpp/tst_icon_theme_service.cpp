#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#define private public
#include "services/freedesktop_icon_theme_catalog.h"
#include "services/icon_theme_service.h"
#undef private

using Astrea::Explorer::Native::Services::FreedesktopIconThemeCatalog;
using Astrea::Explorer::Native::Services::IconThemeService;

namespace {

struct ThemeSearchPathGuard final
{
    ThemeSearchPathGuard()
        : paths(QIcon::themeSearchPaths())
        , themeName(QIcon::themeName())
        , fallbackThemeName(QIcon::fallbackThemeName())
        , fallbackSearchPaths(QIcon::fallbackSearchPaths())
    {
    }

    ~ThemeSearchPathGuard()
    {
        QIcon::setThemeName(themeName);
        QIcon::setThemeSearchPaths(paths);
        QIcon::setFallbackThemeName(fallbackThemeName);
        QIcon::setFallbackSearchPaths(fallbackSearchPaths);
    }

    QStringList paths;
    QString themeName;
    QString fallbackThemeName;
    QStringList fallbackSearchPaths;
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

void writeXpmIcon(const QString &root, const QString &directory, const QString &name, const QColor &color)
{
    const QString path = QDir(root).filePath(directory + QLatin1Char('/') + name + QStringLiteral(".xpm"));
    const QByteArray contents = QByteArrayLiteral("/* XPM */\nstatic const char *icon[] = {\n")
        + QByteArrayLiteral("\"2 2 1 1\",\n\"  c ")
        + color.name(QColor::HexRgb).toLatin1()
        + QByteArrayLiteral("\",\n\"  \",\n\"  \"\n};\n");
    writeFile(path, contents);
}

void writeSvgIcon(const QString &root, const QString &directory, const QString &name, const QColor &color)
{
    const QString path = QDir(root).filePath(directory + QLatin1Char('/') + name + QStringLiteral(".svg"));
    const QByteArray contents = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\">"
        "<rect width=\"32\" height=\"32\" fill=\"%1\"/></svg>")
        .arg(color.name(QColor::HexRgb))
        .toUtf8();
    writeFile(path, contents);
}

void replaceIconAtomically(
    const QString &root,
    const QString &directory,
    const QString &name,
    const QColor &color)
{
    const QString path = QDir(root).filePath(directory + QLatin1Char('/') + name + QStringLiteral(".png"));
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QSaveFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    QVERIFY2(image.save(&file, "PNG"), qPrintable(path));
    QVERIFY2(file.commit(), qPrintable(path));
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

void writeSolidImage(const QString &path, const QSize &size, const QColor &color)
{
    QVERIFY2(QDir().mkpath(QFileInfo(path).absolutePath()), qPrintable(path));
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    QVERIFY2(image.save(path, "PNG"), qPrintable(path));
}

void writeSimpleThemeIndex(
    const QString &root,
    const QString &name,
    const QStringList &directories,
    const QStringList &inherits = {},
    const QStringList &scaledDirectories = {})
{
    QString index = QStringLiteral(
        "[Icon Theme]\n"
        "Name=Test Theme\n"
        "Comment=Theme used by deterministic tests\n"
        "Directories=")
        + directories.join(QLatin1Char(','))
        + QLatin1Char('\n');
    if (!inherits.isEmpty()) {
        index += QStringLiteral("Inherits=") + inherits.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    if (!scaledDirectories.isEmpty()) {
        index += QStringLiteral("ScaledDirectories=")
            + scaledDirectories.join(QLatin1Char(','))
            + QLatin1Char('\n');
    }

    QStringList allDirectories = directories;
    for (const QString &directory : scaledDirectories) {
        if (!allDirectories.contains(directory)) {
            allDirectories.append(directory);
        }
    }
    for (const QString &directory : allDirectories) {
        index += QStringLiteral("\n[") + directory
            + QStringLiteral("]\nSize=16\nType=Fixed\n");
    }
    writeFile(QDir(root).filePath(name + QStringLiteral("/index.theme")), index.toUtf8());
}

void writeSimpleTheme(
    const QString &root,
    const QString &name,
    const QStringList &directories = {QStringLiteral("16x16/mimetypes")},
    const QStringList &inherits = {},
    const QStringList &scaledDirectories = {})
{
    writeSimpleThemeIndex(root, name, directories, inherits, scaledDirectories);
}

void writeTheme(const QString &root, const QString &name, const QColor &actionColor, bool inherited)
{
    const QString themeRoot = QDir(root).filePath(name);
    const QByteArray index = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Test Theme\n"
        "Comment=Theme used by deterministic tests\n"
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
                "[Icon Theme]\nName=Theme Parent\nComment=Theme parent\nDirectories=16x16/mimetypes\n\n"
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

void configureSingleIconTheme(
    const QString &root,
    const QString &theme,
    const QString &iconName,
    const QColor &color,
    QString *configPath)
{
    writeSimpleTheme(root, theme);
    writeIcon(root, theme + QStringLiteral("/16x16/mimetypes"), iconName, color);
    QIcon::setThemeSearchPaths({root});
    *configPath = QDir(root).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(*configPath, theme);
    qunsetenv("ASTREA_ICON_THEME");
}

QImage renderThemeSource(IconThemeService &service, const QString &source, const QSize &size)
{
    const QString prefix = QStringLiteral("image://astrea-icons/theme/");
    const QString id = source.mid(prefix.size());
    const int queryIndex = id.indexOf(QLatin1Char('?'));
    const QString encodedCandidates = queryIndex < 0 ? id : id.left(queryIndex);
    const QStringList candidates = QUrl::fromPercentEncoding(encodedCandidates.toUtf8())
        .split(QLatin1Char('|'), Qt::SkipEmptyParts);
    return service.renderIcon(candidates, size);
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
    void renderIconUsesPhysicalPixelsForDpr();
    void symbolicSourceUsesActualSymbolicCandidates();
    void rendersActualSymbolicArtworkWithoutRecoloring();
    void generatesCanonicalSymbolicAliases();
    void missingSymbolicCandidateUsesSymbolicFallback();
    void missingEmblemIsOmitted();
    void rendersAvailableEmblem();
    void resolvesSymbolicLinkEmblemIdentity();
    void resolvesPrefixedSymbolicLinkEmblemIdentity();
    void resolvesAutomaticAccessEmblemIdentities();
    void resolvesBareMetadataEmblemKeyword();
    void semanticIconOverrideBeatsExactCustomFile();
    void reloadsCanonicalConfigAfterAtomicReplacement();
    void rendersAndReloadsAppearanceVariant();
    void environmentOverrideWinsOverCanonicalConfig();
    void themeExistsUsesIndexNotProbeIcons();
    void listResolutionPrefersSelectedThemeCandidate();
    void listResolutionPrefersSpecificSelectedCandidate();
    void resolvesExactInheritedParentAsset();
    void rendersInheritedParentBeforeFallbackThroughService();
    void rendersSelectedGenericBeforeFallbackSpecificThroughService();
    void inheritedThemePrecedesPlatformFallback();
    void inheritedThemesFollowRecursiveDeclarationOrder();
    void hicolorIsAlwaysLastThemedFallback();
    void inheritanceCyclesTerminate();
    void splitThemeRootsUseFirstMetadataAndAllAssets();
    void scaledDirectoriesParticipateInPresence();
    void selectsDeclaredDirectoryBySizeAndScale();
    void closestDirectoryDistanceIgnoresScalePriority();
    void declaredDirectoryOrderPrecedesRootOrder();
    void exactThemeFormatsAgreeWithQtSupport();
    void themeAssetChangesInvalidateRenderedResults();
    void installingPreferredVariantInvalidatesTopology();
    void installingCompatibilityVariantInvalidatesTopology();
    void unrelatedThemeDoesNotInvalidateTopology();
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
    writeTheme(directory.path(), QStringLiteral("ProbeFallback"), QColor(0x55, 0x55, 0xdd), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("ProbeFallback"));
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

void IconThemeServiceTest::renderIconUsesPhysicalPixelsForDpr()
{
    IconThemeService service;
    const QString normal = service.iconSourceForNames({QStringLiteral("test-action")}, 16, 1.0);
    const QString hidpi = service.iconSourceForNames({QStringLiteral("test-action")}, 16, 2.0);
    QVERIFY(normal != hidpi);
    QVERIFY(normal.contains(QStringLiteral("dpr=1")));
    QVERIFY(hidpi.contains(QStringLiteral("dpr=2")));
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

void IconThemeServiceTest::missingEmblemIsOmitted()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeThatIsNotInstalled"));

    IconThemeService service(configPath);
    QCOMPARE(
        service.emblemIconSource(
            QStringLiteral("astrea-test-emblem-that-is-not-installed-987654"),
            18),
        QString());
}

void IconThemeServiceTest::rendersAvailableEmblem()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    writeIcon(
        directory.path(),
        QStringLiteral("ThemeA/16x16/actions"),
        QStringLiteral("emblem-astrea-test"),
        QColor(0x22, 0xaa, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    IconThemeService service(configPath);

    const QString source = service.emblemIconSource(QStringLiteral("astrea-test"), 16);
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("emblem-astrea-test")));
}

void IconThemeServiceTest::resolvesSymbolicLinkEmblemIdentity()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString configPath;
    configureSingleIconTheme(
        directory.path(),
        QStringLiteral("EmblemTheme"),
        QStringLiteral("symbolic-link-symbolic"),
        QColor(0x22, 0xaa, 0x66),
        &configPath);
    IconThemeService service(configPath);

    QVERIFY(!service.emblemIconSource(QStringLiteral("symbolic-link-symbolic"), 16).isEmpty());
}

void IconThemeServiceTest::resolvesPrefixedSymbolicLinkEmblemIdentity()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString configPath;
    configureSingleIconTheme(
        directory.path(),
        QStringLiteral("EmblemTheme"),
        QStringLiteral("emblem-symbolic-link-symbolic"),
        QColor(0x22, 0xaa, 0x66),
        &configPath);
    IconThemeService service(configPath);

    QVERIFY(!service.emblemIconSource(QStringLiteral("symbolic-link-symbolic"), 16).isEmpty());
}

void IconThemeServiceTest::resolvesAutomaticAccessEmblemIdentities()
{
    ThemeSearchPathGuard guard;
    for (const QString &iconName : {
             QStringLiteral("not-accessible-symbolic"),
             QStringLiteral("readonly-symbolic")}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString configPath;
        configureSingleIconTheme(
            directory.path(),
            QStringLiteral("EmblemTheme"),
            iconName,
            QColor(0x22, 0xaa, 0x66),
            &configPath);
        IconThemeService service(configPath);

        QVERIFY2(
            !service.emblemIconSource(iconName, 16).isEmpty(),
            qPrintable(iconName));
    }
}

void IconThemeServiceTest::resolvesBareMetadataEmblemKeyword()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString configPath;
    configureSingleIconTheme(
        directory.path(),
        QStringLiteral("EmblemTheme"),
        QStringLiteral("emblem-favorite"),
        QColor(0x22, 0xaa, 0x66),
        &configPath);
    IconThemeService service(configPath);

    QVERIFY(!service.emblemIconSource(QStringLiteral("favorite"), 16).isEmpty());
}

void IconThemeServiceTest::semanticIconOverrideBeatsExactCustomFile()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString configPath;
    configureSingleIconTheme(
        directory.path(),
        QStringLiteral("RichTheme"),
        QStringLiteral("semantic-application"),
        QColor(0xdd, 0x33, 0x33),
        &configPath);
    const QString customPath = QDir(directory.path()).filePath(QStringLiteral("custom.png"));
    writeSolidImage(customPath, QSize(16, 16), QColor(0x33, 0x33, 0xdd));
    IconThemeService service(configPath);

    const QString source = service.richFileIconSource(
        QStringLiteral("/tmp/example.desktop"),
        false,
        false,
        16,
        QStringLiteral("semantic-application"),
        {},
        QUrl::fromLocalFile(customPath),
        QStringLiteral("1"));
    QVERIFY(source.startsWith(QStringLiteral("image://astrea-icons/theme/")));
    QCOMPARE(
        centerColor(renderThemeSource(service, source, QSize(16, 16))),
        QColor(0xdd, 0x33, 0x33));
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

void IconThemeServiceTest::themeExistsUsesIndexNotProbeIcons()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("NoProbe"));
    writeIcon(
        directory.path(),
        QStringLiteral("NoProbe/16x16/mimetypes"),
        QStringLiteral("custom-installed"),
        QColor(0xaa, 0x55, 0x33));
    writeSimpleTheme(directory.path(), QStringLiteral("Fallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("Fallback/16x16/mimetypes"),
        QStringLiteral("folder"),
        QColor(0x33, 0x55, 0xaa));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("Fallback"));

    FreedesktopIconThemeCatalog catalog;
    QVERIFY(catalog.themeExists(QStringLiteral("NoProbe")));
    QVERIFY(!catalog.themeExists(QStringLiteral("NoProbe-dark")));
}

void IconThemeServiceTest::listResolutionPrefersSelectedThemeCandidate()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("Selected"));
    writeIcon(
        directory.path(),
        QStringLiteral("Selected/16x16/mimetypes"),
        QStringLiteral("text-x-generic"),
        QColor(0xdd, 0x55, 0x55));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("PlatformFallback/16x16/mimetypes"),
        QStringLiteral("text-x-python"),
        QColor(0x55, 0x55, 0xdd));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconName(
            QStringLiteral("Selected"),
            {QStringLiteral("text-x-python"), QStringLiteral("text-x-generic")}),
        QStringLiteral("text-x-generic"));
}

void IconThemeServiceTest::listResolutionPrefersSpecificSelectedCandidate()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("Selected"));
    writeIcon(
        directory.path(),
        QStringLiteral("Selected/16x16/mimetypes"),
        QStringLiteral("text-x-python"),
        QColor(0xdd, 0x55, 0x55));
    writeIcon(
        directory.path(),
        QStringLiteral("Selected/16x16/mimetypes"),
        QStringLiteral("text-x-generic"),
        QColor(0x55, 0xdd, 0x55));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("PlatformFallback/16x16/mimetypes"),
        QStringLiteral("application-x-generic"),
        QColor(0x55, 0x55, 0xdd));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconName(
            QStringLiteral("Selected"),
            {QStringLiteral("text-x-python"), QStringLiteral("text-x-generic")}),
        QStringLiteral("text-x-python"));
}

void IconThemeServiceTest::resolvesExactInheritedParentAsset()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("Selected"),
        {QStringLiteral("16x16/mimetypes")},
        {QStringLiteral("ParentA"), QStringLiteral("ParentB")});
    writeSimpleTheme(directory.path(), QStringLiteral("ParentA"));
    writeSimpleTheme(directory.path(), QStringLiteral("ParentB"));
    writeIcon(
        directory.path(),
        QStringLiteral("ParentB/16x16/mimetypes"),
        QStringLiteral("shared-icon"),
        QColor(0x44, 0xaa, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});

    FreedesktopIconThemeCatalog catalog;
    const auto asset = catalog.resolveIconAsset(
        QStringLiteral("Selected"),
        {QStringLiteral("shared-icon")},
        QSize(16, 16),
        1.0);
    QCOMPARE(asset.themeName, QStringLiteral("ParentB"));
    QCOMPARE(asset.iconName, QStringLiteral("shared-icon"));
    QCOMPARE(
        asset.filePath,
        QDir(directory.path()).filePath(
            QStringLiteral("ParentB/16x16/mimetypes/shared-icon.png")));
}

void IconThemeServiceTest::rendersInheritedParentBeforeFallbackThroughService()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("Selected"),
        {QStringLiteral("16x16/mimetypes")},
        {QStringLiteral("ParentA"), QStringLiteral("ParentB")});
    writeSimpleTheme(directory.path(), QStringLiteral("ParentA"));
    writeSimpleTheme(directory.path(), QStringLiteral("ParentB"));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeSimpleTheme(directory.path(), QStringLiteral("hicolor"));
    writeIcon(
        directory.path(),
        QStringLiteral("ParentA/16x16/mimetypes"),
        QStringLiteral("shared-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("ParentB/16x16/mimetypes"),
        QStringLiteral("shared-icon"),
        QColor(0x44, 0x66, 0xaa));
    writeIcon(
        directory.path(),
        QStringLiteral("PlatformFallback/16x16/mimetypes"),
        QStringLiteral("shared-icon"),
        QColor(0xaa, 0x44, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("hicolor/16x16/mimetypes"),
        QStringLiteral("shared-icon"),
        QColor(0xaa, 0xaa, 0x44));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("Selected"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QImage image = service.renderIcon({QStringLiteral("shared-icon")}, QSize(16, 16));
    QCOMPARE(image.pixelColor(8, 8), QColor(0x44, 0xaa, 0x66));
}

void IconThemeServiceTest::rendersSelectedGenericBeforeFallbackSpecificThroughService()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("Selected"));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("Selected/16x16/mimetypes"),
        QStringLiteral("text-x-generic"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("PlatformFallback/16x16/mimetypes"),
        QStringLiteral("text-x-python"),
        QColor(0xaa, 0x44, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("Selected"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const QImage image = service.renderIcon(
        {QStringLiteral("text-x-python"), QStringLiteral("text-x-generic")},
        QSize(16, 16));
    QCOMPARE(image.pixelColor(8, 8), QColor(0x44, 0xaa, 0x66));
}

void IconThemeServiceTest::inheritedThemePrecedesPlatformFallback()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("Selected"), {QStringLiteral("16x16/mimetypes")}, {QStringLiteral("Parent")});
    writeSimpleTheme(directory.path(), QStringLiteral("Parent"));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("Parent/16x16/mimetypes"),
        QStringLiteral("inherited-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("PlatformFallback/16x16/mimetypes"),
        QStringLiteral("platform-icon"),
        QColor(0xaa, 0x44, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconName(
            QStringLiteral("Selected"),
            {QStringLiteral("platform-icon"), QStringLiteral("inherited-icon")}),
        QStringLiteral("inherited-icon"));
}

void IconThemeServiceTest::inheritedThemesFollowRecursiveDeclarationOrder()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("Selected"),
        {QStringLiteral("16x16/mimetypes")},
        {QStringLiteral("ParentA"), QStringLiteral("ParentB")});
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("ParentA"),
        {QStringLiteral("16x16/mimetypes")},
        {QStringLiteral("Grandchild")});
    writeSimpleTheme(directory.path(), QStringLiteral("Grandchild"));
    writeSimpleTheme(directory.path(), QStringLiteral("ParentB"));
    writeSimpleTheme(directory.path(), QStringLiteral("PlatformFallback"));
    writeIcon(
        directory.path(),
        QStringLiteral("Grandchild/16x16/mimetypes"),
        QStringLiteral("ordered-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("ParentB/16x16/mimetypes"),
        QStringLiteral("ordered-icon"),
        QColor(0xaa, 0x44, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("PlatformFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.themeFamily(QStringLiteral("Selected")),
        QStringList({
            QStringLiteral("Selected"),
            QStringLiteral("ParentA"),
            QStringLiteral("Grandchild"),
            QStringLiteral("ParentB"),
            QStringLiteral("PlatformFallback"),
        }));
    QCOMPARE(
        catalog.resolveIconName(
            QStringLiteral("Selected"),
            {QStringLiteral("ordered-icon")}),
        QStringLiteral("ordered-icon"));
}

void IconThemeServiceTest::hicolorIsAlwaysLastThemedFallback()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("Selected"),
        {QStringLiteral("16x16/mimetypes")},
        {QStringLiteral("hicolor"), QStringLiteral("Other")});
    writeSimpleTheme(directory.path(), QStringLiteral("Other"));
    writeSimpleTheme(directory.path(), QStringLiteral("hicolor"));
    writeIcon(
        directory.path(),
        QStringLiteral("Other/16x16/mimetypes"),
        QStringLiteral("other-only"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("hicolor/16x16/mimetypes"),
        QStringLiteral("hicolor-only"),
        QColor(0xaa, 0x44, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("MissingFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.themeFamily(QStringLiteral("Selected")),
        QStringList({QStringLiteral("Selected"), QStringLiteral("Other"), QStringLiteral("hicolor")}));
    QCOMPARE(
        catalog.resolveIconName(
            QStringLiteral("Selected"),
            {QStringLiteral("hicolor-only"), QStringLiteral("other-only")}),
        QStringLiteral("other-only"));
}

void IconThemeServiceTest::inheritanceCyclesTerminate()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("CycleA"), {QStringLiteral("16x16/mimetypes")}, {QStringLiteral("CycleB")});
    writeSimpleTheme(directory.path(), QStringLiteral("CycleB"), {QStringLiteral("16x16/mimetypes")}, {QStringLiteral("CycleA")});
    writeIcon(
        directory.path(),
        QStringLiteral("CycleB/16x16/mimetypes"),
        QStringLiteral("cycle-icon"),
        QColor(0x44, 0xaa, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setFallbackThemeName(QStringLiteral("MissingFallback"));

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.themeFamily(QStringLiteral("CycleA")),
        QStringList({QStringLiteral("CycleA"), QStringLiteral("CycleB")}));
    QCOMPARE(
        catalog.resolveIconName(QStringLiteral("CycleA"), {QStringLiteral("cycle-icon")}),
        QStringLiteral("cycle-icon"));
}

void IconThemeServiceTest::splitThemeRootsUseFirstMetadataAndAllAssets()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;
    QVERIFY(firstRoot.isValid());
    QVERIFY(secondRoot.isValid());
    writeSimpleTheme(firstRoot.path(), QStringLiteral("Layered"));
    writeSimpleTheme(
        secondRoot.path(),
        QStringLiteral("Layered"),
        {QStringLiteral("32x32/mimetypes")});
    writeIcon(
        secondRoot.path(),
        QStringLiteral("Layered/16x16/mimetypes"),
        QStringLiteral("from-second-root"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        secondRoot.path(),
        QStringLiteral("Layered/32x32/mimetypes"),
        QStringLiteral("not-declared"),
        QColor(0xaa, 0x44, 0x66));
    QIcon::setThemeSearchPaths({firstRoot.path(), secondRoot.path()});

    FreedesktopIconThemeCatalog catalog;
    QVERIFY(catalog.themeExists(QStringLiteral("Layered")));
    QCOMPARE(
        catalog.resolveIconName(QStringLiteral("Layered"), {QStringLiteral("from-second-root")}),
        QStringLiteral("from-second-root"));
    QCOMPARE(
        catalog.resolveIconName(QStringLiteral("Layered"), {QStringLiteral("not-declared")}),
        QString());
}

void IconThemeServiceTest::scaledDirectoriesParticipateInPresence()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(
        directory.path(),
        QStringLiteral("Scaled"),
        {QStringLiteral("16x16/mimetypes")},
        {},
        {QStringLiteral("scalable/mimetypes")});
    writeIcon(
        directory.path(),
        QStringLiteral("Scaled/scalable/mimetypes"),
        QStringLiteral("scaled-only"),
        QColor(0x44, 0xaa, 0x66));
    QIcon::setThemeSearchPaths({directory.path()});

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconName(QStringLiteral("Scaled"), {QStringLiteral("scaled-only")}),
        QStringLiteral("scaled-only"));
}

void IconThemeServiceTest::selectsDeclaredDirectoryBySizeAndScale()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray index = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Size Theme\n"
        "Comment=Theme used by deterministic tests\n"
        "Directories=16x16/mimetypes,32x32/mimetypes,scalable/mimetypes,16x16@2/mimetypes,defaulted/mimetypes\n"
        "\n"
        "[16x16/mimetypes]\nSize=16\nType=Fixed\n\n"
        "[32x32/mimetypes]\nSize=32\nType=Fixed\n\n"
        "[scalable/mimetypes]\nSize=24\nMinSize=16\nMaxSize=64\nType=Scalable\n\n"
        "[16x16@2/mimetypes]\nSize=16\nScale=2\nType=Fixed\n\n"
        "[defaulted/mimetypes]\nSize=16\n");
    writeFile(QDir(directory.path()).filePath(QStringLiteral("Sized/index.theme")), index);
    writeIcon(
        directory.path(),
        QStringLiteral("Sized/16x16/mimetypes"),
        QStringLiteral("size-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("Sized/32x32/mimetypes"),
        QStringLiteral("size-icon"),
        QColor(0xaa, 0x44, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("Sized/scalable/mimetypes"),
        QStringLiteral("size-icon"),
        QColor(0x44, 0x66, 0xaa));
    writeIcon(
        directory.path(),
        QStringLiteral("Sized/16x16@2/mimetypes"),
        QStringLiteral("size-icon"),
        QColor(0xaa, 0xaa, 0x44));
    writeIcon(
        directory.path(),
        QStringLiteral("Sized/defaulted/mimetypes"),
        QStringLiteral("default-icon"),
        QColor(0xaa, 0x44, 0xaa));
    QIcon::setThemeSearchPaths({directory.path()});

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconAsset(QStringLiteral("Sized"), {QStringLiteral("size-icon")}, QSize(16, 16), 1.0)
            .filePath,
        QDir(directory.path()).filePath(QStringLiteral("Sized/16x16/mimetypes/size-icon.png")));
    QCOMPARE(
        catalog.resolveIconAsset(QStringLiteral("Sized"), {QStringLiteral("size-icon")}, QSize(24, 24), 1.0)
            .filePath,
        QDir(directory.path()).filePath(QStringLiteral("Sized/scalable/mimetypes/size-icon.png")));
    QCOMPARE(
        catalog.resolveIconAsset(QStringLiteral("Sized"), {QStringLiteral("size-icon")}, QSize(16, 16), 2.0)
            .filePath,
        QDir(directory.path()).filePath(QStringLiteral("Sized/16x16@2/mimetypes/size-icon.png")));
    QCOMPARE(
        catalog.resolveIconAsset(QStringLiteral("Sized"), {QStringLiteral("default-icon")}, QSize(18, 18), 1.0)
            .filePath,
        QDir(directory.path()).filePath(QStringLiteral("Sized/defaulted/mimetypes/default-icon.png")));
}

void IconThemeServiceTest::closestDirectoryDistanceIgnoresScalePriority()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray index = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Distance Theme\n"
        "Comment=Theme used by deterministic tests\n"
        "Directories=huge-scale,small-scale,same-scale,near-scale\n"
        "\n"
        "[huge-scale]\nSize=512\nScale=2\nType=Fixed\n\n"
        "[small-scale]\nSize=16\nScale=1\nType=Fixed\n\n"
        "[same-scale]\nSize=48\nScale=2\nType=Fixed\n\n"
        "[near-scale]\nSize=16\nScale=1\nType=Fixed\n");
    writeFile(QDir(directory.path()).filePath(QStringLiteral("Distance/index.theme")), index);
    writeIcon(
        directory.path(),
        QStringLiteral("Distance/huge-scale"),
        QStringLiteral("distance-icon"),
        QColor(0xaa, 0x44, 0x44));
    writeIcon(
        directory.path(),
        QStringLiteral("Distance/small-scale"),
        QStringLiteral("distance-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeIcon(
        directory.path(),
        QStringLiteral("Distance/same-scale"),
        QStringLiteral("realistic-distance-icon"),
        QColor(0xaa, 0x44, 0xaa));
    writeIcon(
        directory.path(),
        QStringLiteral("Distance/near-scale"),
        QStringLiteral("realistic-distance-icon"),
        QColor(0x44, 0x66, 0xaa));
    QIcon::setThemeSearchPaths({directory.path()});

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconAsset(
            QStringLiteral("Distance"),
            {QStringLiteral("distance-icon")},
            QSize(16, 16),
            2.0)
            .filePath,
        QDir(directory.path()).filePath(QStringLiteral("Distance/small-scale/distance-icon.png")));
    QCOMPARE(
        catalog.resolveIconAsset(
            QStringLiteral("Distance"),
            {QStringLiteral("realistic-distance-icon")},
            QSize(16, 16),
            2.0)
            .filePath,
        QDir(directory.path()).filePath(
            QStringLiteral("Distance/near-scale/realistic-distance-icon.png")));
}

void IconThemeServiceTest::declaredDirectoryOrderPrecedesRootOrder()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;
    QVERIFY(firstRoot.isValid());
    QVERIFY(secondRoot.isValid());
    const QByteArray index = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Split Order Theme\n"
        "Comment=Theme used by deterministic tests\n"
        "Directories=preferred,secondary\n"
        "\n"
        "[preferred]\nSize=16\nType=Fixed\n\n"
        "[secondary]\nSize=16\nType=Fixed\n");
    writeFile(QDir(firstRoot.path()).filePath(QStringLiteral("SplitOrder/index.theme")), index);
    writeIcon(
        firstRoot.path(),
        QStringLiteral("SplitOrder/secondary"),
        QStringLiteral("split-icon"),
        QColor(0xaa, 0x44, 0x44));
    writeIcon(
        secondRoot.path(),
        QStringLiteral("SplitOrder/preferred"),
        QStringLiteral("split-icon"),
        QColor(0x44, 0xaa, 0x66));

    const QByteArray sameDirectoryIndex = QByteArrayLiteral(
        "[Icon Theme]\n"
        "Name=Same Directory Theme\n"
        "Comment=Theme used by deterministic tests\n"
        "Directories=preferred\n"
        "\n"
        "[preferred]\nSize=16\nType=Fixed\n");
    writeFile(
        QDir(firstRoot.path()).filePath(QStringLiteral("SplitSame/index.theme")),
        sameDirectoryIndex);
    writeIcon(
        firstRoot.path(),
        QStringLiteral("SplitSame/preferred"),
        QStringLiteral("split-icon"),
        QColor(0x44, 0x66, 0xaa));
    writeIcon(
        secondRoot.path(),
        QStringLiteral("SplitSame/preferred"),
        QStringLiteral("split-icon"),
        QColor(0xaa, 0xaa, 0x44));
    QIcon::setThemeSearchPaths({firstRoot.path(), secondRoot.path()});

    FreedesktopIconThemeCatalog catalog;
    QCOMPARE(
        catalog.resolveIconAsset(
            QStringLiteral("SplitOrder"),
            {QStringLiteral("split-icon")},
            QSize(16, 16),
            1.0)
            .filePath,
        QDir(secondRoot.path()).filePath(QStringLiteral("SplitOrder/preferred/split-icon.png")));
    QCOMPARE(
        catalog.resolveIconAsset(
            QStringLiteral("SplitSame"),
            {QStringLiteral("split-icon")},
            QSize(16, 16),
            1.0)
            .filePath,
        QDir(firstRoot.path()).filePath(QStringLiteral("SplitSame/preferred/split-icon.png")));
}

void IconThemeServiceTest::exactThemeFormatsAgreeWithQtSupport()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeSimpleTheme(directory.path(), QStringLiteral("Formats"));
    writeIcon(
        directory.path(),
        QStringLiteral("Formats/16x16/mimetypes"),
        QStringLiteral("png-icon"),
        QColor(0x44, 0xaa, 0x66));
    writeXpmIcon(
        directory.path(),
        QStringLiteral("Formats/16x16/mimetypes"),
        QStringLiteral("xpm-icon"),
        QColor(0xaa, 0x44, 0x66));
    writeSvgIcon(
        directory.path(),
        QStringLiteral("Formats/16x16/mimetypes"),
        QStringLiteral("svg-icon"),
        QColor(0x44, 0x66, 0xaa));
    writeFile(
        QDir(directory.path()).filePath(QStringLiteral("Formats/16x16/mimetypes/svgz-icon.svgz")),
        QByteArrayLiteral("not a supported deterministic svgz asset"));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath, QStringLiteral("Formats"));
    qunsetenv("ASTREA_ICON_THEME");
    IconThemeService service(configPath);

    const auto supports = [](const QByteArray &format) {
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        return std::any_of(
            formats.cbegin(),
            formats.cend(),
            [&format](const QByteArray &supported) {
                return supported.compare(format, Qt::CaseInsensitive) == 0;
            });
    };

    const auto pngAsset = service.renderIcon({QStringLiteral("png-icon")}, QSize(16, 16));
    QCOMPARE(pngAsset.pixelColor(8, 8), QColor(0x44, 0xaa, 0x66));

    if (supports(QByteArrayLiteral("xpm"))) {
        const auto xpmAsset = service.renderIcon({QStringLiteral("xpm-icon")}, QSize(16, 16));
        QCOMPARE(xpmAsset.pixelColor(8, 8), QColor(0xaa, 0x44, 0x66));
    }
    if (supports(QByteArrayLiteral("svg"))) {
        const auto svgAsset = service.renderIcon({QStringLiteral("svg-icon")}, QSize(16, 16));
        QCOMPARE(svgAsset.pixelColor(8, 8), QColor(0x44, 0x66, 0xaa));
    }

    FreedesktopIconThemeCatalog catalog;
    QVERIFY(catalog.resolveIconAsset(
                 QStringLiteral("Formats"),
                 {QStringLiteral("svgz-icon")},
                 QSize(16, 16),
                 1.0)
                .filePath
                .isEmpty());
}

void IconThemeServiceTest::themeAssetChangesInvalidateRenderedResults()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    replaceIconAtomically(
        directory.path(),
        QStringLiteral("ThemeA/16x16/mimetypes"),
        QStringLiteral("asset-icon"),
        QColor(0x33, 0xaa, 0x77));
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("ui/theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const quint64 previousRevision = service.revision();
    const QImage previousImage = service.renderIcon(
        {QStringLiteral("asset-icon")},
        QSize(16, 16));
    QCOMPARE(previousImage.pixelColor(8, 8), QColor(0x33, 0xaa, 0x77));
    const QString previousSource = service.iconSourceForNames({QStringLiteral("asset-icon")}, 16);

    replaceIconAtomically(
        directory.path(),
        QStringLiteral("ThemeA/16x16/mimetypes"),
        QStringLiteral("asset-icon"),
        QColor(0x55, 0x66, 0xdd));

    QTRY_VERIFY_WITH_TIMEOUT(service.revision() > previousRevision, 3000);
    const QImage updatedImage = service.renderIcon(
        {QStringLiteral("asset-icon")},
        QSize(16, 16));
    QCOMPARE(updatedImage.pixelColor(8, 8), QColor(0x55, 0x66, 0xdd));
    QVERIFY(service.iconSourceForNames({QStringLiteral("asset-icon")}, 16) != previousSource);
}

void IconThemeServiceTest::installingPreferredVariantInvalidatesTopology()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("ui/theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ThemeA")},
        {QStringLiteral("theme"), QStringLiteral("dark")},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
    const quint64 previousRevision = service.revision();
    writeTheme(directory.path(), QStringLiteral("ThemeA-dark"), QColor(0x55, 0x55, 0xdd), false);

    QTRY_COMPARE_WITH_TIMEOUT(service.effectiveTheme(), QStringLiteral("ThemeA-dark"), 3000);
    QVERIFY(service.revision() > previousRevision);
}

void IconThemeServiceTest::installingCompatibilityVariantInvalidatesTopology()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("MacTahoe"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("ui/theme.json"));
    writeThemeConfigObject(configPath, QJsonObject {
        {QStringLiteral("theme"), QStringLiteral("dark")},
    });
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("MacTahoe"));
    const quint64 previousRevision = service.revision();
    writeTheme(directory.path(), QStringLiteral("MacTahoe-dark"), QColor(0x55, 0x55, 0xdd), false);

    QTRY_COMPARE_WITH_TIMEOUT(service.effectiveTheme(), QStringLiteral("MacTahoe-dark"), 3000);
    QVERIFY(service.revision() > previousRevision);
}

void IconThemeServiceTest::unrelatedThemeDoesNotInvalidateTopology()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeTheme(directory.path(), QStringLiteral("ThemeA"), QColor(0xdd, 0x55, 0x55), false);
    QIcon::setThemeSearchPaths({directory.path()});
    QIcon::setThemeName(QStringLiteral("MissingPlatformTheme"));
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("ui/theme.json"));
    writeThemeConfig(configPath, QStringLiteral("ThemeA"));
    qunsetenv("ASTREA_ICON_THEME");

    IconThemeService service(configPath);
    const quint64 previousRevision = service.revision();
    QSignalSpy themeChanges(&service, &IconThemeService::themeChanged);
    writeTheme(directory.path(), QStringLiteral("UnrelatedTheme"), QColor(0x55, 0x55, 0xdd), false);

    QTest::qWait(500);
    QCOMPARE(service.effectiveTheme(), QStringLiteral("ThemeA"));
    QCOMPARE(service.revision(), previousRevision);
    QCOMPARE(themeChanges.count(), 0);
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    IconThemeServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_icon_theme_service.moc"
