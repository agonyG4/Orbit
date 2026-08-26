#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include <memory>

#include "backend/fake_backend_client.h"
#include "controllers/file_operations_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/file_operation_service.h"
#include "services/filesystem_service.h"

#define private public
#include "controllers/archive_controller.h"
#include "controllers/app_state_facade.h"
#undef private

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

struct ArchiveFacadeFixture final
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation;
    SelectionController selection;
    Astrea::Explorer::Native::Services::FilesystemService filesystem;
    ArchiveController archive;
    AppStateFacadeDependencies dependencies;
    std::unique_ptr<AppStateFacade> facade;

    ArchiveFacadeFixture()
        : navigation(&client, &model, &watcher)
        , selection(&model)
        , filesystem(&client)
        , archive(&filesystem, &navigation)
    {
        dependencies.navigation = &navigation;
        dependencies.selection = &selection;
        dependencies.model = &model;
        dependencies.archive = &archive;
        dependencies.filesystem = &filesystem;
        facade = std::make_unique<AppStateFacade>(dependencies);
    }
};

} // namespace

class AppStateCompatibilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void directNativeBoundaryUsesRegisteredSingleton();
    void publicAppStateRequiresRegisteredNativeRuntime();
    void portalAndFileDialogPathsResolveThroughPublicAppState();
    void qmlAndNativeStatePropagateThroughNativeIdentity();
    void reactiveSelectionMembershipUpdatesAfterReplacement();
    void publicAppStateExposesNativeStateAfterPostLoadEvent();
    void appStatePublishesAggregateOperationSnapshots();
    void appStatePublishesArchiveOperationSnapshots();
    void archiveAdmissionRejectsExtractionDuringPasswordContinuation();
    void archiveAdmissionRejectsCompressionDuringPasswordContinuation();
    void archiveAdmissionRejectsExtractionDuringConflictContinuation();
    void archiveAdmissionRejectsCompressionDuringConflictContinuation();
    void archiveWorkflowOccupancyReportsAllStates();
};

void AppStateCompatibilityTest::directNativeBoundaryUsesRegisteredSingleton()
{
    const QString qmlPath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    const QString appStateQml = readFile(qmlPath);
    QVERIFY2(!appStateQml.isEmpty(), qPrintable(qmlPath));
    QVERIFY(appStateQml.contains(QStringLiteral("import Astrea.Explorer.Native 1.0")));
    QVERIFY(appStateQml.contains(QStringLiteral("readonly property QtObject nativeAppState: NativeAppState")));
    QVERIFY(!appStateQml.contains(QStringLiteral("Loader")));
    QVERIFY(!appStateQml.contains(QStringLiteral("NativeAppStateAdapter")));
    QVERIFY(!appStateQml.contains(QStringLiteral("LegacyAppStateAdapter")));
    QVERIFY(!appStateQml.contains(QStringLiteral("StateModules.SelectionState")));
    QVERIFY(!appStateQml.contains(QStringLiteral("StateModules.NavigationState")));
    QVERIFY(!appStateQml.contains(QStringLiteral("StateModules.RecentState")));

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
    QVERIFY(!applicationCpp.contains(QStringLiteral("astreaNativeAppStateAvailable")));
}

void AppStateCompatibilityTest::publicAppStateRequiresRegisteredNativeRuntime()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    QVERIFY(!component.isReady());
    QVERIFY(component.errorString().contains(QStringLiteral("Astrea.Explorer.Native")));
}

void AppStateCompatibilityTest::portalAndFileDialogPathsResolveThroughPublicAppState()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacade facade(&navigation, &selection, &model);
    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native", 1, 0, "NativeAppState", &facade);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));
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

void AppStateCompatibilityTest::reactiveSelectionMembershipUpdatesAfterReplacement()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacade facade(&navigation, &selection, &model);
    QVERIFY(model.applyEntries(
        {bridgeEntry(QStringLiteral("one.txt"), QStringLiteral("/fixture/one.txt")),
         bridgeEntry(QStringLiteral("two.txt"), QStringLiteral("/fixture/two.txt")),
         bridgeEntry(QStringLiteral("three.txt"), QStringLiteral("/fixture/three.txt"))},
        1));

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native",
        1,
        0,
        "NativeAppState",
        &facade);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));

    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent appStateComponent(&engine, QUrl::fromLocalFile(appStatePath));
    QVERIFY2(appStateComponent.isReady(), qPrintable(appStateComponent.errorString()));
    QObject *appState = appStateComponent.create();
    QVERIFY2(appState != nullptr, qPrintable(appStateComponent.errorString()));
    QCOMPARE(appState->property("nativeAppState").value<QObject *>(), static_cast<QObject *>(&facade));

    engine.rootContext()->setContextProperty(QStringLiteral("AppStateUnderTest"), appState);
    QQmlComponent fixture(&engine);
    fixture.setData(
        QByteArrayLiteral(
            "import QtQml 2.15\n"
            "QtObject {\n"
            "    property string pathA: \"/fixture/one.txt\"\n"
            "    property string pathB: \"/fixture/two.txt\"\n"
            "    property string pathC: \"/fixture/three.txt\"\n"
            "    property bool aSelected: AppStateUnderTest.isPathSelected(pathA)\n"
            "    property bool bSelected: AppStateUnderTest.isPathSelected(pathB)\n"
            "    property bool cSelected: AppStateUnderTest.isPathSelected(pathC)\n"
            "    property bool aNameSelected: AppStateUnderTest.isSelected(\"one.txt\")\n"
            "    property bool bNameSelected: AppStateUnderTest.isSelected(\"two.txt\")\n"
            "    property bool cNameSelected: AppStateUnderTest.isSelected(\"three.txt\")\n"
            "}"),
        QUrl(QStringLiteral("qrc:/reactive-selection.qml")));
    QVERIFY2(fixture.isReady(), qPrintable(fixture.errorString()));
    QObject *root = fixture.create();
    QVERIFY2(root != nullptr, qPrintable(fixture.errorString()));

    selection.handleSelection(QStringLiteral("one.txt"), 0, false, false, false);
    QCOMPARE(selection.selectedPaths(), QStringList({QStringLiteral("/fixture/one.txt")}));
    QTRY_COMPARE_WITH_TIMEOUT(root->property("aSelected").toBool(), true, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("bSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("cSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("aNameSelected").toBool(), true, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("bNameSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("cNameSelected").toBool(), false, 1000);

    selection.handleSelection(QStringLiteral("two.txt"), 1, false, false, false);
    QCOMPARE(selection.selectedPaths(), QStringList({QStringLiteral("/fixture/two.txt")}));
    QTRY_COMPARE_WITH_TIMEOUT(root->property("aSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("bSelected").toBool(), true, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("cSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("aNameSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("bNameSelected").toBool(), true, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("cNameSelected").toBool(), false, 1000);

    selection.handleSelection(QStringLiteral("three.txt"), 2, false, false, false);
    QCOMPARE(selection.selectedPaths(), QStringList({QStringLiteral("/fixture/three.txt")}));
    QTRY_COMPARE_WITH_TIMEOUT(root->property("aSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("bSelected").toBool(), false, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(root->property("cSelected").toBool(), true, 1000);

    delete root;
    delete appState;
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
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    if (!component.isReady()) {
        const QString error = component.errorString();
        QVERIFY2(false, qPrintable(error));
    }

    QObject *root = component.create();
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));

    QCOMPARE(root->property("nativeAppState").value<QObject *>(), static_cast<QObject *>(&facade));

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

void AppStateCompatibilityTest::appStatePublishesAggregateOperationSnapshots()
{
    FakeRustBackendClient client;
    Astrea::Explorer::Native::Services::FileOperationService service(&client);
    FileOperationsController operations(&service);
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    AppStateFacadeDependencies dependencies;
    dependencies.navigation = &navigation;
    dependencies.selection = &selection;
    dependencies.model = &model;
    dependencies.fileOperations = &operations;
    AppStateFacade facade(dependencies);

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native",
        1,
        0,
        "NativeAppState",
        &facade);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *appState = component.create();
    QVERIFY2(appState != nullptr, qPrintable(component.errorString()));
    QObject *fileOps = appState->property("fileOps").value<QObject *>();
    QVERIFY(fileOps != nullptr);

    QSignalSpy fileSpy(fileOps, SIGNAL(fileOperationChanged(QVariant)));
    QSignalSpy archiveSpy(fileOps, SIGNAL(archiveOperationChanged(QVariant)));
    const BackendRequestId requestId = operations.transferFiles(
        {QStringLiteral("/tmp/source.txt")},
        QStringLiteral("/tmp/destination"),
        QStringLiteral("copy"));
    QVERIFY(requestId > 0);
    QTRY_COMPARE(fileSpy.count(), 1);
    QCOMPARE(fileSpy.at(0).at(0).toMap().value(QStringLiteral("running")).toBool(), true);
    QCOMPARE(fileSpy.at(0).at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("running"));

    FileOperationProgress progress;
    progress.requestId = requestId;
    progress.doneCount = 1;
    progress.totalCount = 2;
    progress.percent = 50;
    progress.fileName = QStringLiteral("source.txt");
    client.completeFileOperationProgress(requestId, progress);
    QTRY_COMPARE(fileSpy.count(), 2);
    QCOMPARE(fileSpy.at(1).at(0).toMap().value(QStringLiteral("percent")).toInt(), 50);
    QCOMPARE(fileSpy.at(1).at(0).toMap().value(QStringLiteral("running")).toBool(), true);

    FileOperationResult result;
    result.requestId = requestId;
    result.ok = true;
    result.doneCount = 2;
    result.totalCount = 2;
    result.percent = 100;
    client.completeFileOperation(requestId, result);
    QTRY_COMPARE(fileSpy.count(), 3);
    QCOMPARE(fileSpy.at(2).at(0).toMap().value(QStringLiteral("running")).toBool(), false);
    QCOMPARE(fileSpy.at(2).at(0).toMap().value(QStringLiteral("state")).toString(), QStringLiteral("success"));
    QCOMPARE(archiveSpy.count(), 0);

    delete appState;
}

void AppStateCompatibilityTest::appStatePublishesArchiveOperationSnapshots()
{
    FakeRustBackendClient client;
    DirectoryModel model;
    DirectoryWatchService watcher;
    NavigationController navigation(&client, &model, &watcher);
    SelectionController selection(&model);
    Astrea::Explorer::Native::Services::FilesystemService filesystem(&client);
    ArchiveController archive(&filesystem, &navigation);
    AppStateFacadeDependencies dependencies;
    dependencies.navigation = &navigation;
    dependencies.selection = &selection;
    dependencies.model = &model;
    dependencies.archive = &archive;
    dependencies.filesystem = &filesystem;
    AppStateFacade facade(dependencies);

    qmlRegisterSingletonInstance<AppStateFacade>(
        "Astrea.Explorer.Native",
        1,
        0,
        "NativeAppState",
        &facade);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT));
    const QString appStatePath = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/AppState.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(appStatePath));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QObject *appState = component.create();
    QVERIFY2(appState != nullptr, qPrintable(component.errorString()));
    QObject *fileOps = appState->property("fileOps").value<QObject *>();
    QVERIFY(fileOps != nullptr);

    QSignalSpy archiveSpy(fileOps, SIGNAL(archiveOperationChanged(QVariant)));
    facade.startArchiveExtraction(QStringLiteral("/tmp/source.zip"), QStringLiteral("expanded"));
    QTRY_COMPARE(archiveSpy.count(), 1);
    const QVariantMap started = archiveSpy.at(0).at(0).toMap();
    QCOMPARE(started.value(QStringLiteral("running")).toBool(), true);
    QCOMPARE(started.value(QStringLiteral("progress")).toDouble(), 0.0);
    QCOMPARE(started.value(QStringLiteral("percent")).toInt(), 0);
    QCOMPARE(started.value(QStringLiteral("doneCount")).toInt(), 0);
    QCOMPARE(started.value(QStringLiteral("totalCount")).toInt(), 0);
    QCOMPARE(started.value(QStringLiteral("status")).toString(), QStringLiteral("Extraindo..."));
    QCOMPARE(
        started.value(QStringLiteral("destination")).toString(),
        facade.archiveExtractionDestination());

    UtilityResult success;
    success.operation = QStringLiteral("archive-extract");
    success.ok = true;
    success.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/actual-expanded"));
    client.completeUtility(1, success);
    QTRY_COMPARE(archiveSpy.count(), 2);
    const QVariantMap completed = archiveSpy.at(1).at(0).toMap();
    QCOMPARE(completed.value(QStringLiteral("running")).toBool(), false);
    QCOMPARE(completed.value(QStringLiteral("progress")).toDouble(), 1.0);
    QCOMPARE(completed.value(QStringLiteral("percent")).toInt(), 100);
    QCOMPARE(completed.value(QStringLiteral("doneCount")).toInt(), 1);
    QCOMPARE(completed.value(QStringLiteral("totalCount")).toInt(), 1);
    QCOMPARE(completed.value(QStringLiteral("error")).toString(), QString());
    QCOMPARE(
        completed.value(QStringLiteral("destination")).toString(),
        QStringLiteral("/tmp/actual-expanded"));

    const QString runtimeRoot = QStringLiteral(ASTREA_EXPLORER_RUNTIME_ROOT)
        + QStringLiteral("/Apps/Explorer/");
    const QString fileOperationsStatePath = runtimeRoot
        + QStringLiteral("state/FileOperationsState.qml");
    const QString presenterPath = runtimeRoot
        + QStringLiteral("components/common/OperationProgressPresenter.qml");
    QQmlComponent pipelineComponent(&engine);
    pipelineComponent.setData(
        QStringLiteral(
            "import QtQuick 2.15\n"
            "import QtQml 2.15\n"
            "Item {\n"
            "    Loader {\n"
            "        id: stateLoader\n"
            "        source: \"%1\"\n"
            "        onLoaded: item.app = AppStateUnderTest\n"
            "    }\n"
            "    Loader {\n"
            "        id: presenterLoader\n"
            "        source: \"%2\"\n"
            "        onLoaded: {\n"
            "            item.operationState = stateLoader.item\n"
            "            item.minimumRunningDisplayMs = 0\n"
            "            item.successHoldMs = 1000\n"
            "            item.fadeOutMs = 20\n"
            "        }\n"
            "    }\n"
            "    property QtObject pipelineState: stateLoader.item\n"
            "    property QtObject pipelinePresenter: presenterLoader.item\n"
            "    onPipelineStateChanged: if (pipelinePresenter) pipelinePresenter.operationState = pipelineState\n"
            "    onPipelinePresenterChanged: if (pipelinePresenter) pipelinePresenter.operationState = pipelineState\n"
            "}")
            .arg(QUrl::fromLocalFile(fileOperationsStatePath).toString())
            .arg(QUrl::fromLocalFile(presenterPath).toString())
            .toUtf8(),
        QUrl(QStringLiteral("qrc:/archive-operation-pipeline.qml")));
    QVERIFY2(pipelineComponent.isReady(), qPrintable(pipelineComponent.errorString()));
    engine.rootContext()->setContextProperty(QStringLiteral("AppStateUnderTest"), appState);
    QObject *pipeline = pipelineComponent.create();
    QVERIFY2(pipeline != nullptr, qPrintable(pipelineComponent.errorString()));
    QObject *pipelinePresenter = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (pipelinePresenter = pipeline->property("pipelinePresenter").value<QObject *>()) != nullptr,
        1000);
    QObject *pipelineState = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (pipelineState = pipeline->property("pipelineState").value<QObject *>()) != nullptr,
        1000);
    QTRY_VERIFY_WITH_TIMEOUT(
        pipelineState->property("bridge").value<QObject *>() != nullptr,
        1000);
    QSignalSpy pipelineArchiveSpy(pipelineState, SIGNAL(archiveOperationChanged(QVariant)));

    QCOMPARE(
        pipelineState->property("archivePasswordError").toString(),
        appState->property("archivePasswordError").toString());
    QCOMPARE(
        pipelineState->property("archiveConflictVisible").toBool(),
        appState->property("archiveConflictVisible").toBool());
    QCOMPARE(
        pipelineState->property("archiveConflictDestination").toString(),
        appState->property("archiveConflictDestination").toString());
    QCOMPARE(
        pipelineState->property("archiveConflictName").toString(),
        appState->property("archiveConflictName").toString());
    QCOMPARE(
        pipelineState->property("appImageInstallRunning").toBool(),
        appState->property("appImageInstallRunning").toBool());
    QCOMPARE(
        pipelineState->property("wallpaperApplyRunning").toBool(),
        appState->property("wallpaperApplyRunning").toBool());

    archive.m_passwordError = QStringLiteral("incorrect password");
    archive.m_conflict = true;
    archive.m_conflictDestination = QStringLiteral("/tmp/existing");
    archive.m_conflictName = QStringLiteral("existing");
    facade.m_appImageInstallRunning = true;
    facade.m_wallpaperApplyRunning = true;
    QSignalSpy facadeArchiveSpy(&facade, &AppStateFacade::archiveStateChanged);
    QCOMPARE(facade.archivePasswordError(), QStringLiteral("incorrect password"));
    QCOMPARE(facade.archiveConflictVisible(), true);
    archive.publishState();
    QTRY_COMPARE(facadeArchiveSpy.count(), 1);
    QTRY_COMPARE(
        appState->property("archivePasswordError").toString(),
        QStringLiteral("incorrect password"));
    QTRY_COMPARE(
        pipelineState->property("archivePasswordError").toString(),
        QStringLiteral("incorrect password"));
    QTRY_VERIFY(pipelineState->property("archiveConflictVisible").toBool());
    QTRY_COMPARE(
        pipelineState->property("archiveConflictDestination").toString(),
        QStringLiteral("/tmp/existing"));
    QTRY_COMPARE(
        pipelineState->property("archiveConflictName").toString(),
        QStringLiteral("existing"));
    emit facade.wallpaperStateChanged();
    QTRY_VERIFY(pipelineState->property("appImageInstallRunning").toBool());
    QTRY_VERIFY(pipelineState->property("wallpaperApplyRunning").toBool());

    archive.m_conflict = false;
    facade.startFolderCompression(QStringLiteral("/tmp/folder"), QStringLiteral("zip"));
    QTRY_COMPARE(archiveSpy.count(), 4);
    QTRY_COMPARE(pipelineArchiveSpy.count(), 2);
    QTRY_COMPARE(pipelinePresenter->property("activeKind").toString(), QStringLiteral("archive"));
    QCOMPARE(pipelinePresenter->property("phase").toString(), QStringLiteral("running"));
    QCOMPARE(pipelinePresenter->property("indeterminate").toBool(), true);
    const BackendRequestId compressionRequestId = static_cast<BackendRequestId>(
        client.listRequests().size() + client.utilityRequests().size());
    UtilityResult compressionSuccess;
    compressionSuccess.operation = QStringLiteral("archive-compress");
    compressionSuccess.ok = true;
    compressionSuccess.data.insert(QStringLiteral("destination"), QStringLiteral("/tmp/folder.zip"));
    client.completeUtility(compressionRequestId, compressionSuccess);
    QTRY_COMPARE(archiveSpy.count(), 5);
    QTRY_VERIFY(!facade.archiveExtractionRunning());
    QTRY_COMPARE(pipelinePresenter->property("phase").toString(), QStringLiteral("terminal"));
    QCOMPARE(pipelinePresenter->property("title").toString(), QStringLiteral("Completed"));
    QCOMPARE(pipelinePresenter->property("percent").toInt(), 100);

    facade.startArchiveExtraction(QStringLiteral("/tmp/failing.zip"), QStringLiteral("failed"));
    QTRY_COMPARE(archiveSpy.count(), 6);
    const BackendRequestId failedRequestId = static_cast<BackendRequestId>(
        client.listRequests().size() + client.utilityRequests().size());
    client.failRequest(
        failedRequestId,
        QStringLiteral("permission_denied"),
        QStringLiteral("archive destination is not writable"));
    QTRY_COMPARE(archiveSpy.count(), 7);
    const QVariantMap failed = archiveSpy.at(6).at(0).toMap();
    QCOMPARE(failed.value(QStringLiteral("running")).toBool(), false);
    QCOMPARE(failed.value(QStringLiteral("progress")).toDouble(), 0.0);
    QCOMPARE(failed.value(QStringLiteral("percent")).toInt(), 0);
    QCOMPARE(failed.value(QStringLiteral("error")).toString(), QStringLiteral("archive destination is not writable"));
    QCOMPARE(failed.value(QStringLiteral("status")).toString(), QStringLiteral("Falha"));

    delete pipeline;
    delete appState;
}

void AppStateCompatibilityTest::archiveAdmissionRejectsExtractionDuringPasswordContinuation()
{
    ArchiveFacadeFixture fixture;
    fixture.archive.m_path = QStringLiteral("/tmp/pending.zip");
    fixture.archive.m_passwordPrompt = true;
    const int revision = fixture.archive.stateRevision();
    const int requestCount = fixture.client.utilityRequests().size();

    fixture.facade->startArchiveExtraction(
        QStringLiteral("/tmp/replacement.zip"), QStringLiteral("replacement"));

    QCOMPARE(fixture.archive.stateRevision(), revision);
    QCOMPARE(fixture.client.utilityRequests().size(), requestCount);
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
}

void AppStateCompatibilityTest::archiveAdmissionRejectsCompressionDuringPasswordContinuation()
{
    ArchiveFacadeFixture fixture;
    fixture.archive.m_path = QStringLiteral("/tmp/pending.zip");
    fixture.archive.m_passwordPrompt = true;
    const int revision = fixture.archive.stateRevision();
    const int requestCount = fixture.client.utilityRequests().size();

    fixture.facade->startFolderCompression(QStringLiteral("/tmp/replacement"), QStringLiteral("zip"));

    QCOMPARE(fixture.archive.stateRevision(), revision);
    QCOMPARE(fixture.client.utilityRequests().size(), requestCount);
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
}

void AppStateCompatibilityTest::archiveAdmissionRejectsExtractionDuringConflictContinuation()
{
    ArchiveFacadeFixture fixture;
    fixture.archive.m_path = QStringLiteral("/tmp/pending.zip");
    fixture.archive.m_conflict = true;
    const int revision = fixture.archive.stateRevision();
    const int requestCount = fixture.client.utilityRequests().size();

    fixture.facade->startArchiveExtraction(
        QStringLiteral("/tmp/replacement.zip"), QStringLiteral("replacement"));

    QCOMPARE(fixture.archive.stateRevision(), revision);
    QCOMPARE(fixture.client.utilityRequests().size(), requestCount);
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
}

void AppStateCompatibilityTest::archiveAdmissionRejectsCompressionDuringConflictContinuation()
{
    ArchiveFacadeFixture fixture;
    fixture.archive.m_path = QStringLiteral("/tmp/pending.zip");
    fixture.archive.m_conflict = true;
    const int revision = fixture.archive.stateRevision();
    const int requestCount = fixture.client.utilityRequests().size();

    fixture.facade->startFolderCompression(QStringLiteral("/tmp/replacement"), QStringLiteral("zip"));

    QCOMPARE(fixture.archive.stateRevision(), revision);
    QCOMPARE(fixture.client.utilityRequests().size(), requestCount);
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
}

void AppStateCompatibilityTest::archiveWorkflowOccupancyReportsAllStates()
{
    ArchiveFacadeFixture fixture;
    QVERIFY(!fixture.facade->archiveWorkflowOccupied());

    fixture.archive.m_running = true;
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
    fixture.archive.m_running = false;
    fixture.archive.m_passwordPrompt = true;
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
    fixture.archive.m_passwordPrompt = false;
    fixture.archive.m_conflict = true;
    QVERIFY(fixture.facade->archiveWorkflowOccupied());
}

QTEST_MAIN(AppStateCompatibilityTest)

#include "tst_app_state_compatibility.moc"
