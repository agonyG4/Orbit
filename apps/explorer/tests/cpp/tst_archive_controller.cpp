#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/navigation_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/filesystem_service.h"

#define private public
#include "controllers/archive_controller.h"
#undef private

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class ArchiveControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsConcurrentExtractionWithoutMutation();
    void rejectsExtractionWhileCompressionRuns();
    void acceptsWorkAfterTerminalCompletion();
    void rejectsExtractionDuringPasswordContinuation();
    void rejectsCompressionDuringPasswordContinuation();
    void rejectsExtractionDuringConflictContinuation();
    void rejectsCompressionDuringConflictContinuation();
    void reportsOccupancyForRunningPasswordAndConflictStates();
    void rejectsPasswordSubmissionWhileRunning();
    void rejectsStalePasswordAfterCompletion();
    void rejectsConflictWithoutActiveConflict();
    void rejectsConflictWhileRunning();
    void rejectsUnsupportedConflictPolicyWithoutMutation();
    void ignoresPasswordCancelWithoutPrompt();
    void ignoresConflictCancelWithoutConflict();
    void releasesSlotBeforePublishingTerminalState();
};

struct ArchiveFixture
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation {&client, &model, &watcher};
    FilesystemService filesystem {&client};
    ArchiveController archive {&filesystem, &navigation};
};

void arrangeContinuation(ArchiveController &archive, bool passwordPrompt, bool conflict)
{
    archive.m_request = 0;
    archive.m_path = QStringLiteral("/tmp/pending.zip");
    archive.m_destination = QStringLiteral("/tmp/pending");
    archive.m_fileName = QStringLiteral("pending.zip");
    archive.m_status = QStringLiteral("Aguardando decisão");
    archive.m_error = QStringLiteral("previous error");
    archive.m_passwordError = QStringLiteral("password error");
    archive.m_destinationResult = QStringLiteral("/tmp/previous-result");
    archive.m_conflictDestination = QStringLiteral("/tmp/pending");
    archive.m_conflictName = QStringLiteral("pending");
    archive.m_conflictPolicy = QStringLiteral("keep-both");
    archive.m_running = false;
    archive.m_passwordPrompt = passwordPrompt;
    archive.m_conflict = conflict;
    archive.m_progress = 0.25;
    archive.m_percent = 25;
    archive.m_doneCount = 2;
    archive.m_totalCount = 8;
}

void ArchiveControllerTest::rejectsConcurrentExtractionWithoutMutation()
{
    ArchiveFixture fixture;
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/first.zip"), QStringLiteral("first"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    const QString fileName = fixture.archive.fileName();
    const QString destination = fixture.archive.destination();
    const QString status = fixture.archive.status();
    const int eventCount = stateSpy.count();

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/second.zip"), QStringLiteral("second"));

    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    QCOMPARE(stateSpy.count(), eventCount);
    QCOMPARE(fixture.archive.fileName(), fileName);
    QCOMPARE(fixture.archive.destination(), destination);
    QCOMPARE(fixture.archive.status(), status);
    QVERIFY(fixture.archive.running());
}

void ArchiveControllerTest::rejectsExtractionWhileCompressionRuns()
{
    ArchiveFixture fixture;
    fixture.archive.startFolderCompression(QStringLiteral("/tmp/folder"), QStringLiteral("zip"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    const QString fileName = fixture.archive.fileName();
    const QString destination = fixture.archive.destination();
    const int eventCount = fixture.archive.stateRevision();

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/second.zip"), QStringLiteral("second"));

    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QCOMPARE(fixture.archive.fileName(), fileName);
    QCOMPARE(fixture.archive.destination(), destination);
    QVERIFY(fixture.archive.running());
}

void ArchiveControllerTest::acceptsWorkAfterTerminalCompletion()
{
    ArchiveFixture fixture;
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/first.zip"), QStringLiteral("first"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);

    UtilityResult completed;
    completed.operation = QStringLiteral("archive-extract");
    completed.ok = true;
    completed.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/actual-first"));
    fixture.client.completeUtility(1, completed);
    QTRY_VERIFY(!fixture.archive.running());
    QCOMPARE(fixture.archive.destinationResult(), QStringLiteral("/tmp/actual-first"));

    fixture.archive.startFolderCompression(QStringLiteral("/tmp/folder"), QStringLiteral("tar.gz"));
    QCOMPARE(fixture.client.utilityRequests().size(), 2);
    QVERIFY(fixture.archive.running());
    QCOMPARE(fixture.archive.status(), QStringLiteral("Comprimindo..."));
}

void ArchiveControllerTest::rejectsExtractionDuringPasswordContinuation()
{
    ArchiveFixture fixture;
    arrangeContinuation(fixture.archive, true, false);
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/replacement.zip"), QStringLiteral("replacement"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QVERIFY(fixture.archive.passwordPromptVisible());
}

void ArchiveControllerTest::rejectsCompressionDuringPasswordContinuation()
{
    ArchiveFixture fixture;
    arrangeContinuation(fixture.archive, true, false);
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);
    fixture.archive.startFolderCompression(QStringLiteral("/tmp/replacement"), QStringLiteral("zip"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QVERIFY(fixture.archive.passwordPromptVisible());
}

void ArchiveControllerTest::rejectsExtractionDuringConflictContinuation()
{
    ArchiveFixture fixture;
    arrangeContinuation(fixture.archive, false, true);
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/replacement.zip"), QStringLiteral("replacement"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QVERIFY(fixture.archive.conflictVisible());
}

void ArchiveControllerTest::rejectsCompressionDuringConflictContinuation()
{
    ArchiveFixture fixture;
    arrangeContinuation(fixture.archive, false, true);
    QSignalSpy stateSpy(&fixture.archive, &ArchiveController::stateChanged);
    fixture.archive.startFolderCompression(QStringLiteral("/tmp/replacement"), QStringLiteral("zip"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(stateSpy.count(), 0);
    QVERIFY(fixture.archive.conflictVisible());
}

void ArchiveControllerTest::reportsOccupancyForRunningPasswordAndConflictStates()
{
    ArchiveFixture fixture;
    QVERIFY(!fixture.archive.workflowOccupied());

    fixture.archive.m_running = true;
    QVERIFY(fixture.archive.workflowOccupied());
    fixture.archive.m_running = false;
    fixture.archive.m_passwordPrompt = true;
    QVERIFY(fixture.archive.workflowOccupied());
    fixture.archive.m_passwordPrompt = false;
    fixture.archive.m_conflict = true;
    QVERIFY(fixture.archive.workflowOccupied());
    fixture.archive.m_running = true;
    QVERIFY(fixture.archive.workflowOccupied());
}

void ArchiveControllerTest::rejectsPasswordSubmissionWhileRunning()
{
    ArchiveFixture fixture;
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/active.zip"), QStringLiteral("active"));
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.submitArchivePassword(QStringLiteral("secret"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(fixture.archive.running());
}

void ArchiveControllerTest::rejectsStalePasswordAfterCompletion()
{
    ArchiveFixture fixture;
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/completed.zip"), QStringLiteral("completed"));
    UtilityResult completed;
    completed.operation = QStringLiteral("archive-extract");
    completed.ok = true;
    completed.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/completed-result"));
    fixture.client.completeUtility(1, completed);
    QTRY_VERIFY(!fixture.archive.running());
    const int requestCount = fixture.client.utilityRequests().size();
    const int eventCount = fixture.archive.stateRevision();

    fixture.archive.submitArchivePassword(QStringLiteral("stale-secret"));

    QCOMPARE(fixture.client.utilityRequests().size(), requestCount);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(!fixture.archive.running());
}

void ArchiveControllerTest::rejectsConflictWithoutActiveConflict()
{
    ArchiveFixture fixture;
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.submitArchiveConflict(QStringLiteral("overwrite"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(!fixture.archive.conflictVisible());
}

void ArchiveControllerTest::rejectsConflictWhileRunning()
{
    ArchiveFixture fixture;
    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/active.zip"), QStringLiteral("active"));
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.submitArchiveConflict(QStringLiteral("overwrite"));
    QCOMPARE(fixture.client.utilityRequests().size(), 1);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(fixture.archive.running());
}

void ArchiveControllerTest::rejectsUnsupportedConflictPolicyWithoutMutation()
{
    ArchiveFixture fixture;
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.submitArchiveConflict(QStringLiteral("merge"));
    QCOMPARE(fixture.client.utilityRequests().size(), 0);
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QCOMPARE(fixture.archive.conflictPolicy(), QStringLiteral("keep-both"));
}

void ArchiveControllerTest::ignoresPasswordCancelWithoutPrompt()
{
    ArchiveFixture fixture;
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.cancelArchivePassword();
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(!fixture.archive.passwordPromptVisible());
}

void ArchiveControllerTest::ignoresConflictCancelWithoutConflict()
{
    ArchiveFixture fixture;
    const int eventCount = fixture.archive.stateRevision();
    fixture.archive.cancelArchiveConflict();
    QCOMPARE(fixture.archive.stateRevision(), eventCount);
    QVERIFY(!fixture.archive.conflictVisible());
}

void ArchiveControllerTest::releasesSlotBeforePublishingTerminalState()
{
    ArchiveFixture fixture;
    bool startedReentrantly = false;
    connect(&fixture.archive, &ArchiveController::stateChanged, &fixture.archive, [&]() {
        if (!fixture.archive.running() && !startedReentrantly) {
            startedReentrantly = true;
            fixture.archive.startFolderCompression(QStringLiteral("/tmp/reentrant"), QStringLiteral("zip"));
        }
    });

    fixture.archive.startArchiveExtraction(QStringLiteral("/tmp/first.zip"), QStringLiteral("first"));
    UtilityResult completed;
    completed.operation = QStringLiteral("archive-extract");
    completed.ok = true;
    completed.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/first"));
    fixture.client.completeUtility(1, completed);

    QTRY_VERIFY(startedReentrantly);
    QCOMPARE(fixture.client.utilityRequests().size(), 2);
    QVERIFY(fixture.archive.running());
    QVERIFY(fixture.archive.request() != 0);
    QVERIFY(fixture.archive.request() != static_cast<BackendRequestId>(1));
}

QTEST_GUILESS_MAIN(ArchiveControllerTest)

#include "tst_archive_controller.moc"
