#include <QElapsedTimer>
#include <QFile>
#include <QFileDevice>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/persistent_worker_transport.h"

using namespace Astrea::Explorer::Native::Backend;

namespace {

QString makeWorker(const QTemporaryDir &directory, const QString &body)
{
    const QString path = directory.filePath(QStringLiteral("worker.py"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("#!/usr/bin/env python3\n");
    file.write(body.toUtf8());
    file.close();
    if (!file.setPermissions(QFileDevice::ReadOwner
                             | QFileDevice::WriteOwner
                             | QFileDevice::ExeOwner)) {
        return {};
    }
    return path;
}

BackendTransportError errorFrom(const QSignalSpy &spy)
{
    return spy.constFirst().at(1).value<BackendTransportError>();
}

} // namespace

class PersistentWorkerTransportTest final : public QObject
{
    Q_OBJECT

private slots:
    void queuesRequestsUntilWorkerIsReady();
    void reportsStartFailureAsynchronously();
    void cancelsAnInFlightRequest();
};

void PersistentWorkerTransportTest::queuesRequestsUntilWorkerIsReady()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString worker = makeWorker(
        directory,
        QStringLiteral(
            "import json,sys,time\n"
            "time.sleep(0.2)\n"
            "for line in sys.stdin:\n"
            "    request=json.loads(line)\n"
            "    print(json.dumps({'id':request['id'],'ok':True,'payload':'ready'}), flush=True)\n"));
    QVERIFY(!worker.isEmpty());

    PersistentWorkerTransportOptions options;
    options.backendProgram = worker;
    options.requestTimeoutMs = 2000;
    PersistentWorkerTransport transport(options);
    QSignalSpy completedSpy(&transport, &BackendTransport::completed);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    QElapsedTimer elapsed;
    elapsed.start();
    const BackendRequestId first = transport.start({QStringLiteral("one")});
    const BackendRequestId second = transport.start({QStringLiteral("two")});
    QVERIFY(elapsed.elapsed() < 150);

    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.count(), 2, 3000);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(completedSpy.at(0).at(0).value<BackendRequestId>(), first);
    QCOMPARE(completedSpy.at(1).at(0).value<BackendRequestId>(), second);
}

void PersistentWorkerTransportTest::reportsStartFailureAsynchronously()
{
    PersistentWorkerTransportOptions options;
    options.backendProgram = QStringLiteral("/does/not/exist/astrea-worker");
    options.requestTimeoutMs = 1000;
    PersistentWorkerTransport transport(options);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    const BackendRequestId requestId = transport.start({QStringLiteral("one")});
    QCOMPARE(failedSpy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1500);
    const BackendTransportError error = errorFrom(failedSpy);
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("worker_start_failed"));
}

void PersistentWorkerTransportTest::cancelsAnInFlightRequest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString worker = makeWorker(
        directory,
        QStringLiteral(
            "import json,sys,time\n"
            "for line in sys.stdin:\n"
            "    request=json.loads(line)\n"
            "    if request['arguments'][0] == 'cancel':\n"
            "        continue\n"
            "    time.sleep(1)\n"
            "    print(json.dumps({'id':request['id'],'ok':True,'payload':'late'}), flush=True)\n"));
    QVERIFY(!worker.isEmpty());

    PersistentWorkerTransportOptions options;
    options.backendProgram = worker;
    options.requestTimeoutMs = 2000;
    PersistentWorkerTransport transport(options);
    QSignalSpy failedSpy(&transport, &BackendTransport::failed);

    const BackendRequestId requestId = transport.start({QStringLiteral("slow")});
    QTimer::singleShot(100, &transport, [requestId, &transport]() {
        transport.cancel(requestId);
    });

    QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 1500);
    const BackendTransportError error = errorFrom(failedSpy);
    QCOMPARE(error.requestId, requestId);
    QCOMPARE(error.code, QStringLiteral("cancelled"));
}

QTEST_MAIN(PersistentWorkerTransportTest)
#include "tst_persistent_worker_transport.moc"
