#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "services/windows_launch_controller.h"

using namespace Astrea::Explorer::Native::Services;

class WindowsLaunchControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidWindowsLaunchRecord();
    void exposesUmuFailure();
    void exposesWineFailure();
    void exposesInvalidPeFailure();
    void rejectsMalformedSuccessJson();
    void reportsFailedProcessStartImmediately();
    void boundsProviderOutput();
    void completesWhenLauncherLeavesApplicationChildAlive();

private:
    static QString writeLauncher(const QString &root, const QString &body);
    static QString successRecord(
        const QString &target,
        const QString &runner = QStringLiteral("umu-proton"),
        const QString &machine = QStringLiteral("x86_64"),
        const QStringList &warnings = {});
    static LaunchSpec spec(const QString &program, const QString &target);
};

QString WindowsLaunchControllerTest::writeLauncher(const QString &root, const QString &body)
{
    const QString path = QDir(root).filePath(QStringLiteral("astrea-launch"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("#!/bin/sh\n");
    file.write(body.toUtf8());
    file.write("\n");
    file.close();
    if (!QFile::setPermissions(
            path,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        return {};
    }
    return path;
}

QString WindowsLaunchControllerTest::successRecord(
    const QString &target,
    const QString &runner,
    const QString &machine,
    const QStringList &warnings)
{
    QJsonArray warningArray;
    for (const QString &warning : warnings) {
        warningArray.append(warning);
    }
    const QJsonObject windows {
        {QStringLiteral("runner"), runner},
        {QStringLiteral("machine"), machine},
        {QStringLiteral("warnings"), warningArray},
    };
    const QJsonObject record {
        {QStringLiteral("timestamp_ms"), 1},
        {QStringLiteral("kind"), QStringLiteral("windows")},
        {QStringLiteral("target"), target},
        {QStringLiteral("argv"), QJsonArray {QStringLiteral("umu-run"), target}},
        {QStringLiteral("pid"), 1234},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("detail"), QStringLiteral("spawned")},
        {QStringLiteral("windows"), windows},
    };
    return QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact));
}

LaunchSpec WindowsLaunchControllerTest::spec(const QString &program, const QString &target)
{
    return {program, {QStringLiteral("--windows"), target}};
}

void WindowsLaunchControllerTest::acceptsValidWindowsLaunchRecord()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = QDir(directory.path()).filePath(QStringLiteral("game.exe"));
    const QString json = successRecord(
        target,
        QStringLiteral("umu-proton"),
        QStringLiteral("x86_64"),
        {QStringLiteral("GameMode unavailable")});
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("printf '%s\\n' '%1'").arg(json));

    WindowsLaunchController controller;
    QVERIFY(controller.launch(spec(launcher, target)));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QVERIFY(controller.error().isEmpty());
    QCOMPARE(controller.runner(), QStringLiteral("umu-proton"));
    QCOMPARE(controller.machine(), QStringLiteral("x86_64"));
    QCOMPARE(controller.warnings(), QStringList {QStringLiteral("GameMode unavailable")});
    QCOMPARE(controller.status(), QStringLiteral("Opening Windows application via UMU"));
}

void WindowsLaunchControllerTest::exposesUmuFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("printf '%s\\n' 'UMU is required for the Proton Windows runner' >&2\nexit 1"));
    WindowsLaunchController controller;

    QVERIFY(controller.launch(spec(launcher, QStringLiteral("/tmp/game.exe"))));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QCOMPARE(controller.error(), QStringLiteral("UMU is required for the Proton Windows runner"));
}

void WindowsLaunchControllerTest::exposesWineFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("printf '%s\\n' 'Wine not found; install wine or choose Auto' >&2\nexit 1"));
    WindowsLaunchController controller;

    QVERIFY(controller.launch(spec(launcher, QStringLiteral("/tmp/game.exe"))));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QCOMPARE(controller.error(), QStringLiteral("Wine not found; install wine or choose Auto"));
}

void WindowsLaunchControllerTest::exposesInvalidPeFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("printf '%s\\n' 'Windows executable is not a valid DOS/PE image' >&2\nexit 1"));
    WindowsLaunchController controller;

    QVERIFY(controller.launch(spec(launcher, QStringLiteral("/tmp/bad.exe"))));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QCOMPARE(controller.error(), QStringLiteral("Windows executable is not a valid DOS/PE image"));
}

void WindowsLaunchControllerTest::rejectsMalformedSuccessJson()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString launcher = writeLauncher(
        directory.path(), QStringLiteral("printf '%s\\n' 'not json'"));
    WindowsLaunchController controller;

    QVERIFY(controller.launch(spec(launcher, QStringLiteral("/tmp/game.exe"))));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QVERIFY(controller.error().contains(QStringLiteral("protocol")));
}

void WindowsLaunchControllerTest::reportsFailedProcessStartImmediately()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    WindowsLaunchController controller;

    QVERIFY(!controller.launch(spec(
        QDir(directory.path()).filePath(QStringLiteral("missing-astrea-launch")),
        QStringLiteral("/tmp/game.exe"))));
    QCOMPARE(controller.error(), QStringLiteral("launcher_missing"));
    QVERIFY(!controller.running());
}

void WindowsLaunchControllerTest::boundsProviderOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("i=0\nwhile [ $i -lt 100000 ]; do printf x >&2; i=$((i+1)); done\nexit 1"));
    WindowsLaunchController controller;

    QVERIFY(controller.launch(spec(launcher, QStringLiteral("/tmp/game.exe"))));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QVERIFY(controller.error().size() <= 64 * 1024);
}

void WindowsLaunchControllerTest::completesWhenLauncherLeavesApplicationChildAlive()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString target = QDir(directory.path()).filePath(QStringLiteral("game.exe"));
    const QString pidPath = QDir(directory.path()).filePath(QStringLiteral("child.pid"));
    const QString json = successRecord(target);
    const QString launcher = writeLauncher(
        directory.path(),
        QStringLiteral("sleep 5 & printf '%s\\n' \"$!\" > '%1'\nprintf '%s\\n' '%2'")
            .arg(pidPath, json));

    WindowsLaunchController controller;
    QVERIFY(controller.launch(spec(launcher, target)));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.running(), 5000);
    QVERIFY(controller.error().isEmpty());

    QFile pidFile(pidPath);
    QVERIFY(pidFile.open(QIODevice::ReadOnly));
    const QString pid = QString::fromUtf8(pidFile.readAll()).trimmed();
    QVERIFY(!pid.isEmpty());
    QCOMPARE(QProcess::execute(QStringLiteral("/bin/kill"), {QStringLiteral("-0"), pid}), 0);
    QProcess::execute(QStringLiteral("/bin/kill"), {pid});
}

QTEST_GUILESS_MAIN(WindowsLaunchControllerTest)

#include "tst_windows_launch_controller.moc"
