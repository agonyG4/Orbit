#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <QtTest>

class NativeBootstrapTest final : public QObject
{
    Q_OBJECT

private slots:
    void selfTestLoadsBootstrap();
    void selfTestLoadsRealExplorerQml();
};

void NativeBootstrapTest::selfTestLoadsBootstrap()
{
    const QString program = QStringLiteral(ASTREA_EXPLORER_BIN);

    QVERIFY2(
        QFileInfo::exists(program),
        qPrintable(QStringLiteral("Expected native executable at %1").arg(program)));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));

    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments({QStringLiteral("--self-test"), QStringLiteral("--bootstrap")});
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.start();

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(10000), "astrea-explorer --self-test timed out");
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void NativeBootstrapTest::selfTestLoadsRealExplorerQml()
{
    const QString program = QStringLiteral(ASTREA_EXPLORER_BIN);
    QVERIFY2(
        QFileInfo::exists(program),
        qPrintable(QStringLiteral("Expected native executable at %1").arg(program)));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(
        QStringLiteral("ASTREA_ROOT"),
        QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));

    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments({QStringLiteral("--self-test")});
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.start();

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(10000), "astrea-explorer --self-test timed out");
    const QByteArray output = process.readAll();
    if (output.contains("quickshell-ioplugin") || output.contains("module \"Quickshell")) {
        QSKIP(qPrintable(QStringLiteral("Explorer runtime dependencies are unavailable: %1")
                            .arg(QString::fromUtf8(output))));
    }
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QVERIFY2(
        output.contains("Apps/Explorer/Main.qml"),
        qPrintable(QStringLiteral("Native self-test did not load real Main.qml: %1")
                       .arg(QString::fromUtf8(output))));
}

QTEST_MAIN(NativeBootstrapTest)

#include "tst_native_bootstrap.moc"
