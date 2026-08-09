#include <QGuiApplication>
#include <QClipboard>
#include <QSignalSpy>
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
    void pasteDelegatesTypedRequestAndPublishesProgress();
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
}

QTEST_MAIN(FileOperationsControllerTest)

#include "tst_file_operations_controller.moc"
