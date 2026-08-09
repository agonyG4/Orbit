#include "explorer_application.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QUrl>
#include <QString>
#include <QVariant>

#include <QDebug>

#include <QtQml/qqml.h>

#include "backend/one_shot_cli_transport.h"
#include "backend/rust_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/device_controller.h"
#include "controllers/file_operations_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/open_with_controller.h"
#include "controllers/portal_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "runtime/explorer_runtime_paths.h"
#include "services/clipboard_service.h"
#include "services/directory_watch_service.h"
#include "services/file_operation_service.h"
#include "services/launch_service.h"
#include "services/settings_service.h"

namespace {
constexpr auto kApplicationName = "Explorer";
constexpr auto kOrganizationName = "agony";
constexpr auto kOrganizationDomain = "local";
constexpr auto kBootstrapModuleUri = "Astrea.Explorer.Native";
constexpr auto kBootstrapTypeName = "NativeBootstrap";
constexpr auto kSelfTestArgument = "--self-test";
constexpr auto kBootstrapArgument = "--bootstrap";
}

int ExplorerApplication::run(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QString::fromLatin1(kApplicationName));
    QCoreApplication::setOrganizationName(QString::fromLatin1(kOrganizationName));
    QCoreApplication::setOrganizationDomain(QString::fromLatin1(kOrganizationDomain));

    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const bool useBootstrap = application.arguments().contains(
        QString::fromLatin1(kBootstrapArgument));
    const auto runtimePaths = Astrea::Explorer::Native::Runtime::ExplorerRuntimeResolver::resolve(
        QCoreApplication::applicationDirPath(),
        QDir::homePath(),
        environment);
    if (!runtimePaths.valid && !useBootstrap) {
        for (const QString &diagnostic : runtimePaths.diagnostics) {
            qCritical().noquote() << diagnostic;
            QTextStream(stderr) << diagnostic << Qt::endl;
        }
        return 1;
    }
    if (runtimePaths.valid && !environment.contains(QStringLiteral("ASTREA_ROOT"))) {
        qputenv("ASTREA_ROOT", runtimePaths.root.toUtf8());
    }

    using namespace Astrea::Explorer::Native::Backend;
    using namespace Astrea::Explorer::Native::Services;
    OneShotCliTransportOptions transportOptions;
    if (runtimePaths.valid) {
        transportOptions.backendProgram = runtimePaths.backendProgram;
    }
    OneShotCliTransport transport(transportOptions, &application);
    RustBackendClient backendClient(&transport, &application);
    DirectoryModel directoryModel(&application);
    DirectoryWatchService directoryWatcher(&application);
    SettingsService settings(
        QDir(QDir::homePath()).filePath(QStringLiteral(".config/explorer.conf")));
    const ExplorerSettings initialSettings = settings.load();
    ClipboardService clipboard(QGuiApplication::clipboard());
    LaunchService launchService(
        runtimePaths.launcherProgram,
        runtimePaths.windowsRunnerProgram);
    FileOperationService fileOperationService(&backendClient, &application);
    FileOperationsController fileOperations(&fileOperationService, &application);
    DeviceController devices(
        &backendClient,
        &application,
        initialSettings.autoMountDeviceIdsJson);
    OpenWithController openWith(&launchService, &application);
    PortalController portal(&application);
    NavigationController navigation(
        &backendClient,
        &directoryModel,
        &directoryWatcher,
        &application);
    SelectionController selection(&directoryModel, &application);
    AppStateFacade appState(
        &navigation,
        &selection,
        &directoryModel,
        &application,
        &settings,
        &fileOperations,
        &devices,
        runtimePaths);
    qmlRegisterSingletonInstance<AppStateFacade>(
        kBootstrapModuleUri,
        1,
        0,
        "AppState",
        &appState);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("AppState"), &appState);

    const bool loaded = useBootstrap
        ? loadBootstrap(engine)
        : loadExplorerQml(engine, runtimePaths);
    if (!loaded) {
        return 1;
    }

    if (navigation.currentPath().isEmpty()) {
        navigation.navigateTo(
            initialSettings.currentPath.isEmpty()
                ? QDir::homePath()
                : initialSettings.currentPath);
    }

    if (application.arguments().contains(QString::fromLatin1(kSelfTestArgument))) {
        return runSelfTest(engine);
    }

    return application.exec();
}

bool ExplorerApplication::loadExplorerQml(
    QQmlApplicationEngine &engine,
    const Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths &paths) const
{
    for (const QString &importPath : paths.importPaths) {
        engine.addImportPath(importPath);
    }

    QStringList warnings;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        &engine,
        [&warnings](const QList<QQmlError> &errors) {
            for (const QQmlError &error : errors) {
                warnings.append(error.toString());
                qWarning().noquote() << error.toString();
            }
        });

    const QUrl mainUrl = QUrl::fromLocalFile(paths.explorerMainQml);
    qInfo().noquote() << "Loading Explorer QML:" << mainUrl.toLocalFile();
    QTextStream(stderr) << "Loading Explorer QML: " << mainUrl.toLocalFile() << Qt::endl;
    engine.load(mainUrl);

    if (engine.rootObjects().isEmpty()) {
        QTextStream(stderr) << "Explorer QML produced no root object" << Qt::endl;
    }
    for (const QString &warning : warnings) {
        QTextStream(stderr) << warning << Qt::endl;
    }

    return !engine.rootObjects().isEmpty() && warnings.isEmpty();
}

bool ExplorerApplication::loadBootstrap(QQmlApplicationEngine &engine) const
{
    engine.loadFromModule(
        QLatin1StringView(kBootstrapModuleUri),
        QLatin1StringView(kBootstrapTypeName));

    return !engine.rootObjects().isEmpty();
}

int ExplorerApplication::runSelfTest(QQmlApplicationEngine &engine) const
{
    const auto rootObjects = engine.rootObjects();
    return rootObjects.isEmpty() || rootObjects.constFirst() == nullptr ? 1 : 0;
}
