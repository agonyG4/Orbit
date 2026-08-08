#include <QtTest>

#include "services/launch_service.h"

using namespace Astrea::Explorer::Native::Services;

class LaunchServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void buildsFileLaunchArgv();
    void buildsDesktopLaunchArgv();
    void buildsWindowsLaunchArgv();
    void preservesShellCharactersAsOneArgument();
    void rejectsEmptyLaunchPaths();
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

QTEST_GUILESS_MAIN(LaunchServiceTest)

#include "tst_launch_service.moc"
