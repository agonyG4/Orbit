#include <QFile>
#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
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

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

bool createQmlDependencyStubs(const QString &root, bool inheritedMarker = false)
{
    const QDir directory(root);
    if (!directory.mkpath(QStringLiteral("Quickshell/Io"))) {
        return false;
    }

    const QByteArray marker = inheritedMarker ? QByteArrayLiteral("1") : QByteArray();
    const QByteArray quickshellQml = QByteArrayLiteral(
        "pragma Singleton\n"
        "import QtQml 2.15\n"
        "QtObject { function env(name) { return name === \"ASTREA_EXPLORER_INHERITED_MARKER\" ? \"")
        + marker
        + QByteArrayLiteral("\" : \"\" } }\n");

    return writeFile(
               directory.filePath(QStringLiteral("Quickshell/qmldir")),
               QByteArrayLiteral(
                   "module Quickshell\n"
                   "singleton Quickshell 1.0 Quickshell.qml\n"
                   "FileView 1.0 FileView.qml\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Quickshell.qml")),
               quickshellQml)
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/FileView.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject {\n"
                   "    property string path: \"\"\n"
                   "    property bool preload: false\n"
                   "    property bool blockLoading: false\n"
                   "    property bool watchChanges: false\n"
                   "    property bool printErrors: false\n"
                   "    signal fileChanged()\n"
                   "}\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/qmldir")),
               QByteArrayLiteral(
                   "module Quickshell.Io\n"
                   "Process 1.0 Process.qml\n"
                   "StdioCollector 1.0 StdioCollector.qml\n"
                   "SplitParser 1.0 SplitParser.qml\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/Process.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject {\n"
                   "    property var command: []\n"
                   "    property bool running: false\n"
                   "    property bool stdinEnabled: false\n"
                   "    property QtObject stdout: null\n"
                   "    property QtObject stderr: null\n"
                   "    function write(data) {}\n"
                   "    signal started()\n"
                   "    signal exited(int exitCode)\n"
                   "}\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/StdioCollector.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject { property string text: \"\"; signal streamFinished() }\n"))
        && writeFile(
               directory.filePath(QStringLiteral("Quickshell/Io/SplitParser.qml")),
               QByteArrayLiteral(
                   "import QtQml 2.15\n"
                   "QtObject { signal read(string data) }\n"));
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
    void legacyRuntimeLoadsPublicAppStateWithoutNativeRegistration();
    void portalAndFileDialogPathsResolveThroughPublicAppState();
    void qmlAndNativeStatePropagateThroughNativeIdentity();
    void publicAppStateExposesNativeStateAfterPostLoadEvent();
};

void AppStateCompatibilityTest::publicQmlSingletonDelegatesToNativeIdentity()
{
    const QString qmlPath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    const QString appStateQml = readFile(qmlPath);
    QVERIFY2(!appStateQml.isEmpty(), qPrintable(qmlPath));
    QVERIFY(!appStateQml.contains(QStringLiteral("import Astrea.Explorer.Native 1.0")));
    QVERIFY(appStateQml.contains(QStringLiteral("compatibility/NativeAppStateAdapter.qml")));
    QVERIFY(appStateQml.contains(QStringLiteral("LegacyAppStateAdapter")));

    const QString applicationCpp = readFile(
        QStringLiteral(ASTREA_EXPLORER_NATIVE_SOURCE_ROOT)
        + QStringLiteral("/explorer_application.cpp"));
    QVERIFY2(!applicationCpp.isEmpty(), qPrintable(applicationCpp));
    QVERIFY(applicationCpp.contains(QStringLiteral("NativeAppState")));
    QVERIFY(!applicationCpp.contains(QStringLiteral("setContextProperty(QStringLiteral(\"AppState\")")));
    QVERIFY(!applicationCpp.contains(QStringLiteral("\"AppState\",\n        &appState")));
    const QString oldMarker = QStringLiteral("ASTREA_EXPLORER_") + QStringLiteral("NATIVE_RUNTIME");
    QVERIFY(!appStateQml.contains(oldMarker));
    QVERIFY(!applicationCpp.contains(oldMarker));
}

void AppStateCompatibilityTest::legacyRuntimeLoadsPublicAppStateWithoutNativeRegistration()
{
    QTemporaryDir stubs;
    QVERIFY(stubs.isValid());
    QVERIFY(createQmlDependencyStubs(stubs.path()));

    const bool hadMarker = qEnvironmentVariableIsSet("ASTREA_EXPLORER_INHERITED_MARKER");
    const QByteArray previousMarker = qgetenv("ASTREA_EXPLORER_INHERITED_MARKER");
    qputenv("ASTREA_EXPLORER_INHERITED_MARKER", QByteArrayLiteral("1"));
    QQmlEngine engine;
    engine.addImportPath(stubs.path());
    engine.rootContext()->setContextProperty(
        QStringLiteral("astreaNativeAppStateAvailable"), false);
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    QCOMPARE(root->property("nativeNavigationActive").toBool(), false);
    QVERIFY(root->property("nativeAppState").value<QObject *>() != nullptr);
    QObject *navigation = root->property("navigation").value<QObject *>();
    QVERIFY(navigation != nullptr);
    QCOMPARE(navigation->property("legacyProcessExecutionEnabled").toBool(), false);
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "navigateTo",
        Q_ARG(QVariant, QVariant(QStringLiteral("/legacy-fixture")))));
    QCOMPARE(root->property("currentPath").toString(), QString());
    delete root;

    if (hadMarker) {
        qputenv("ASTREA_EXPLORER_INHERITED_MARKER", previousMarker);
    } else {
        qunsetenv("ASTREA_EXPLORER_INHERITED_MARKER");
    }
}

void AppStateCompatibilityTest::portalAndFileDialogPathsResolveThroughPublicAppState()
{
    QTemporaryDir stubs;
    QVERIFY(stubs.isValid());
    QVERIFY(createQmlDependencyStubs(stubs.path()));

    QQmlEngine engine;
    engine.addImportPath(stubs.path());
    const QString runtimeRoot = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/");
    QQmlComponent fileDialog(&engine, QUrl::fromLocalFile(runtimeRoot + QStringLiteral("FileDialog.qml")));
    QVERIFY2(fileDialog.isReady(), qPrintable(fileDialog.errorString()));
    QQmlComponent portalDialog(
        &engine,
        QUrl::fromLocalFile(runtimeRoot + QStringLiteral("PortalDialog.qml")));
    QVERIFY2(portalDialog.isReady(), qPrintable(portalDialog.errorString()));

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

    QTemporaryDir stubs;
    QVERIFY(stubs.isValid());
    QVERIFY(createQmlDependencyStubs(stubs.path()));
    QQmlEngine engine;
    engine.addImportPath(stubs.path());
    engine.rootContext()->setContextProperty(
        QStringLiteral("astreaNativeAppStateAvailable"), true);
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    if (!component.isReady()) {
        const QString error = component.errorString();
        QVERIFY2(false, qPrintable(error));
    }

    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    QTRY_VERIFY(root->property("nativeAppState").value<QObject *>() != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(
        root->property("nativeAppState").value<QObject *>()->property("nativeFacade").toBool(),
        true,
        3000);
    QObject *nativeNavigation = root->property("navigation").value<QObject *>();
    QVERIFY(nativeNavigation != nullptr);
    QCOMPARE(nativeNavigation->property("legacyProcessExecutionEnabled").toBool(), false);

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

    QVariantMap recentItem;
    recentItem.insert(QStringLiteral("fileName"), QStringLiteral("Recent application"));
    recentItem.insert(
        QStringLiteral("filePath"),
        QStringLiteral("/public-fixture/org.example.Recent.desktop"));
    recentItem.insert(QStringLiteral("fileKind"), QStringLiteral("Aplicativo"));
    recentItem.insert(QStringLiteral("fileIconName"), QStringLiteral("recent-application"));
    recentItem.insert(QStringLiteral("lastAccessed"), 9876);
    recentItem.insert(QStringLiteral("fileModified"), 9876);
    recentItem.insert(QStringLiteral("recentSource"), QStringLiteral("launch"));
    QVERIFY(facade.replaceFileModel(QVariantList {recentItem}));
    QTRY_COMPARE(publicModel->property("count").toInt(), 1);
    QVariantMap visibleRecent;
    QVERIFY(QMetaObject::invokeMethod(
        publicModel,
        "get",
        Q_RETURN_ARG(QVariantMap, visibleRecent),
        Q_ARG(int, 0)));
    QCOMPARE(
        visibleRecent.value(QStringLiteral("fileName")).toString(),
        QStringLiteral("Recent application"));
    QCOMPARE(
        visibleRecent.value(QStringLiteral("fileIconName")).toString(),
        QStringLiteral("recent-application"));
    QCOMPARE(visibleRecent.value(QStringLiteral("lastAccessed")).toLongLong(), 9876);

    QVariantMap existing;
    existing.insert(QStringLiteral("fileName"), QStringLiteral("public.txt"));
    existing.insert(QStringLiteral("filePath"), QStringLiteral("/public-fixture/public.txt"));
    existing.insert(QStringLiteral("fileKind"), QStringLiteral("TXT"));
    QVariantMap replacement;
    replacement.insert(QStringLiteral("fileName"), QStringLiteral("replacement.txt"));
    replacement.insert(QStringLiteral("filePath"), QStringLiteral("/public-fixture/replacement.txt"));
    replacement.insert(QStringLiteral("fileKind"), QStringLiteral("TXT"));
    QVERIFY(QMetaObject::invokeMethod(
        root,
        "replaceFileModel",
        Q_ARG(QVariant, QVariant(QVariantList {existing, replacement}))));
    QTRY_COMPARE(publicModel->property("count").toInt(), 2);
    QVERIFY(root->property("fileModelRevision").toInt() > 0);

    facade.selectByName(QStringLiteral("public.txt"));
    QTRY_COMPARE(root->property("selectedFile").toString(), QStringLiteral("public.txt"));

    facade.setShowHidden(true);
    QTRY_VERIFY(root->property("showHidden").toBool());

    delete root;
}

QTEST_MAIN(AppStateCompatibilityTest)

#include "tst_app_state_compatibility.moc"
