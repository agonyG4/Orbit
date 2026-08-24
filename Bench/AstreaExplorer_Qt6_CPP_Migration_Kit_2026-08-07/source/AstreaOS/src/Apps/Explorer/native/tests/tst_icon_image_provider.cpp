#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/astrea_icon_image_provider.h"
#include "services/icon_theme_service.h"

using Astrea::Explorer::Native::Runtime::AstreaIconImageProvider;
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

void writeProviderTheme(const QString &root)
{
    const QString theme = QDir(root).filePath(QStringLiteral("ProviderTheme"));
    writeFile(
        QDir(theme).filePath(QStringLiteral("index.theme")),
        QByteArrayLiteral(
            "[Icon Theme]\nName=Provider Theme\nDirectories=16x16/mimetypes,mimetypes/symbolic\n\n"
            "[16x16/mimetypes]\nSize=16\nContext=MimeTypes\nType=Fixed\n\n"
            "[mimetypes/symbolic]\nSize=16\nMinSize=16\nMaxSize=512\nContext=MimeTypes\nType=Scalable\n"));
    const QString iconPath = QDir(theme).filePath(QStringLiteral("16x16/mimetypes/test-mime.png"));
    QVERIFY2(QDir().mkpath(QFileInfo(iconPath).absolutePath()), qPrintable(iconPath));
    QImage image(16, 16, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0x33, 0xaa, 0x77));
    QVERIFY(image.save(iconPath, "PNG"));
    QVERIFY(image.save(QDir(theme).filePath(QStringLiteral("16x16/mimetypes/application-x-generic.png")), "PNG"));
    const QString symbolicPath = QDir(theme).filePath(QStringLiteral("mimetypes/symbolic/test-mime-symbolic.png"));
    QVERIFY2(QDir().mkpath(QFileInfo(symbolicPath).absolutePath()), qPrintable(symbolicPath));
    QImage symbolic(16, 16, QImage::Format_ARGB32_Premultiplied);
    symbolic.fill(Qt::transparent);
    for (int y = 4; y < 12; ++y) {
        for (int x = 4; x < 12; ++x) {
            symbolic.setPixelColor(x, y, QColor(0xdd, 0x44, 0xbb));
        }
    }
    QVERIFY(symbolic.save(symbolicPath, "PNG"));
}

void writeThemeConfig(const QString &path)
{
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray data = QJsonDocument(QJsonObject {
        {QStringLiteral("desktop_icon_theme"), QStringLiteral("ProviderTheme")}})
        .toJson(QJsonDocument::Compact);
    QCOMPARE(file.write(data), data.size());
    QVERIFY(file.commit());
}

} // namespace

class IconImageProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void rendersEncodedCandidateSynchronously();
    void rendersSymbolicCandidateFromUrl();
    void alwaysReturnsBuiltInFallbackForMissingCandidates();
};

void IconImageProviderTest::rendersEncodedCandidateSynchronously()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeProviderTheme(directory.path());
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath);
    qunsetenv("ASTREA_ICON_THEME");
    IconThemeService service(configPath);
    AstreaIconImageProvider provider(&service);

    QSize requested;
    const QString id = QStringLiteral("theme/") + QString::fromLatin1(
        QUrl::toPercentEncoding(QStringLiteral("test-mime")));
    const QImage image = provider.requestImage(id, &requested, QSize(16, 16));
    QVERIFY(!image.isNull());
    QCOMPARE(requested, QSize(16, 16));
    QCOMPARE(image.pixelColor(8, 8), QColor(0x33, 0xaa, 0x77));
}

void IconImageProviderTest::rendersSymbolicCandidateFromUrl()
{
    ThemeSearchPathGuard guard;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeProviderTheme(directory.path());
    QIcon::setThemeSearchPaths({directory.path()});
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath);
    qunsetenv("ASTREA_ICON_THEME");
    IconThemeService service(configPath);
    AstreaIconImageProvider provider(&service);

    QSize requested;
    const QString id = QStringLiteral("theme/") + QString::fromLatin1(
        QUrl::toPercentEncoding(QStringLiteral("test-mime-symbolic")));
    const QImage image = provider.requestImage(id, &requested, QSize(16, 16));
    QVERIFY(!image.isNull());
    QCOMPARE(image.pixelColor(8, 8), QColor(0xdd, 0x44, 0xbb));
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
}

void IconImageProviderTest::alwaysReturnsBuiltInFallbackForMissingCandidates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = QDir(directory.path()).filePath(QStringLiteral("theme.json"));
    writeThemeConfig(configPath);
    IconThemeService service(configPath);
    AstreaIconImageProvider provider(&service);

    QSize requested;
    const QString id = QStringLiteral("theme/") + QString::fromLatin1(
        QUrl::toPercentEncoding(QStringLiteral("not-installed")));
    const QImage image = provider.requestImage(id, &requested, QSize(20, 20));
    QVERIFY(!image.isNull());
    QCOMPARE(requested, QSize(20, 20));
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    IconImageProviderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_icon_image_provider.moc"
