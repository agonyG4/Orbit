#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>
#include <QtTest>

#include "services/launch_service.h"

using namespace Astrea::Explorer::Native::Services;

class LaunchServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsFileLaunchArgv();
    void buildsDesktopLaunchArgv();
    void buildsDesktopLaunchArgvWithOrderedTargets();
    void forwardsTargetsThroughAstreaLaunchToFinalArgv();
    void buildsWindowsLaunchArgv();
    void preservesShellCharactersAsOneArgument();
    void rejectsEmptyLaunchPaths();
    void rejectsMissingAbsoluteLauncherAtLaunchTime();
};

void LaunchServiceTest::buildsFileLaunchArgv()
{
    LaunchService service(
        QStringLiteral("/opt/Astrea/bin/astrea-launch"),
        QStringLiteral("/opt/Astrea/System/scripts/astrea-windows-run"));

    const LaunchSpec spec = service.fileLaunch(QStringLiteral("/tmp/space name.txt"));
    QCOMPARE(spec.program, QStringLiteral("/opt/Astrea/bin/astrea-launch"));
    QCOMPARE(spec.arguments, QStringList({QStringLiteral("--file"), QStringLiteral("/tmp/space name.txt")}));
}

void LaunchServiceTest::buildsDesktopLaunchArgv()
{
    LaunchService service(QStringLiteral("launcher"), QStringLiteral("windows-run"));

    const LaunchSpec spec = service.desktopLaunch(QStringLiteral("/tmp/My App.desktop"));
    QCOMPARE(spec.program, QStringLiteral("launcher"));
    QCOMPARE(spec.arguments, QStringList({QStringLiteral("--desktop"), QStringLiteral("/tmp/My App.desktop")}));
}

void LaunchServiceTest::buildsDesktopLaunchArgvWithOrderedTargets()
{
    LaunchService service(QStringLiteral("launcher"), QStringLiteral("windows-run"));

    const LaunchSpec spec = service.desktopLaunch(
        QStringLiteral("org.example.Editor.desktop"),
        {QStringLiteral("/tmp/one file.txt"), QStringLiteral("/tmp/two.txt")},
        {QStringLiteral("https://example.test/item")});
    QCOMPARE(
        spec.arguments,
        QStringList({QStringLiteral("--desktop"), QStringLiteral("org.example.Editor.desktop"),
                     QStringLiteral("--file"), QStringLiteral("/tmp/one file.txt"),
                     QStringLiteral("--file"), QStringLiteral("/tmp/two.txt"),
                     QStringLiteral("--url"), QStringLiteral("https://example.test/item")}));
}

void LaunchServiceTest::forwardsTargetsThroughAstreaLaunchToFinalArgv()
{
#ifndef ASTREA_LAUNCH_ARTIFACT
    QSKIP("Astrea launch artifact is not available");
#else
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString dataHome = QDir(fixture.path()).filePath(QStringLiteral("data"));
    const QString applications = QDir(dataHome).filePath(QStringLiteral("applications"));
    const QString configDir = QDir(fixture.path()).filePath(QStringLiteral("config/AstreaOS/system"));
    QVERIFY(QDir().mkpath(applications));
    QVERIFY(QDir().mkpath(configDir));
    const QString recorder = QDir(fixture.path()).filePath(QStringLiteral("recorder"));
    const QString output = QDir(fixture.path()).filePath(QStringLiteral("argv.txt"));
    {
        QFile file(recorder);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QByteArrayLiteral(
            "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$ASTREA_RECORDER_OUTPUT\"\n")) > 0);
        file.close();
        QVERIFY(file.setPermissions(QFileDevice::ExeOwner | QFileDevice::ReadOwner
                                    | QFileDevice::WriteOwner));
    }
    {
        QFile file(QDir(applications).filePath(QStringLiteral("org.example.Editor.desktop")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=Editor\nExec=%1 %F\n")
                              .arg(recorder)
                              .toUtf8()) > 0);
    }
    {
        QFile file(QDir(configDir).filePath(QStringLiteral("launch.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QByteArrayLiteral(
            "{\"isolate_launches\":false,\"latency\":{\"enabled\":false}}")) > 0);
    }

    const QString first = QDir(fixture.path()).filePath(QStringLiteral("one file.txt"));
    const QString second = QDir(fixture.path()).filePath(QStringLiteral("two.txt"));
    LaunchService service(QStringLiteral(ASTREA_LAUNCH_ARTIFACT), QStringLiteral("windows-run"));
    const LaunchSpec spec = service.desktopLaunch(
        QStringLiteral("org.example.Editor.desktop"),
        {first, second});
    QVERIFY(spec.isValid());

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("HOME"), fixture.path());
    environment.insert(QStringLiteral("XDG_DATA_HOME"), dataHome);
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), QDir(fixture.path()).filePath(QStringLiteral("config")));
    environment.insert(QStringLiteral("XDG_STATE_HOME"), QDir(fixture.path()).filePath(QStringLiteral("state")));
    environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), QDir(fixture.path()).filePath(QStringLiteral("runtime")));
    environment.insert(QStringLiteral("ASTREA_RECORDER_OUTPUT"), output);
    environment.insert(QStringLiteral("PATH"), fixture.path());
    process.setProcessEnvironment(environment);
    process.start(spec.program, spec.arguments);
    QVERIFY2(process.waitForFinished(5000), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(output), 2000);
    QFile recorded(output);
    QVERIFY(recorded.open(QIODevice::ReadOnly));
    QCOMPARE(
        QString::fromUtf8(recorded.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
        QStringList({first, second}));
#endif
}

void LaunchServiceTest::buildsWindowsLaunchArgv()
{
    LaunchService service(QStringLiteral("launcher"), QStringLiteral("windows-run"));

    const LaunchSpec spec = service.windowsLaunch(QStringLiteral("/tmp/Game Folder/game.exe"));
    QCOMPARE(spec.program, QStringLiteral("windows-run"));
    QCOMPARE(spec.arguments, QStringList({QStringLiteral("--json"), QStringLiteral("/tmp/Game Folder/game.exe")}));
}

void LaunchServiceTest::preservesShellCharactersAsOneArgument()
{
    LaunchService service(QStringLiteral("launcher"), QStringLiteral("windows-run"));
    const QString path = QStringLiteral("/tmp/space;$(touch SHOULD_NOT_RUN)-ç.txt");

    const LaunchSpec spec = service.fileLaunch(path);
    QCOMPARE(spec.arguments.size(), 2);
    QCOMPARE(spec.arguments.constLast(), path);
}

void LaunchServiceTest::rejectsEmptyLaunchPaths()
{
    LaunchService service(QStringLiteral("launcher"), QStringLiteral("windows-run"));

    QVERIFY(!service.fileLaunch(QString()).isValid());
    QVERIFY(!service.desktopLaunch(QString()).isValid());
    QVERIFY(!service.windowsLaunch(QString()).isValid());
    QVERIFY(!service.launch({}).started);
}

void LaunchServiceTest::rejectsMissingAbsoluteLauncherAtLaunchTime()
{
    LaunchService service(
        QStringLiteral("/path/that/does/not/exist/astrea-launch"),
        QStringLiteral("/path/that/does/not/exist/windows-run"));

    const LaunchResult result = service.launch(
        service.fileLaunch(QStringLiteral("/tmp/example.txt")));

    QVERIFY(!result.started);
    QCOMPARE(result.error, QStringLiteral("launcher_missing"));
}

QTEST_GUILESS_MAIN(LaunchServiceTest)

#include "tst_launch_service.moc"
