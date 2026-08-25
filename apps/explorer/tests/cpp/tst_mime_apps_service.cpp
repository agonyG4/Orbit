#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "services/mime_apps_service.h"

using Astrea::Explorer::Native::Services::MimeAppsService;
using Astrea::Explorer::Native::Services::XdgPaths;

namespace {

XdgPaths testPaths(const QTemporaryDir &fixture)
{
    XdgPaths paths = XdgPaths::fromEnvironment();
    paths.home = fixture.path();
    paths.configHome = QDir(fixture.path()).filePath(QStringLiteral("config"));
    paths.configDirs.clear();
    paths.dataHome = QDir(fixture.path()).filePath(QStringLiteral("data"));
    paths.dataDirs.clear();
    QDir().mkpath(QDir(paths.dataHome).filePath(QStringLiteral("applications")));
    return paths;
}

void writeDesktop(const XdgPaths &paths, const QString &id)
{
    const QString applications = QDir(paths.dataHome).filePath(QStringLiteral("applications"));
    QDir().mkpath(applications);
    QFile file(QDir(applications).filePath(id));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Desktop Entry]\nType=Application\nName=Test\nExec=test\n")) > 0);
}

void writeDesktopAt(const QString &applications, const QString &id)
{
    QDir().mkpath(applications);
    QFile file(QDir(applications).filePath(id));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Desktop Entry]\nType=Application\nName=Test\nExec=test\n")) > 0);
}

} // namespace

class MimeAppsServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsLiteralMimeKeysAndLists();
    void updatesDefaultAndAssociationsAtomically();
    void preservesUnrelatedAndMalformedContent();
    void emptyFileRoundTripsThroughFreshService();
    void resolvesDesktopSpecificXdgFilesWithUserPrecedenceAndFallback();
    void resolvesAllCurrentDesktopComponentsAndBaseMimeTypes();
};

void MimeAppsServiceTest::readsLiteralMimeKeysAndLists()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const XdgPaths paths = testPaths(fixture);
    writeDesktop(paths, QStringLiteral("org.example.Editor.desktop"));
    writeDesktop(paths, QStringLiteral("org.example.Fallback.desktop"));
    writeDesktop(paths, QStringLiteral("org.example.Pdf.desktop"));
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.example.Editor.desktop;org.example.Fallback.desktop;\n"
        "application/pdf=org.example.Pdf.desktop;\n"
        "\n"
        "[Added Associations]\n"
        "text/plain=org.example.Editor.desktop;\n")) > 0);
    file.close();

    MimeAppsService service(path, 5000, paths);
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("org.example.Fallback.desktop")}));
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("application/pdf")),
        QStringList({QStringLiteral("org.example.Pdf.desktop")}));
    QCOMPARE(
        service.associationsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("org.example.Fallback.desktop")}));
}

void MimeAppsServiceTest::updatesDefaultAndAssociationsAtomically()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const XdgPaths paths = testPaths(fixture);
    writeDesktop(paths, QStringLiteral("org.example.Editor.desktop"));
    writeDesktop(paths, QStringLiteral("org.example.Fallback.desktop"));
    writeDesktop(paths, QStringLiteral("org.example.Old.desktop"));
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.example.Fallback.desktop;org.example.Old.desktop;\n"
        "\n"
        "[Added Associations]\n"
        "text/plain=org.example.Old.desktop;\n")) > 0);
    file.close();

    MimeAppsService service(path, 5000, paths);
    QVERIFY(service.setDefault(
        QStringLiteral("text/plain"),
        QStringLiteral("org.example.Editor")));

    QCOMPARE(
        service.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("org.example.Fallback.desktop"),
                     QStringLiteral("org.example.Old.desktop")}));
    const QStringList associations = service.associationsForMime(QStringLiteral("text/plain"));
    QVERIFY(associations.contains(QStringLiteral("org.example.Editor.desktop")));
    QVERIFY(associations.contains(QStringLiteral("org.example.Old.desktop")));
    QVERIFY(!associations.contains(QStringLiteral("org.example.Editor")));

    MimeAppsService fresh(path, 5000, paths);
    QCOMPARE(fresh.defaultsForMime(QStringLiteral("text/plain")).constFirst(),
             QStringLiteral("org.example.Editor.desktop"));
}

void MimeAppsServiceTest::preservesUnrelatedAndMalformedContent()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const XdgPaths paths = testPaths(fixture);
    writeDesktop(paths, QStringLiteral("org.example.Pdf.desktop"));
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArrayLiteral(
        "# keep this comment\n"
        "malformed line\n"
        "[Default Applications]\n"
        "text/plain=org.example.Old.desktop;;\n"
        "not-an-assignment\n"
        "[Unrelated]\n"
        "answer=42\n")) > 0);
    file.close();

    MimeAppsService service(path, 5000, paths);
    QVERIFY(service.setDefault(
        QStringLiteral("application/pdf"),
        QStringLiteral("org.example.Pdf.desktop")));

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("# keep this comment")));
    QVERIFY(contents.contains(QStringLiteral("malformed line")));
    QVERIFY(contents.contains(QStringLiteral("[Unrelated]")));
    QVERIFY(contents.contains(QStringLiteral("answer=42")));
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("application/pdf")),
        QStringList({QStringLiteral("org.example.Pdf.desktop")}));
}

void MimeAppsServiceTest::emptyFileRoundTripsThroughFreshService()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const XdgPaths paths = testPaths(fixture);
    writeDesktop(paths, QStringLiteral("org.example.Editor.desktop"));
    const QString path = fixture.filePath(QStringLiteral("mimeapps.list"));
    MimeAppsService service(path, 5000, paths);
    QVERIFY(service.setDefault(
        QStringLiteral("text/plain"),
        QStringLiteral("org.example.Editor.desktop")));

    MimeAppsService fresh(path, 5000, paths);
    QCOMPARE(
        fresh.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.example.Editor.desktop")}));
    QVERIFY(fresh.associationsForMime(QStringLiteral("text/plain"))
                .contains(QStringLiteral("org.example.Editor.desktop")));
}

void MimeAppsServiceTest::resolvesDesktopSpecificXdgFilesWithUserPrecedenceAndFallback()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    XdgPaths paths = testPaths(fixture);
    paths.currentDesktop = QStringLiteral("AstreaTest");
    const QString systemConfig = fixture.filePath(QStringLiteral("system-config"));
    const QString systemApplications = fixture.filePath(QStringLiteral("system-data/applications"));
    paths.configDirs = {systemConfig};
    paths.dataDirs = {fixture.filePath(QStringLiteral("system-data"))};
    writeDesktop(paths, QStringLiteral("org.user.Editor.desktop"));
    writeDesktopAt(systemApplications, QStringLiteral("org.system.Viewer.desktop"));

    QVERIFY(QDir().mkpath(systemConfig));
    QFile systemMime(QDir(systemConfig).filePath(QStringLiteral("mimeapps.list")));
    QVERIFY(systemMime.open(QIODevice::WriteOnly));
    QVERIFY(systemMime.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.system.Viewer.desktop;\n"
        "[Added Associations]\n"
        "text/plain=org.system.Viewer.desktop;\n")) > 0);
    systemMime.close();

    QVERIFY(QDir().mkpath(paths.configHome));
    QFile userMime(QDir(paths.configHome).filePath(QStringLiteral("astreatest-mimeapps.list")));
    QVERIFY(userMime.open(QIODevice::WriteOnly));
    QVERIFY(userMime.write(QByteArrayLiteral(
        "[Default Applications]\n"
        "text/plain=org.invalid.Missing.desktop;\n"
        "[Added Associations]\n"
        "text/plain=org.user.Editor.desktop;\n"
        "[Removed Associations]\n"
        "text/plain=org.system.Viewer.desktop;\n")) > 0);
    userMime.close();

    MimeAppsService service({}, 5000, paths);
    QCOMPARE(
        service.defaultsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.user.Editor.desktop")}));
    QCOMPARE(
        service.associationsForMime(QStringLiteral("text/plain")),
        QStringList({QStringLiteral("org.user.Editor.desktop")}));
    QCOMPARE(
        service.filePath(),
        QDir(paths.configHome).filePath(QStringLiteral("mimeapps.list")));
}

void MimeAppsServiceTest::resolvesAllCurrentDesktopComponentsAndBaseMimeTypes()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    XdgPaths paths = testPaths(fixture);
    paths.currentDesktop = QStringLiteral("GNOME:Astrea:GNOME");
    writeDesktop(paths, QStringLiteral("org.example.Base.desktop"));
    const QString basePath = QDir(paths.dataHome).filePath(
        QStringLiteral("applications/org.example.Base.desktop"));
    QFile base(basePath);
    QVERIFY(base.open(QIODevice::WriteOnly));
    QVERIFY(base.write(QByteArrayLiteral(
        "[Desktop Entry]\nType=Application\nName=Base\nExec=base\nMimeType=text/plain;\n")) > 0);
    base.close();

    QVERIFY(QDir().mkpath(paths.configHome));
    QFile astrea(QDir(paths.configHome).filePath(QStringLiteral("astrea-mimeapps.list")));
    QVERIFY(astrea.open(QIODevice::WriteOnly));
    QVERIFY(astrea.write(QByteArrayLiteral(
        "[Added Associations]\ntext/plain=org.example.Base.desktop;\n")) > 0);
    astrea.close();

    const QStringList names = paths.desktopNames();
    QCOMPARE(names, QStringList({QStringLiteral("gnome"), QStringLiteral("astrea")}));
    const QStringList search = paths.mimeAppsSearchPaths();
    QVERIFY(search.indexOf(QDir(paths.configHome).filePath(QStringLiteral("gnome-mimeapps.list")))
            < search.indexOf(QDir(paths.configHome).filePath(QStringLiteral("astrea-mimeapps.list"))));

    MimeAppsService service({}, 5000, paths);
    QCOMPARE(service.associationsForMime(QStringLiteral("text/plain")),
             QStringList({QStringLiteral("org.example.Base.desktop")}));
}

QTEST_GUILESS_MAIN(MimeAppsServiceTest)

#include "tst_mime_apps_service.moc"
