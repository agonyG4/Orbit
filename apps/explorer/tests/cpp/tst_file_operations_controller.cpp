#include <QGuiApplication>
#include <QClipboard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/file_operations_controller.h"
#include "services/clipboard_service.h"
#include "services/file_operation_service.h"

using namespace Astrea::Explorer::Native::Backend;
using namespace Astrea::Explorer::Native::Services;

class FileOperationsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void copyAndCutExposeClipboardState();
    void emptySelectionPreservesClipboardState();
    void repeatedCutIsOrderInsensitiveAndClearsClipboard();
    void pasteDelegatesTypedRequestAndPublishesProgress();
    void progressSignalsCarryLiveUpdates();
    void terminalSignalIsSingleAndCoherent();
    void exposesTerminalItemResults();
    void pastePreflightsConflictsBeforeStarting();
    void dragTransferDoesNotMutateClipboard();
    void successfulMoveReconcilesCutClipboard();
    void partialMoveRetainsFailedCutClipboardItems();
    void cancelDelegatesToBackendAndClearsBusyState();
};

void FileOperationsControllerTest::copyAndCutExposeClipboardState()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    ClipboardService clipboard(QGuiApplication::clipboard());
    FileOperationsController controller(&service, &clipboard);

    controller.setSelection({QStringLiteral("/tmp/one.txt"), QStringLiteral("/tmp/two.txt")});
    controller.copySelection();
    QCOMPARE(controller.clipboardFiles(), QStringList({QStringLiteral("/tmp/one.txt"), QStringLiteral("/tmp/two.txt")}));
    QCOMPARE(controller.clipboardMode(), QStringLiteral("copy"));
    QCOMPARE(
        QGuiApplication::clipboard()->mimeData()->urls(),
        QList<QUrl>({QUrl::fromLocalFile(QStringLiteral("/tmp/one.txt")),
                     QUrl::fromLocalFile(QStringLiteral("/tmp/two.txt"))}));
    controller.cutSelection();
    QCOMPARE(controller.clipboardMode(), QStringLiteral("cut"));
    QVERIFY(controller.isCutPending(QStringLiteral("one.txt")));
    QVERIFY(controller.isCutPathPending(QStringLiteral("/tmp/one.txt")));
    QVERIFY(!controller.isCutPathPending(QStringLiteral("/tmp/other/one.txt")));
}

void FileOperationsControllerTest::emptySelectionPreservesClipboardState()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    ClipboardService clipboard(QGuiApplication::clipboard());
    FileOperationsController controller(&service, &clipboard);

    const QStringList paths {QStringLiteral("/tmp/keep.txt")};
    controller.setSelection(paths);
    controller.copySelection();
    const QStringList clipboardBefore = controller.clipboardFiles();
    const QString modeBefore = controller.clipboardMode();
    const QList<QUrl> urlsBefore = QGuiApplication::clipboard()->mimeData()->urls();

    controller.setSelection({});
    controller.copySelection();
    controller.cutSelection();

    QCOMPARE(controller.clipboardFiles(), clipboardBefore);
    QCOMPARE(controller.clipboardMode(), modeBefore);
    QCOMPARE(QGuiApplication::clipboard()->mimeData()->urls(), urlsBefore);
}

void FileOperationsControllerTest::repeatedCutIsOrderInsensitiveAndClearsClipboard()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    ClipboardService clipboard(QGuiApplication::clipboard());
    FileOperationsController controller(&service, &clipboard);

    const QString first = QStringLiteral("/tmp/first.txt");
    const QString second = QStringLiteral("/tmp/second.txt");
    controller.setSelection({first, second});
    controller.cutSelection();
    QVERIFY(!QGuiApplication::clipboard()->mimeData()->urls().isEmpty());

    controller.setSelection({second, first});
    controller.cutSelection();

    QVERIFY(controller.clipboardFiles().isEmpty());
    QCOMPARE(controller.clipboardMode(), QString());
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData == nullptr || mimeData->urls().isEmpty());
}

void FileOperationsControllerTest::pasteDelegatesTypedRequestAndPublishesProgress()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    QSignalSpy finishedSpy(&controller, &FileOperationsController::operationFinished);

    controller.setSelection({QStringLiteral("/tmp/source.txt")});
    controller.copySelection();
    const BackendRequestId requestId = controller.pasteFiles(
        QStringLiteral("/tmp/destination"), QStringLiteral("keep-both"));
    QCOMPARE(client.fileOperationRequests().size(), 1);
    QCOMPARE(client.fileOperationRequests().constFirst().mode, QStringLiteral("copy"));
    QCOMPARE(client.fileOperationRequests().constFirst().destination, QStringLiteral("/tmp/destination"));
    QCOMPARE(controller.running(), true);
    QCOMPARE(controller.operationRequestId(), requestId);

    FileOperationProgress progress;
    progress.requestId = requestId;
    progress.doneCount = 1;
    progress.totalCount = 1;
    progress.percent = 100;
    progress.fileName = QStringLiteral("source.txt");
    client.completeFileOperationProgress(requestId, progress);
    QCOMPARE(controller.operationPercent(), 100);
    QCOMPARE(controller.operationFileName(), QStringLiteral("source.txt"));

    FileOperationResult result;
    result.requestId = requestId;
    result.ok = true;
    result.destination = QStringLiteral("/tmp/destination");
    result.doneCount = 1;
    result.totalCount = 1;
    client.completeFileOperation(requestId, result);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QCOMPARE(controller.running(), false);
    QCOMPARE(finishedSpy.takeFirst().at(0).value<FileOperationResult>().ok, true);
}

void FileOperationsControllerTest::progressSignalsCarryLiveUpdates()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    QVector<int> emittedPercents;
    QVector<bool> emittedRunning;
    connect(
        &controller,
        &FileOperationsController::operationStateChanged,
        &controller,
        [&]() {
            emittedPercents.append(controller.operationPercent());
            emittedRunning.append(controller.running());
        });

    const BackendRequestId requestId = controller.transferFiles(
        {QStringLiteral("/tmp/source.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("copy"));
    QVERIFY(requestId > 0);
    emittedPercents.clear();
    emittedRunning.clear();

    for (const int percent : {25, 50, 75}) {
        FileOperationProgress progress;
        progress.requestId = requestId;
        progress.doneCount = percent == 75 ? 1 : 0;
        progress.totalCount = 1;
        progress.percent = percent;
        client.completeFileOperationProgress(requestId, progress);
    }

    QCOMPARE(emittedPercents, QVector<int>({25, 50, 75}));
    QCOMPARE(emittedRunning, QVector<bool>({true, true, true}));
    QCOMPARE(controller.operationState(), QStringLiteral("running"));
}

void FileOperationsControllerTest::terminalSignalIsSingleAndCoherent()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    QVector<QString> emittedStates;
    QVector<bool> emittedRunning;
    connect(
        &controller,
        &FileOperationsController::operationStateChanged,
        &controller,
        [&]() {
            emittedStates.append(controller.operationState());
            emittedRunning.append(controller.running());
        });

    const BackendRequestId successRequest = controller.transferFiles(
        {QStringLiteral("/tmp/success.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("copy"));
    emittedStates.clear();
    emittedRunning.clear();
    FileOperationResult success;
    success.requestId = successRequest;
    success.ok = true;
    success.doneCount = 1;
    success.totalCount = 1;
    success.percent = 100;
    client.completeFileOperation(successRequest, success);
    QCOMPARE(emittedStates, QVector<QString>({QStringLiteral("success")}));
    QCOMPARE(emittedRunning, QVector<bool>({false}));

    const BackendRequestId failureRequest = controller.transferFiles(
        {QStringLiteral("/tmp/failure.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("copy"));
    emittedStates.clear();
    emittedRunning.clear();
    client.failRequest(
        failureRequest,
        QStringLiteral("permission_denied"),
        QStringLiteral("permission denied"));
    QCOMPARE(emittedStates, QVector<QString>({QStringLiteral("failed")}));
    QCOMPARE(emittedRunning, QVector<bool>({false}));

    const BackendRequestId cancelledRequest = controller.transferFiles(
        {QStringLiteral("/tmp/cancelled.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("copy"));
    emittedStates.clear();
    emittedRunning.clear();
    controller.cancelOperation();
    QCOMPARE(client.cancelledRequests().constLast(), cancelledRequest);
    QCOMPARE(emittedStates, QVector<QString>({QStringLiteral("cancelled")}));
    QCOMPARE(emittedRunning, QVector<bool>({false}));
}

void FileOperationsControllerTest::exposesTerminalItemResults()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);

    const BackendRequestId requestId = controller.transferFiles(
        {QStringLiteral("/tmp/source.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("move"));
    FileOperationResult result;
    result.requestId = requestId;
    result.mode = QStringLiteral("move");
    result.ok = false;
    result.state = QStringLiteral("partial-success");
    result.doneCount = 1;
    result.totalCount = 2;
    result.percent = 50;
    result.errorCode = QStringLiteral("permission-denied");
    result.errorMessage = QStringLiteral("one item failed");
    FileOperationItemResult item;
    item.source = QStringLiteral("/tmp/source.txt");
    item.target = QStringLiteral("/tmp/destination/source.txt");
    item.status = QStringLiteral("failed");
    item.errorCode = QStringLiteral("permission-denied");
    item.errorMessage = QStringLiteral("one item failed");
    result.items.append(item);
    client.completeFileOperation(requestId, result);

    QCOMPARE(controller.operationState(), QStringLiteral("partial-success"));
    QCOMPARE(controller.operationItems().size(), 1);
    const QVariantMap itemMap = controller.operationItems().constFirst().toMap();
    QCOMPARE(itemMap.value(QStringLiteral("status")).toString(), QStringLiteral("failed"));
    QCOMPARE(itemMap.value(QStringLiteral("errorCode")).toString(), QStringLiteral("permission-denied"));
    QCOMPARE(controller.operationError(), QStringLiteral("one item failed"));
}

void FileOperationsControllerTest::dragTransferDoesNotMutateClipboard()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);

    controller.setSelection({QStringLiteral("/tmp/clipboard.txt")});
    controller.copySelection();
    const QStringList clipboardBefore = controller.clipboardFiles();

    const BackendRequestId requestId = controller.transferFiles(
        {QStringLiteral("/tmp/dragged.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("move"));

    QCOMPARE(controller.clipboardFiles(), clipboardBefore);
    QCOMPARE(controller.clipboardMode(), QStringLiteral("copy"));
    QCOMPARE(client.fileOperationRequests().size(), 1);
    const FileOperationRequest request = client.fileOperationRequests().constLast();
    QVERIFY(requestId > 0);
    QCOMPARE(request.mode, QStringLiteral("move"));
    QCOMPARE(request.sources, QStringList({QStringLiteral("/tmp/dragged.txt")}));
}

void FileOperationsControllerTest::pastePreflightsConflictsBeforeStarting()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString sourcePath = fixture.filePath(QStringLiteral("source.txt"));
    const QString destinationPath = fixture.filePath(QStringLiteral("destination"));
    QVERIFY(QDir().mkpath(destinationPath));
    QVERIFY(QFile(sourcePath).open(QIODevice::WriteOnly));
    QVERIFY(QFile(destinationPath + QStringLiteral("/source.txt")).open(QIODevice::WriteOnly));

    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    controller.setSelection({sourcePath});
    controller.copySelection();

    QCOMPARE(controller.pasteFiles(destinationPath), BackendRequestId(0));
    QVERIFY(controller.pasteConflictVisible());
    QCOMPARE(controller.pasteConflictItems(), QVariantList({sourcePath}));
    QCOMPARE(client.fileOperationRequests().size(), 0);

    controller.resolvePasteConflict(QStringLiteral("skip"));
    QCOMPARE(client.fileOperationRequests().size(), 1);
    QCOMPARE(client.fileOperationRequests().constFirst().conflictPolicy, QStringLiteral("skip"));
}

void FileOperationsControllerTest::successfulMoveReconcilesCutClipboard()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString sourcePath = fixture.filePath(QStringLiteral("source.txt"));
    QVERIFY(QFile(sourcePath).open(QIODevice::WriteOnly));
    const QString destinationPath = fixture.filePath(QStringLiteral("destination"));
    QVERIFY(QDir().mkpath(destinationPath));

    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    controller.setSelection({sourcePath});
    controller.cutSelection();
    const BackendRequestId requestId = controller.pasteFiles(destinationPath, QStringLiteral("overwrite"));

    FileOperationResult result;
    result.requestId = requestId;
    result.ok = true;
    result.mode = QStringLiteral("move");
    result.doneCount = 1;
    result.totalCount = 1;
    result.percent = 100;
    client.completeFileOperation(requestId, result);

    QVERIFY(controller.clipboardFiles().isEmpty());
    QVERIFY(controller.clipboardMode().isEmpty());
}

void FileOperationsControllerTest::partialMoveRetainsFailedCutClipboardItems()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    const QString moved = QStringLiteral("/tmp/moved.txt");
    const QString failed = QStringLiteral("/tmp/failed.txt");
    controller.setSelection({moved, failed});
    controller.cutSelection();

    const BackendRequestId requestId = controller.pasteFiles(
        QStringLiteral("/tmp/destination"), QStringLiteral("overwrite"));
    FileOperationResult result;
    result.requestId = requestId;
    result.ok = true;
    result.mode = QStringLiteral("move");
    result.state = QStringLiteral("partial-success");
    result.doneCount = 1;
    result.totalCount = 2;
    result.percent = 50;
    result.items = {
        FileOperationItemResult {
            moved,
            QStringLiteral("/tmp/destination/moved.txt"),
            QStringLiteral("moved"),
            {},
            {},
        },
        FileOperationItemResult {
            failed,
            QStringLiteral("/tmp/destination/failed.txt"),
            QStringLiteral("failed"),
            QStringLiteral("permission_denied"),
            QStringLiteral("permission denied"),
        },
    };
    client.completeFileOperation(requestId, result);

    QCOMPARE(controller.clipboardFiles(), QStringList({failed}));
    QCOMPARE(controller.clipboardMode(), QStringLiteral("cut"));
}

void FileOperationsControllerTest::cancelDelegatesToBackendAndClearsBusyState()
{
    FakeRustBackendClient client;
    FileOperationService service(&client);
    FileOperationsController controller(&service);
    controller.setSelection({QStringLiteral("/tmp/source.txt")});
    controller.copySelection();
    const BackendRequestId requestId = controller.pasteFiles(
        QStringLiteral("/tmp/destination"), QStringLiteral("overwrite"));
    controller.cancelOperation();
    QCOMPARE(client.cancelledRequests(), QVector<BackendRequestId>({requestId}));
    QCOMPARE(controller.running(), false);
    QCOMPARE(controller.operationState(), QStringLiteral("cancelled"));
    QCOMPARE(controller.operationStatus(), QStringLiteral("Cancelled"));
}

QTEST_MAIN(FileOperationsControllerTest)

#include "tst_file_operations_controller.moc"
