#include "explorer_application.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QString>
#include <QDebug>

#include <QtQml/qqml.h>

#include "backend/persistent_worker_transport.h"
#include "backend/rust_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/device_controller.h"
#include "controllers/file_operations_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/open_with_controller.h"
#include "controllers/portal_controller.h"
#include "controllers/recent_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "runtime/astrea_icon_image_provider.h"
#include "runtime/explorer_runtime_paths.h"
#include "services/clipboard_service.h"
#include "services/directory_watch_service.h"
#include "services/desktop_application_catalog.h"
#include "services/file_operation_service.h"
#include "services/filesystem_service.h"
#include "services/launch_service.h"
#include "services/settings_service.h"

namespace {
constexpr auto kApplicationName = "Explorer";
constexpr auto kOrganizationName = "agony";
constexpr auto kOrganizationDomain = "local";
constexpr auto kBootstrapModuleUri = "Astrea.Explorer.Native";
constexpr auto kBootstrapTypeName = "NativeBootstrap";
constexpr auto kNativeAppStateTypeName = "NativeAppState";
constexpr auto kSelfTestArgument = "--self-test";
constexpr auto kBootstrapArgument = "--bootstrap";

QVariantMap readJsonMap(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object().toVariantMap() : QVariantMap{};
}

QString configuredLanguage(const QString &configRoot)
{
    const QString configPath = QDir(configRoot).filePath(
        QStringLiteral("AstreaOS/system/settings.json"));
    const QVariantMap settings = readJsonMap(configPath);
    const QStringList keys = {
        QStringLiteral("language"),
        QStringLiteral("locale"),
        QStringLiteral("ui_language"),
        QStringLiteral("lang")};
    for (const QString &key : keys) {
        QString value = settings.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value.replace(QLatin1Char('-'), QLatin1Char('_'));
        }
    }
    return QStringLiteral("en_US");
}

struct I18nContext final {
    QString language;
    QVariantMap strings;
    QVariantMap fallbackStrings;
    QVariantMap messages;
};

I18nContext loadI18nContext(const QString &runtimeRoot)
{
    const QString catalogRoot = QDir(runtimeRoot).filePath(QStringLiteral("System/i18n"));
    const QVariantMap fallbackStrings = readJsonMap(
        QDir(catalogRoot).filePath(QStringLiteral("en_US.json")));
    QString language = configuredLanguage(
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
    const QStringList catalogs = QDir(catalogRoot).entryList(
        {QStringLiteral("*.json")}, QDir::Files);
    QString matchedLanguage;
    for (const QString &catalog : catalogs) {
        const QString candidate = QFileInfo(catalog).completeBaseName();
        if (candidate.compare(language, Qt::CaseInsensitive) == 0) {
            matchedLanguage = candidate;
            break;
        }
    }
    if (matchedLanguage.isEmpty()) {
        matchedLanguage = QStringLiteral("en_US");
    }
    QVariantMap strings = readJsonMap(
        QDir(catalogRoot).filePath(matchedLanguage + QStringLiteral(".json")));
    if (strings.isEmpty() && matchedLanguage != QStringLiteral("en_US")) {
        matchedLanguage = QStringLiteral("en_US");
        strings = fallbackStrings;
    }

    QVariantMap messages = fallbackStrings;
    for (auto it = strings.cbegin(); it != strings.cend(); ++it) {
        messages.insert(it.key(), it.value());
    }
    return {matchedLanguage, strings, fallbackStrings, messages};
}
}

int ExplorerApplication::run(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QString::fromLatin1(kApplicationName));
    QCoreApplication::setOrganizationName(QString::fromLatin1(kOrganizationName));
    QCoreApplication::setOrganizationDomain(QString::fromLatin1(kOrganizationDomain));

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
#ifdef ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT
    if (!environment.contains(QStringLiteral("ASTREA_ROOT"))
        && !environment.contains(QStringLiteral("ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT"))) {
        environment.insert(
            QStringLiteral("ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT"),
            QStringLiteral(ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT));
    }
#endif
    const bool useBootstrap = application.arguments().contains(
        QString::fromLatin1(kBootstrapArgument));
    const bool usePortal = application.arguments().contains(QStringLiteral("--portal"));
    const bool useSelfTest = application.arguments().contains(
        QString::fromLatin1(kSelfTestArgument));

    if (useBootstrap && useSelfTest) {
        QQmlApplicationEngine engine;
        if (!loadBootstrap(engine)) {
            return 1;
        }
        return runSelfTest(engine);
    }

    const auto runtimePaths = Astrea::Explorer::Native::Runtime::ExplorerRuntimeResolver::resolve(
        QCoreApplication::applicationDirPath(),
        QDir::homePath(),
        environment);
    const bool runtimeReady = usePortal
        ? runtimePaths.portalRuntimeReady
        : runtimePaths.normalRuntimeReady;
    if (!runtimeReady && !useBootstrap) {
        for (const QString &diagnostic : runtimePaths.diagnostics) {
            qCritical().noquote() << diagnostic;
            QTextStream(stderr) << diagnostic << Qt::endl;
        }
        if (runtimePaths.resourceRootValid) {
            QTextStream(stderr)
                << (usePortal
                        ? QStringLiteral("Astrea portal runtime is missing required capabilities")
                        : QStringLiteral("Astrea Explorer runtime is missing required capabilities"))
                << Qt::endl;
        }
        return 1;
    }
    if (runtimePaths.valid && !environment.contains(QStringLiteral("ASTREA_ROOT"))) {
        qputenv("ASTREA_ROOT", runtimePaths.root.toUtf8());
    }
    using namespace Astrea::Explorer::Native::Backend;
    using namespace Astrea::Explorer::Native::Services;
    IconThemeService iconThemeService(&application);
    PersistentWorkerTransportOptions transportOptions;
    if (runtimePaths.valid) {
        transportOptions.backendProgram = runtimePaths.backendProgram;
    }
    PersistentWorkerTransport transport(transportOptions, &application);
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
    DesktopApplicationCatalog applicationCatalog({}, &application);
    MimeAppsService mimeApps;
    mimeApps.setCatalog(&applicationCatalog);
    FileOperationService fileOperationService(&backendClient, &application);
    FilesystemService filesystemService(&backendClient, &application);
    FileOperationsController fileOperations(
        &fileOperationService,
        &clipboard,
        &application);
    DeviceController devices(
        &backendClient,
        &application,
        initialSettings.autoMountDeviceIdsJson);
    OpenWithController openWith(&launchService, &application);
    openWith.setCatalog(&applicationCatalog);
    openWith.setMimeAppsService(&mimeApps);
    WallpaperService wallpaper(&application);
    PortalController portal(&application);
    NavigationController navigation(
        &backendClient,
        &directoryModel,
        &directoryWatcher,
        &application);
    RecentSourcePaths recentSources;
    recentSources.finderPath = QDir(QDir::homePath()).filePath(
        QStringLiteral(".local/state/Astrea/finder-recents.json"));
    recentSources.launchHistoryPath = QDir(QDir::homePath()).filePath(
        QStringLiteral(".local/state/Astrea/launch/history.jsonl"));
    recentSources.xbelPath = QDir(QDir::homePath()).filePath(
        QStringLiteral(".local/share/recently-used.xbel"));
    recentSources.limit = 60;
    RecentStore recentStore(recentSources, &application, {}, &applicationCatalog);
    applicationCatalog.discover();
    RecentController recentController(&recentStore, &application);
    navigation.setRecentController(&recentController, recentSources);
    SelectionController selection(&directoryModel, &application);
    AppStateFacade appState(
        &navigation,
        &selection,
        &directoryModel,
        &application,
        &settings,
        &fileOperations,
        &devices,
        runtimePaths,
        &recentController,
        &filesystemService,
        &openWith,
        &launchService,
        &wallpaper,
        &mimeApps,
        &iconThemeService);
    qmlRegisterSingletonInstance<AppStateFacade>(
        kBootstrapModuleUri,
        1,
        0,
        kNativeAppStateTypeName,
        &appState);

    QQmlApplicationEngine engine;
    engine.addImageProvider(
        QStringLiteral("astrea-icons"),
        new Astrea::Explorer::Native::Runtime::AstreaIconImageProvider(&iconThemeService));
    const I18nContext i18n = loadI18nContext(runtimePaths.root);
    application.setProperty("astreaRuntimeRoot", runtimePaths.root);
    application.setProperty(
        "astreaUserName",
        environment.value(QStringLiteral("USER"), QDir::home().dirName()));
    application.setProperty("astreaI18nLanguage", i18n.language);
    application.setProperty("astreaI18nStrings", i18n.strings);
    application.setProperty("astreaI18nFallbackStrings", i18n.fallbackStrings);
    application.setProperty("astreaI18nMessages", i18n.messages);
    engine.rootContext()->setContextProperty(
        QStringLiteral("astreaNativeAppStateAvailable"),
        runtimeReady && !useBootstrap);

    if (usePortal) {
        const QByteArray optionsBytes = environment.value(
            QStringLiteral("ASTREA_FILE_DIALOG_OPTIONS"),
            environment.value(QStringLiteral("BENCH_FILE_DIALOG_OPTIONS"))).toUtf8();
        const QJsonObject options = QJsonDocument::fromJson(optionsBytes).object();
        PortalOptions portalOptions;
        portalOptions.mode = options.value(QStringLiteral("mode")).toString(QStringLiteral("open_file"));
        portalOptions.multiple = options.value(QStringLiteral("multiple")).toBool(false);
        portalOptions.directoryOnly = portalOptions.mode == QStringLiteral("select_folder");
        portalOptions.currentLocation = options.value(QStringLiteral("startFolder")).toString(QDir::homePath());
        portal.begin(portalOptions);
        engine.rootContext()->setContextProperty(
            QStringLiteral("astreaPortalOptionsJson"),
            environment.value(QStringLiteral("ASTREA_FILE_DIALOG_OPTIONS"),
                              environment.value(QStringLiteral("BENCH_FILE_DIALOG_OPTIONS"))));
        engine.rootContext()->setContextProperty(
            QStringLiteral("astreaPortalResultFile"),
            environment.value(QStringLiteral("ASTREA_FILE_DIALOG_RESULT_FILE"),
                              environment.value(QStringLiteral("BENCH_FILE_DIALOG_RESULT_FILE"))));
        engine.rootContext()->setContextProperty(
            QStringLiteral("NativePortalController"),
            &portal);
    }

    const bool loaded = usePortal
        ? loadPortalQml(engine, runtimePaths)
        : useBootstrap
        ? loadBootstrap(engine)
        : loadExplorerQml(engine, runtimePaths);
    if (!loaded) {
        return 1;
    }

    if (!usePortal && !useBootstrap && navigation.currentPath().isEmpty()) {
        const QString requestedStartPath = environment
            .value(QStringLiteral("ASTREA_EXPLORER_START_PATH"))
            .trimmed();
        navigation.navigateTo(
            !requestedStartPath.isEmpty()
                ? requestedStartPath
                : (initialSettings.currentPath.isEmpty()
                       ? QDir::homePath()
                       : initialSettings.currentPath));
    }

    if (useSelfTest) {
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

    m_runtimeWarnings.clear();
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::warnings,
        &engine,
        [this](const QList<QQmlError> &errors) {
            for (const QQmlError &error : errors) {
                m_runtimeWarnings.append(error.toString());
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
    for (const QString &warning : m_runtimeWarnings) {
        QTextStream(stderr) << warning << Qt::endl;
    }

    return !engine.rootObjects().isEmpty() && m_runtimeWarnings.isEmpty();
}

bool ExplorerApplication::loadPortalQml(
    QQmlApplicationEngine &engine,
    const Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths &paths) const
{
    for (const QString &importPath : paths.importPaths) {
        engine.addImportPath(importPath);
    }
    const QUrl portalUrl = QUrl::fromLocalFile(
        QDir(paths.root).filePath(QStringLiteral("Apps/Explorer/PortalDialog.qml")));
    engine.load(portalUrl);
    return !engine.rootObjects().isEmpty();
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
