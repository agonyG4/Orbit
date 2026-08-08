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
    process.setArguments({QStringLiteral("--self-test")});
    process.setProcessChannelMode(QProcess::MergedChannels);

    process.start();

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(10000), "astrea-explorer --self-test timed out");
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

QTEST_MAIN(NativeBootstrapTest)

#include "tst_native_bootstrap.moc"
