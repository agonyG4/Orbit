#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/archive_controller.h"
#include "controllers/navigation_controller.h"
#include "models/directory_model.h"
#include "services/archive_operation_service.h"
#include "services/directory_watch_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

namespace {

ArchiveCapability capability(
    const QString &id,
    const QString &extension,
    bool create,
    bool extract,
    const QStringList &profiles = {QStringLiteral("fast"), QStringLiteral("balanced"), QStringLiteral("maximum")})
{
    ArchiveCapability result;
    result.id = id;
    result.label = id.toUpper();
    result.extension = extension;
    result.createSupported = create;
    result.extractSupported = extract;
    result.profiles = profiles;
    result.createProvider = QStringLiteral("test-provider");
    result.extractProvider = QStringLiteral("test-provider");
    return result;
}

} // namespace

class ArchiveControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void capabilityCatalogDrivesArchiveDetection();
    void createsMixedSourcesAndConsumesRealProgress();
    void extractionActionsUseDistinctDestinationModes();
    void serializesJobsAndCancelsByRequestIdentity();
    void passwordContinuationUsesStructuredStates();
    void conflictContinuationUsesImplementedPolicies();
    void waitingContinuationsDoNotFinishUntilResolved();
    void cancellingWaitingContinuationFinishesAsCancelled();
    void activeCancellationUsesCancelledWorkflowState();
    void rejectsUnsafeArchiveNames();
    void canonicalArchiveStemSupportsCompoundFormats();
    void staleResultsDoNotMutateTheWorkflow();
};

struct ArchiveFixture
{
    FakeRustBackendClient client;
    ArchiveOperationService service {&client};
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation {&client, &model, &watcher};
    ArchiveController archive {&service, &navigation};

    void loadCapabilities()
    {
        archive.refreshCapabilities();
        const BackendRequestId id = client.archiveOperationRequests().constLast().kind == QStringLiteral("capabilities")
            ? 1
            : 0;
        ArchiveOperationResult result;
        result.operation = QStringLiteral("capabilities");
        result.state = QStringLiteral("success");
        result.capabilities = {
            capability(QStringLiteral("zip"), QStringLiteral("zip"), true, true),
            capability(QStringLiteral("tar.zst"), QStringLiteral("tar.zst"), true, true),
        };
        client.completeArchiveOperation(id, result);
    }
};

void ArchiveControllerTest::capabilityCatalogDrivesArchiveDetection()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QVERIFY(fixture.archive.capabilities().size() == 2);
    QVERIFY(fixture.archive.canExtractArchive(QStringLiteral("/tmp/a.zip")));
    QVERIFY(fixture.archive.canExtractArchive(QStringLiteral("/tmp/a.tzst")));
    QVERIFY(!fixture.archive.canExtractArchive(QStringLiteral("/tmp/a.rar")));
}

void ArchiveControllerTest::createsMixedSourcesAndConsumesRealProgress()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);

    const QStringList sources {
        QStringLiteral("/tmp/report.txt"),
        QStringLiteral("/tmp/photos"),
        QStringLiteral("/tmp/notes"),
    };
    fixture.archive.startArchiveCreation(sources, QStringLiteral("Archive"), QStringLiteral("tar.zst"), QStringLiteral("balanced"));
    QCOMPARE(fixture.client.archiveOperationRequests().size(), 2);
    const ArchiveOperationRequest request = fixture.client.archiveOperationRequests().constLast();
    QCOMPARE(request.kind, QStringLiteral("create"));
    QCOMPARE(request.sources, sources);
    QCOMPARE(request.format, QStringLiteral("tar.zst"));
    QCOMPARE(request.profile, QStringLiteral("balanced"));
    QVERIFY(fixture.archive.running());

    ArchiveOperationProgress progress;
    progress.operation = QStringLiteral("create");
    progress.phase = QStringLiteral("compressing");
    progress.doneCount = 5;
    progress.totalCount = 10;
    progress.bytesDone = 50;
    progress.bytesTotal = 100;
    progress.progress = 0.5;
    progress.percent = 50;
    progress.currentName = QStringLiteral("report.txt");
    progress.statusText = QStringLiteral("Compressing");
    fixture.client.completeArchiveOperationProgress(2, progress);

    QVERIFY(stateSpy.count() > 0);
    QCOMPARE(fixture.archive.percent(), 50);
    QCOMPARE(fixture.archive.progress(), 0.5);
    QCOMPARE(fixture.archive.doneCount(), 5);
    QCOMPARE(fixture.archive.totalCount(), 10);
    QCOMPARE(fixture.archive.bytesDone(), qint64(50));
    QCOMPARE(fixture.archive.bytesTotal(), qint64(100));
    QCOMPARE(fixture.archive.currentName(), QStringLiteral("report.txt"));
}

void ArchiveControllerTest::extractionActionsUseDistinctDestinationModes()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();

    fixture.archive.startArchiveExtraction(
        QStringLiteral("/tmp/archive.tar.zst"), QStringLiteral("archive"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().destinationMode,
        QStringLiteral("new-directory"));
    ArchiveOperationResult cancelled;
    cancelled.operation = QStringLiteral("extract");
    cancelled.state = QStringLiteral("cancelled");
    fixture.client.completeArchiveOperation(2, cancelled);

    fixture.archive.startArchiveExtractionHere(
        QStringLiteral("/tmp/archive.tar.zst"), QStringLiteral("/tmp/current"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().destinationMode,
        QStringLiteral("into-directory"));
    fixture.client.completeArchiveOperation(3, cancelled);

    fixture.archive.startArchiveExtractionTo(
        QStringLiteral("/tmp/archive.tar.zst"), QStringLiteral("/tmp/selected"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().destinationMode,
        QStringLiteral("into-directory"));
}

void ArchiveControllerTest::serializesJobsAndCancelsByRequestIdentity()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    fixture.archive.startArchiveCreation({QStringLiteral("/tmp/a")}, QStringLiteral("a"), QStringLiteral("zip"), QStringLiteral("fast"));
    const int requestCount = fixture.client.archiveOperationRequests().size();
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/a.zip"), QStringLiteral("a"));
    QCOMPARE(fixture.client.archiveOperationRequests().size(), requestCount);

    fixture.archive.cancelArchiveOperation();
    QCOMPARE(fixture.client.cancelledRequests(), QVector<BackendRequestId>({2}));
}

void ArchiveControllerTest::passwordContinuationUsesStructuredStates()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QSignalSpy finishedSpy(&fixture.archive, &ArchiveController::operationFinished);
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/secret.zip"), QStringLiteral("secret"));
    ArchiveOperationResult required;
    required.operation = QStringLiteral("extract");
    required.state = QStringLiteral("password-required");
    fixture.client.completeArchiveOperation(2, required);
    QVERIFY(fixture.archive.passwordPromptVisible());
    QVERIFY(!fixture.archive.running());
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("waiting-password"));

    fixture.archive.submitArchivePassword(QStringLiteral("wrong"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().password, QStringLiteral("wrong"));
    ArchiveOperationResult bad;
    bad.operation = QStringLiteral("extract");
    bad.state = QStringLiteral("bad-password");
    fixture.client.completeArchiveOperation(3, bad);
    QVERIFY(fixture.archive.passwordPromptVisible());
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("waiting-password"));
    QVERIFY(!fixture.archive.passwordError().isEmpty());

    fixture.archive.submitArchivePassword(QStringLiteral("correct"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().password, QStringLiteral("correct"));
    ArchiveOperationResult success;
    success.operation = QStringLiteral("extract");
    success.state = QStringLiteral("success");
    success.destination = QStringLiteral("/tmp/secret");
    success.progress = 1.0;
    success.percent = 100;
    fixture.client.completeArchiveOperation(4, success);
    QVERIFY(!fixture.archive.passwordPromptVisible());
    QVERIFY(!fixture.archive.running());
    QCOMPARE(fixture.archive.destinationResult(), QStringLiteral("/tmp/secret"));
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("success"));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.constFirst().at(0).toULongLong(), BackendRequestId(2));
    QCOMPARE(fixture.archive.request(), BackendRequestId(0));
}

void ArchiveControllerTest::conflictContinuationUsesImplementedPolicies()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QSignalSpy finishedSpy(&fixture.archive, &ArchiveController::operationFinished);
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/archive.zip"), QStringLiteral("archive"));
    ArchiveOperationResult conflict;
    conflict.operation = QStringLiteral("extract");
    conflict.state = QStringLiteral("destination-conflict");
    conflict.destination = QStringLiteral("/tmp/archive");
    fixture.client.completeArchiveOperation(2, conflict);
    QVERIFY(fixture.archive.conflictVisible());
    QCOMPARE(fixture.archive.conflictDestination(), QStringLiteral("/tmp/archive"));
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("waiting-conflict"));

    fixture.archive.submitArchiveConflict(QStringLiteral("overwrite"));
    QCOMPARE(fixture.client.archiveOperationRequests().constLast().conflictPolicy, QStringLiteral("overwrite"));
    ArchiveOperationResult success;
    success.operation = QStringLiteral("extract");
    success.state = QStringLiteral("success");
    success.destination = QStringLiteral("/tmp/archive");
    fixture.client.completeArchiveOperation(3, success);
    QVERIFY(!fixture.archive.conflictVisible());
    QVERIFY(!fixture.archive.running());
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("success"));
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.constFirst().at(0).toULongLong(), BackendRequestId(2));
    QCOMPARE(fixture.archive.request(), BackendRequestId(0));
}

void ArchiveControllerTest::waitingContinuationsDoNotFinishUntilResolved()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QSignalSpy finishedSpy(&fixture.archive, &ArchiveController::operationFinished);

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/secret.zip"), QStringLiteral("secret"));
    ArchiveOperationResult required;
    required.operation = QStringLiteral("extract");
    required.state = QStringLiteral("password-required");
    fixture.client.completeArchiveOperation(2, required);
    QCOMPARE(finishedSpy.count(), 0);

    fixture.archive.submitArchivePassword(QStringLiteral("secret"));
    ArchiveOperationResult conflict;
    conflict.operation = QStringLiteral("extract");
    conflict.state = QStringLiteral("destination-conflict");
    fixture.client.completeArchiveOperation(3, conflict);
    QCOMPARE(finishedSpy.count(), 0);
}

void ArchiveControllerTest::cancellingWaitingContinuationFinishesAsCancelled()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    QSignalSpy finishedSpy(&fixture.archive, &ArchiveController::operationFinished);

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/secret.zip"), QStringLiteral("secret"));
    ArchiveOperationResult required;
    required.operation = QStringLiteral("extract");
    required.state = QStringLiteral("password-required");
    fixture.client.completeArchiveOperation(2, required);
    fixture.archive.cancelArchivePassword();
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("cancelled"));
    QVERIFY(!fixture.archive.passwordPromptVisible());
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(finishedSpy.constFirst().at(2).toBool(), false);

    ArchiveFixture conflictFixture;
    conflictFixture.loadCapabilities();
    QSignalSpy conflictFinished(&conflictFixture.archive, &ArchiveController::operationFinished);
    conflictFixture.archive.startArchiveExtraction(QStringLiteral("/tmp/archive.zip"), QStringLiteral("archive"));
    ArchiveOperationResult conflict;
    conflict.operation = QStringLiteral("extract");
    conflict.state = QStringLiteral("destination-conflict");
    conflictFixture.client.completeArchiveOperation(2, conflict);
    conflictFixture.archive.cancelArchiveConflict();
    QCOMPARE(conflictFixture.archive.workflowState(), QStringLiteral("cancelled"));
    QCOMPARE(conflictFinished.count(), 1);
}

void ArchiveControllerTest::activeCancellationUsesCancelledWorkflowState()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    fixture.archive.startArchiveCreation(
        {QStringLiteral("/tmp/a")}, QStringLiteral("a"), QStringLiteral("zip"), QStringLiteral("balanced"));
    fixture.archive.cancelArchiveOperation();
    ArchiveOperationResult cancelled;
    cancelled.operation = QStringLiteral("create");
    cancelled.state = QStringLiteral("cancelled");
    fixture.client.completeArchiveOperation(2, cancelled);
    QCOMPARE(fixture.archive.workflowState(), QStringLiteral("cancelled"));
    QCOMPARE(fixture.archive.status(), QStringLiteral("Cancelled"));
    QVERIFY(fixture.archive.error().isEmpty());
}

void ArchiveControllerTest::rejectsUnsafeArchiveNames()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    for (const QString &name : {QStringLiteral("../escape"), QStringLiteral("folder/name"), QStringLiteral("folder\\name")}) {
        fixture.archive.startArchiveCreation(
            {QStringLiteral("/tmp/source")}, name, QStringLiteral("zip"), QStringLiteral("balanced"));
    }
    QCOMPARE(fixture.client.archiveOperationRequests().size(), 1);
}

void ArchiveControllerTest::canonicalArchiveStemSupportsCompoundFormats()
{
    const QStringList names {
        QStringLiteral("Archive.zip"), QStringLiteral("Archive.7z"),
        QStringLiteral("Archive.rar"), QStringLiteral("Archive.tar"),
        QStringLiteral("Archive.tar.gz"), QStringLiteral("Archive.tgz"),
        QStringLiteral("Archive.tar.bz2"), QStringLiteral("Archive.tbz2"),
        QStringLiteral("Archive.tar.xz"), QStringLiteral("Archive.txz"),
        QStringLiteral("Archive.tar.zst"), QStringLiteral("Archive.tzst"),
    };
    for (const QString &name : names) {
        QCOMPARE(ArchiveController::canonicalArchiveStem(name), QStringLiteral("Archive"));
    }
}

void ArchiveControllerTest::staleResultsDoNotMutateTheWorkflow()
{
    ArchiveFixture fixture;
    fixture.loadCapabilities();
    fixture.archive.startArchiveCreation({QStringLiteral("/tmp/a")}, QStringLiteral("a"), QStringLiteral("zip"), QStringLiteral("balanced"));
    ArchiveOperationResult stale;
    stale.operation = QStringLiteral("create");
    stale.state = QStringLiteral("success");
    stale.destination = QStringLiteral("/tmp/stale.zip");
    fixture.client.completeArchiveOperation(99, stale);
    QVERIFY(fixture.archive.running());
    QCOMPARE(fixture.archive.destinationResult(), QString());
}

QTEST_MAIN(ArchiveControllerTest)
#include "tst_archive_controller.moc"
