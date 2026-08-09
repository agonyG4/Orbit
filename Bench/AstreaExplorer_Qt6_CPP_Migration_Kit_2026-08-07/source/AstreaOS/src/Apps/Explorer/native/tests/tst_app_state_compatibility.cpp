#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTimer>
#include <QVariant>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"

using namespace Astrea::Explorer::Native::Backend;

namespace {

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

DirectoryEntry bridgeEntry(const QString &name, const QString &path)
{
    DirectoryEntry entry;
    entry.fileName = name;
    entry.filePath = path;
    entry.fileUrl = QUrl::fromLocalFile(path);
    return entry;
}

} // namespace

class AppStateCompatibilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void publicQmlSingletonDelegatesToNativeIdentity();
    void qmlAndNativeStatePropagateThroughNativeIdentity();
    void publicAppStateExposesNativeStateAfterPostLoadEvent();
};

void AppStateCompatibilityTest::publicQmlSingletonDelegatesToNativeIdentity()
{
    const QString qmlPath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    const QString appStateQml = readFile(qmlPath);
    QVERIFY2(!appStateQml.isEmpty(), qPrintable(qmlPath));
    QVERIFY(appStateQml.contains(QStringLiteral("import Astrea.Explorer.Native 1.0")));
    QVERIFY(appStateQml.contains(QStringLiteral("NativeAppState")));

    const QString applicationCpp = readFile(
        QStringLiteral(ASTREA_EXPLORER_NATIVE_SOURCE_ROOT)
        + QStringLiteral("/explorer_application.cpp"));
    QVERIFY2(!applicationCpp.isEmpty(), qPrintable(applicationCpp));
    QVERIFY(applicationCpp.contains(QStringLiteral("NativeAppState")));
    QVERIFY(!applicationCpp.contains(QStringLiteral("setContextProperty(QStringLiteral(\"AppState\")")));
    QVERIFY(!applicationCpp.contains(QStringLiteral("\"AppState\",\n        &appState")));
}

void AppStateCompatibilityTest::qmlAndNativeStatePropagateThroughNativeIdentity()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacade facade(&navigation, &selection, &model);

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native.Test",
        1,
        0,
        "NativeAppState",
        &facade);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        QByteArrayLiteral(
            "import QtQml 2.15\n"
            "import Astrea.Explorer.Native.Test 1.0\n"
            "QtObject {\n"
            "    property string observedPath: NativeAppState.currentPath\n"
            "    property int observedCount: NativeAppState.fileModel.count\n"
            "    property string observedSelection: NativeAppState.selectedFile\n"
            "    property string observedError: NativeAppState.loadError\n"
            "    function requestNavigation(path) { return NativeAppState.navigateTo(path) }\n"
            "    function select(name) { NativeAppState.selectByName(name) }\n"
            "}"),
        QUrl(QStringLiteral("qrc:/app-state-compatibility.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    QVERIFY(QMetaObject::invokeMethod(
        root,
        "requestNavigation",
        Q_ARG(QVariant, QVariant(QStringLiteral("/native-fixture")))));
    QCOMPARE(client.listRequests().size(), 1);
    QCOMPARE(client.listRequests().constFirst().path, QStringLiteral("/native-fixture"));
    const BackendRequestId requestId = 1;
    client.completeList(
        requestId,
        {bridgeEntry(QStringLiteral("one.txt"), QStringLiteral("/native-fixture/one.txt"))});
    QTRY_COMPARE(root->property("observedPath").toString(), QStringLiteral("/native-fixture"));
    QTRY_COMPARE(root->property("observedCount").toInt(), 1);

    QVERIFY(QMetaObject::invokeMethod(
        root,
        "select",
        Q_ARG(QVariant, QVariant(QStringLiteral("one.txt")))));
    QTRY_COMPARE(root->property("observedSelection").toString(), QStringLiteral("one.txt"));

    const BackendRequestId failedRequest = facade.navigateTo(QStringLiteral("/broken"));
    client.failRequest(
        failedRequest,
        QStringLiteral("backend_exit"),
        QStringLiteral("fixture navigation failed"));
    QTRY_COMPARE(
        root->property("observedError").toString(),
        QStringLiteral("fixture navigation failed"));

    delete root;
}

void AppStateCompatibilityTest::publicAppStateExposesNativeStateAfterPostLoadEvent()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacade facade(&navigation, &selection, &model);

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native",
        1,
        0,
        "NativeAppState",
        &facade);

    QQmlEngine engine;
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    if (!component.isReady()) {
        const QString error = component.errorString();
        if (error.contains(QStringLiteral("Quickshell"), Qt::CaseInsensitive)
            || error.contains(QStringLiteral("quickshell-ioplugin"), Qt::CaseInsensitive)) {
            QSKIP(qPrintable(QStringLiteral("Full Explorer QML dependencies are unavailable: %1").arg(error)));
        }
        QVERIFY2(false, qPrintable(error));
    }

    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    QTimer::singleShot(0, root, [root]() {
        QMetaObject::invokeMethod(
            root,
            "navigateTo",
            Q_ARG(QVariant, QVariant(QStringLiteral("/public-fixture"))));
    });
    QTRY_COMPARE(client.listRequests().size(), 1);
    QCOMPARE(client.listRequests().constFirst().path, QStringLiteral("/public-fixture"));

    client.completeList(
        1,
        {bridgeEntry(QStringLiteral("public.txt"), QStringLiteral("/public-fixture/public.txt"))});
    QTRY_COMPARE(root->property("currentPath").toString(), QStringLiteral("/public-fixture"));
    QObject *publicModel = root->property("fileModel").value<QObject *>();
    QVERIFY(publicModel != nullptr);
    QTRY_COMPARE(publicModel->property("count").toInt(), 1);

    facade.selectByName(QStringLiteral("public.txt"));
    QTRY_COMPARE(root->property("selectedFile").toString(), QStringLiteral("public.txt"));

    facade.setShowHidden(true);
    QTRY_VERIFY(root->property("showHidden").toBool());

    delete root;
}

QTEST_MAIN(AppStateCompatibilityTest)

#include "tst_app_state_compatibility.moc"
