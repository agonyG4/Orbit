#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "services/directory_metrics_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class DirectoryMetricsServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void forwardsMultiPathRequestAndResult();
    void replacesOldRequestAndRejectsLateSignals();
    void forwardsCancellationAsTerminalState();
    void forwardsNonCancellationFailure();
};

void DirectoryMetricsServiceTest::forwardsMultiPathRequestAndResult()
{
    FakeRustBackendClient client;
    DirectoryMetricsService service(&client);
    QSignalSpy progressSpy(&service, &DirectoryMetricsService::progress);
    QSignalSpy finishedSpy(&service, &DirectoryMetricsService::finished);

    const QStringList paths {QStringLiteral("/tmp/A"), QStringLiteral("/tmp/B")};
    const BackendRequestId requestId = service.start(paths);
    QCOMPARE(client.directoryMetricsRequests().size(), 1);
    QCOMPARE(client.directoryMetricsRequests().constFirst().paths, paths);
    QCOMPARE(service.activeRequest(), requestId);

    DirectoryMetricsProgress progress;
    progress.state = QStringLiteral("running");
    progress.bytes = 42;
    progress.fileCount = 3;
    progress.directoryCount = 2;
    progress.scannedEntryCount = 5;
    client.completeDirectoryMetricsProgress(requestId, progress);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.at(0).at(1).value<DirectoryMetricsProgress>().bytes, qint64(42));

    DirectoryMetricsResult result;
    result.operation = QStringLiteral("directory-metrics");
    result.state = QStringLiteral("partial");
    result.bytes = 42;
    result.fileCount = 3;
    result.directoryCount = 2;
    result.unreadableCount = 1;
    result.errorMessage = QStringLiteral("some items could not be read");
    client.completeDirectoryMetrics(requestId, result);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(service.activeRequest(), BackendRequestId(0));
    const DirectoryMetricsResult completed =
        finishedSpy.at(0).at(1).value<DirectoryMetricsResult>();
    QCOMPARE(completed.state, QStringLiteral("partial"));
    QCOMPARE(completed.unreadableCount, qint64(1));
}

void DirectoryMetricsServiceTest::replacesOldRequestAndRejectsLateSignals()
{
    FakeRustBackendClient client;
    DirectoryMetricsService service(&client);
    QSignalSpy progressSpy(&service, &DirectoryMetricsService::progress);
    QSignalSpy finishedSpy(&service, &DirectoryMetricsService::finished);

    const BackendRequestId first = service.start({QStringLiteral("/tmp/A")});
    const BackendRequestId second = service.start({QStringLiteral("/tmp/B")});
    QVERIFY(first != second);
    QCOMPARE(client.cancelledRequests().constLast(), first);

    DirectoryMetricsProgress stale;
    stale.state = QStringLiteral("running");
    stale.bytes = 100;
    client.completeDirectoryMetricsProgress(first, stale);
    QCOMPARE(progressSpy.count(), 0);

    DirectoryMetricsResult current;
    current.state = QStringLiteral("success");
    current.bytes = 7;
    client.completeDirectoryMetrics(second, current);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.at(0).at(0).value<BackendRequestId>(), second);
}

void DirectoryMetricsServiceTest::forwardsCancellationAsTerminalState()
{
    FakeRustBackendClient client;
    DirectoryMetricsService service(&client);
    QSignalSpy finishedSpy(&service, &DirectoryMetricsService::finished);

    const BackendRequestId requestId = service.start({QStringLiteral("/tmp/A")});
    service.cancel(requestId);
    QCOMPARE(client.cancelledRequests().constLast(), requestId);

    client.failRequest(requestId, QStringLiteral("cancelled"), QStringLiteral("cancelled"));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.at(0).at(1).value<DirectoryMetricsResult>().state,
             QStringLiteral("cancelled"));
}

void DirectoryMetricsServiceTest::forwardsNonCancellationFailure()
{
    FakeRustBackendClient client;
    DirectoryMetricsService service(&client);
    QSignalSpy failedSpy(&service, &DirectoryMetricsService::failed);

    const BackendRequestId requestId = service.start({QStringLiteral("/tmp/A")});
    client.failRequest(requestId, QStringLiteral("protocol_error"), QStringLiteral("bad result"));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.at(0).at(0).value<BackendError>().message, QStringLiteral("bad result"));
}

QTEST_GUILESS_MAIN(DirectoryMetricsServiceTest)

#include "tst_directory_metrics_service.moc"
