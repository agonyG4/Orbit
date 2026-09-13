#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "services/archive_operation_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class ArchiveOperationServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void serializesJobsAndForwardsProgress();
    void completesAndStartsNextJob();
    void cancellationUsesTheActiveIdentity();
    void staleEventsAreIgnored();
};

void ArchiveOperationServiceTest::serializesJobsAndForwardsProgress()
{
    FakeRustBackendClient client;
    ArchiveOperationService service(&client);
    QSignalSpy progressSpy(&service, &ArchiveOperationService::progress);

    ArchiveOperationRequest first;
    first.kind = QStringLiteral("create");
    first.sources = {QStringLiteral("/tmp/a")};
    first.format = QStringLiteral("zip");
    const BackendRequestId firstId = service.start(first);
    QVERIFY(firstId != 0);

    ArchiveOperationRequest second;
    second.kind = QStringLiteral("extract");
    QCOMPARE(service.start(second), BackendRequestId(0));

    ArchiveOperationProgress progress;
    progress.operation = QStringLiteral("create");
    progress.phase = QStringLiteral("compressing");
    progress.progress = 0.5;
    progress.percent = 50;
    client.completeArchiveOperationProgress(firstId, progress);

    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.constFirst().at(0).value<BackendRequestId>(), firstId);
    QCOMPARE(progressSpy.constFirst().at(1).value<ArchiveOperationProgress>().percent, 50);
}

void ArchiveOperationServiceTest::completesAndStartsNextJob()
{
    FakeRustBackendClient client;
    ArchiveOperationService service(&client);
    QSignalSpy finishedSpy(&service, &ArchiveOperationService::finished);

    ArchiveOperationRequest request;
    request.kind = QStringLiteral("create");
    const BackendRequestId firstId = service.start(request);
    ArchiveOperationResult result;
    result.operation = QStringLiteral("create");
    result.state = QStringLiteral("success");
    result.destination = QStringLiteral("/tmp/a.zip");
    client.completeArchiveOperation(firstId, result);

    QTRY_COMPARE(finishedSpy.count(), 1);
    QCOMPARE(service.activeRequest(), BackendRequestId(0));

    const BackendRequestId secondId = service.start(request);
    QVERIFY(secondId != 0);
    QVERIFY(secondId != firstId);
}

void ArchiveOperationServiceTest::cancellationUsesTheActiveIdentity()
{
    FakeRustBackendClient client;
    ArchiveOperationService service(&client);
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("extract");
    const BackendRequestId id = service.start(request);

    service.cancel(id + 1);
    QCOMPARE(client.cancelledRequests().size(), 0);
    service.cancel(id);
    QCOMPARE(client.cancelledRequests(), QVector<BackendRequestId>({id}));
}

void ArchiveOperationServiceTest::staleEventsAreIgnored()
{
    FakeRustBackendClient client;
    ArchiveOperationService service(&client);
    QSignalSpy progressSpy(&service, &ArchiveOperationService::progress);
    QSignalSpy finishedSpy(&service, &ArchiveOperationService::finished);
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("create");
    const BackendRequestId id = service.start(request);

    ArchiveOperationProgress progress;
    progress.percent = 10;
    client.completeArchiveOperationProgress(id + 100, progress);
    QCOMPARE(progressSpy.count(), 0);

    ArchiveOperationResult result;
    result.state = QStringLiteral("success");
    client.completeArchiveOperation(id + 100, result);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(service.activeRequest(), id);
}

QTEST_MAIN(ArchiveOperationServiceTest)
#include "tst_archive_operation_service.moc"
