#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/file_operations_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class FileOperationsControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void copyAndCutExposeClipboardState();
    void pasteDelegatesTypedRequestAndPublishesProgress();
    void cancelDelegatesToBackendAndClearsBusyState();
};

void FileOperationsControllerTest::copyAndCutExposeClipboardState()
{
    FakeRustBackendClient client;
    FileOperationsController controller(&client);

    controller.setSelection({QStringLiteral("/tmp/one.txt"), QStringLiteral("/tmp/two.txt")});
    controller.copySelection();
    QCOMPARE(controller.clipboardFiles(), QStringList({QStringLiteral("/tmp/one.txt"), QStringLiteral("/tmp/two.txt")}));
    QCOMPARE(controller.clipboardMode(), QStringLiteral("copy"));
    controller.cutSelection();
    QCOMPARE(controller.clipboardMode(), QStringLiteral("cut"));
    QVERIFY(controller.isCutPending(QStringLiteral("one.txt")));
}

void FileOperationsControllerTest::pasteDelegatesTypedRequestAndPublishesProgress()
{
    FakeRustBackendClient client;
    FileOperationsController controller(&client);
    QSignalSpy finishedSpy(&controller, &FileOperationsController::operationFinished);

    controller.setSelection({QStringLiteral("/tmp/source.txt")});
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
    QCOMPARE(finishedSpy.takeFirst().at(1).value<FileOperationResult>().ok, true);
}

void FileOperationsControllerTest::cancelDelegatesToBackendAndClearsBusyState()
{
    FakeRustBackendClient client;
    FileOperationsController controller(&client);
    controller.setSelection({QStringLiteral("/tmp/source.txt")});
    const BackendRequestId requestId = controller.pasteFiles(
        QStringLiteral("/tmp/destination"), QStringLiteral("overwrite"));
    controller.cancelOperation();
    QCOMPARE(client.cancelledRequests(), QVector<BackendRequestId>({requestId}));
    QCOMPARE(controller.running(), false);
}

QTEST_GUILESS_MAIN(FileOperationsControllerTest)

#include "tst_file_operations_controller.moc"
