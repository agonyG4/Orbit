#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>
#include <QtTest>

namespace {

QByteArray presenterFixture(const QString &presenterUrl)
{
    return QStringLiteral(
               "import QtQuick 2.15\n"
               "import QtQml 2.15\n"
               "Item {\n"
               "    id: root\n"
               "    width: 1; height: 1\n"
               "    property QtObject operationState: ops\n"
               "    QtObject {\n"
               "        id: ops\n"
               "        property var fileSnapshot: ({ running: false, progress: 0, percent: 0, fileName: '', status: '', error: '', destination: '', doneCount: 0, totalCount: 0, mode: 'copy', state: '', items: [] })\n"
               "        property var archiveSnapshot: ({ running: false, progress: 0, percent: 0, fileName: '', status: '', error: '', destination: '', doneCount: 0, totalCount: 0, remainingText: '' })\n"
               "        signal fileOperationChanged(var snapshot)\n"
               "        signal archiveOperationChanged(var snapshot)\n"
               "        function currentFileOperationSnapshot() { return fileSnapshot }\n"
               "        function currentArchiveOperationSnapshot() { return archiveSnapshot }\n"
               "        function publishFile(snapshot) { fileSnapshot = snapshot; fileOperationChanged(snapshot) }\n"
               "        function publishArchive(snapshot) { archiveSnapshot = snapshot; archiveOperationChanged(snapshot) }\n"
               "    }\n"
               "    property QtObject presenter: loader.item\n"
               "    function fileSnapshot(running, status, progress, percent, doneCount, totalCount, state, error) {\n"
               "        return { running: running, progress: progress, percent: percent, fileName: 'source.txt', status: status, error: error || '', destination: '/tmp/destination', doneCount: doneCount, totalCount: totalCount, mode: 'copy', state: state, items: [] }\n"
               "    }\n"
               "    function archiveSnapshot(running, status, state, error) {\n"
               "        return { running: running, progress: 0, percent: 0, fileName: 'bundle.tar', status: status, error: error || '', destination: '/tmp/bundle', doneCount: 0, totalCount: 0, remainingText: 'Aguardando...' }\n"
               "    }\n"
               "    function startFile() { ops.publishFile(fileSnapshot(true, 'Copying...', 0.0, 0, 0, 1, 'running', '')) }\n"
               "    function setFileProgress(value) { ops.publishFile(fileSnapshot(true, 'Copying...', value / 100, value, value >= 100 ? 1 : 0, 1, 'running', '')) }\n"
               "    function finishFile() { ops.publishFile(fileSnapshot(false, 'Completed', 1, 100, 1, 1, 'success', '')) }\n"
               "    function finishFailedFile() { ops.publishFile(fileSnapshot(false, 'Failed', 0.5, 50, 1, 2, 'failed', 'permission denied')) }\n"
               "    function finishPartialFile() { ops.publishFile(fileSnapshot(false, 'Completed with errors', 0.5, 50, 1, 2, 'partial-success', 'one item failed')) }\n"
               "    function finishCancelledFile() { ops.publishFile(fileSnapshot(false, 'Cancelled', 0, 0, 0, 1, 'cancelled', '')) }\n"
               "    function startArchive() { ops.publishArchive(archiveSnapshot(true, 'Extracting...', 'running', '')) }\n"
               "    function finishArchive() { ops.publishArchive(archiveSnapshot(false, 'Completed', 'success', '')) }\n"
               "    Loader {\n"
               "        id: loader\n"
               "        source: \"%1\"\n"
               "        onLoaded: item.operationState = root.operationState\n"
               "    }\n"
               "}")
        .arg(presenterUrl)
        .toUtf8();
}

QString presenterPath()
{
    return QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/components/common/OperationProgressPresenter.qml");
}

QObject *createRoot(QQmlEngine *engine, const QString &path, QString *error)
{
    QQmlComponent component(engine);
    component.setData(
        presenterFixture(QUrl::fromLocalFile(path).toString()),
        QUrl(QStringLiteral("qrc:/operation-progress-presenter-test.qml")));
    if (!component.isReady()) {
        *error = component.errorString();
        return nullptr;
    }
    QObject *root = component.create();
    if (root == nullptr)
        *error = component.errorString();
    return root;
}

} // namespace

class OperationProgressPresenterTest final : public QObject
{
    Q_OBJECT

private slots:
    void successTerminalStateRemainsVisibleUntilFade();
    void failureUsesFailureHoldAndStyling();
    void partialSuccessUsesLongerHold();
    void cancelledUsesCancelledPresentation();
    void archiveRunningIsIndeterminate();
    void liveOperationPreemptsOldTerminalState();
    void archiveTerminalPresentationDoesNotBlockNewArchive();
    void liveProgressUpdatesWhileSemanticStateRemainsRunning();
    void fastCompletionKeepsRunningPresentationUntilMinimum();
    void pendingTerminalIsPreemptedByNewOperation();
    void fileProgressResumesAfterArchive();
};

void OperationProgressPresenterTest::successTerminalStateRemainsVisibleUntilFade()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 20);
    presenter->setProperty("successHoldMs", 40);
    presenter->setProperty("fadeOutMs", 30);

    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QTRY_VERIFY_WITH_TIMEOUT(presenter->property("cardVisible").toBool(), 500);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));

    QVERIFY(QMetaObject::invokeMethod(root, "finishFile"));
    QTRY_VERIFY_WITH_TIMEOUT(presenter->property("terminal").toBool(), 500);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("terminal"));
    QCOMPARE(presenter->property("title").toString(), QStringLiteral("Completed"));
    QCOMPARE(presenter->property("percent").toInt(), 100);
    QCOMPARE(presenter->property("failed").toBool(), false);

    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("fading"), 500);
    QVERIFY(presenter->property("cardVisible").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!presenter->property("cardVisible").toBool(), 500);

    delete root;
}

void OperationProgressPresenterTest::failureUsesFailureHoldAndStyling()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    presenter->setProperty("failureHoldMs", 35);
    presenter->setProperty("fadeOutMs", 20);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishFailedFile"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("title").toString(), QStringLiteral("Failed"), 500);
    QCOMPARE(presenter->property("terminalState").toString(), QStringLiteral("failed"));
    QCOMPARE(presenter->property("error").toString(), QStringLiteral("permission denied"));
    QCOMPARE(presenter->property("failed").toBool(), true);
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("fading"), 500);
    QTRY_VERIFY_WITH_TIMEOUT(!presenter->property("cardVisible").toBool(), 500);
    delete root;
}

void OperationProgressPresenterTest::partialSuccessUsesLongerHold()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    presenter->setProperty("partialSuccessHoldMs", 100);
    presenter->setProperty("fadeOutMs", 20);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishPartialFile"));
    QTRY_COMPARE_WITH_TIMEOUT(
        presenter->property("title").toString(), QStringLiteral("Completed with errors"), 500);
    QCOMPARE(presenter->property("terminalState").toString(), QStringLiteral("partial-success"));
    QCOMPARE(presenter->property("failed").toBool(), true);
    QTest::qWait(30);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("terminal"));
    QTRY_VERIFY_WITH_TIMEOUT(!presenter->property("cardVisible").toBool(), 500);
    delete root;
}

void OperationProgressPresenterTest::cancelledUsesCancelledPresentation()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    presenter->setProperty("cancelledHoldMs", 35);
    presenter->setProperty("fadeOutMs", 20);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishCancelledFile"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("title").toString(), QStringLiteral("Cancelled"), 500);
    QCOMPARE(presenter->property("terminalState").toString(), QStringLiteral("cancelled"));
    QCOMPARE(presenter->property("failed").toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!presenter->property("cardVisible").toBool(), 500);
    delete root;
}

void OperationProgressPresenterTest::archiveRunningIsIndeterminate()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    QVERIFY(QMetaObject::invokeMethod(root, "startArchive"));
    QTRY_VERIFY_WITH_TIMEOUT(presenter->property("cardVisible").toBool(), 500);
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("archive"));
    QCOMPARE(presenter->property("totalItems").toInt(), 0);
    QCOMPARE(presenter->property("percent").toInt(), 0);
    QCOMPARE(presenter->property("indeterminate").toBool(), true);
    delete root;
}

void OperationProgressPresenterTest::liveOperationPreemptsOldTerminalState()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    presenter->setProperty("successHoldMs", 80);
    presenter->setProperty("fadeOutMs", 20);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishFile"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("terminal"), 500);
    QVERIFY(QMetaObject::invokeMethod(root, "startArchive"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("running"), 500);
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("archive"));
    QTest::qWait(120);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QVERIFY(presenter->property("cardVisible").toBool());
    delete root;
}

void OperationProgressPresenterTest::archiveTerminalPresentationDoesNotBlockNewArchive()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    presenter->setProperty("successHoldMs", 100);
    presenter->setProperty("fadeOutMs", 20);
    QVERIFY(QMetaObject::invokeMethod(root, "startArchive"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishArchive"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("terminal"), 500);

    QVERIFY(QMetaObject::invokeMethod(root, "startArchive"));
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("archive"));
    QCOMPARE(presenter->property("terminal").toBool(), false);
    QTest::qWait(140);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QVERIFY(presenter->property("cardVisible").toBool());
    delete root;
}

void OperationProgressPresenterTest::liveProgressUpdatesWhileSemanticStateRemainsRunning()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    const QList<int> progressValues {25, 50, 75};
    for (const int value : progressValues) {
        QVERIFY(QMetaObject::invokeMethod(
            root, "setFileProgress", Q_ARG(QVariant, QVariant(value))));
        QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
        QCOMPARE(presenter->property("terminal").toBool(), false);
        QCOMPARE(presenter->property("percent").toInt(), value);
    }
    delete root;
}

void OperationProgressPresenterTest::fastCompletionKeepsRunningPresentationUntilMinimum()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 100);
    presenter->setProperty("successHoldMs", 1000);
    presenter->setProperty("fadeOutMs", 10);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishFile"));
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QCOMPARE(presenter->property("terminal").toBool(), false);
    QCOMPARE(presenter->property("title").toString(), QStringLiteral("Copying..."));
    QCOMPARE(presenter->property("percent").toInt(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("phase").toString(), QStringLiteral("terminal"), 500);
    QCOMPARE(presenter->property("terminal").toBool(), true);
    QCOMPARE(presenter->property("title").toString(), QStringLiteral("Completed"));
    delete root;
}

void OperationProgressPresenterTest::pendingTerminalIsPreemptedByNewOperation()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 100);
    presenter->setProperty("successHoldMs", 20);
    presenter->setProperty("fadeOutMs", 10);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishFile"));
    QCOMPARE(presenter->property("terminal").toBool(), false);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QCOMPARE(presenter->property("terminal").toBool(), false);
    QTest::qWait(140);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("file"));
    delete root;
}

void OperationProgressPresenterTest::fileProgressResumesAfterArchive()
{
    QQmlEngine engine;
    QString error;
    QObject *root = createRoot(&engine, presenterPath(), &error);
    QVERIFY2(root != nullptr, qPrintable(error));
    QObject *presenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (presenter = root->property("presenter").value<QObject *>()) != nullptr,
        1000);

    presenter->setProperty("minimumRunningDisplayMs", 0);
    QVERIFY(QMetaObject::invokeMethod(root, "startFile"));
    QVERIFY(QMetaObject::invokeMethod(root, "setFileProgress", Q_ARG(QVariant, QVariant(10))));
    QVERIFY(QMetaObject::invokeMethod(root, "startArchive"));
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("archive"));
    QVERIFY(QMetaObject::invokeMethod(root, "setFileProgress", Q_ARG(QVariant, QVariant(80))));
    QCOMPARE(presenter->property("activeKind").toString(), QStringLiteral("archive"));
    QVERIFY(QMetaObject::invokeMethod(root, "finishArchive"));
    QTRY_COMPARE_WITH_TIMEOUT(presenter->property("activeKind").toString(), QStringLiteral("file"), 500);
    QCOMPARE(presenter->property("percent").toInt(), 80);
    QCOMPARE(presenter->property("phase").toString(), QStringLiteral("running"));
    delete root;
}

QTEST_MAIN(OperationProgressPresenterTest)

#include "tst_operation_progress_presenter.moc"
