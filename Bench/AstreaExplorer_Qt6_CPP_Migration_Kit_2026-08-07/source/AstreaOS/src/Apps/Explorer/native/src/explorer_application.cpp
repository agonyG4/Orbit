#include "explorer_application.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QString>
#include <QVariant>

#include <QtQml/qqml.h>

#include "backend/one_shot_cli_transport.h"
#include "backend/rust_backend_client.h"
#include "controllers/app_state_facade.h"
#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"
#include "services/settings_service.h"

namespace {
constexpr auto kApplicationName = "Explorer";
constexpr auto kOrganizationName = "agony";
constexpr auto kOrganizationDomain = "local";
constexpr auto kBootstrapModuleUri = "Astrea.Explorer.Native";
constexpr auto kBootstrapTypeName = "NativeBootstrap";
constexpr auto kSelfTestArgument = "--self-test";
}

int ExplorerApplication::run(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QString::fromLatin1(kApplicationName));
    QCoreApplication::setOrganizationName(QString::fromLatin1(kOrganizationName));
    QCoreApplication::setOrganizationDomain(QString::fromLatin1(kOrganizationDomain));

    using namespace Astrea::Explorer::Native::Backend;
    using namespace Astrea::Explorer::Native::Services;
    OneShotCliTransport transport({}, &application);
    RustBackendClient backendClient(&transport, &application);
    DirectoryModel directoryModel(&application);
    DirectoryWatchService directoryWatcher(&application);
    SettingsService settings(
        QDir(QDir::homePath()).filePath(QStringLiteral(".config/explorer.conf")));
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
        &settings);
    qmlRegisterSingletonInstance<AppStateFacade>(
        kBootstrapModuleUri,
        1,
        0,
        "AppState",
        &appState);

    QQmlApplicationEngine engine;

    if (!loadBootstrap(engine)) {
        return 1;
    }

    if (application.arguments().contains(QString::fromLatin1(kSelfTestArgument))) {
        return runSelfTest(engine);
    }

    return application.exec();
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
    if (rootObjects.isEmpty() || rootObjects.constFirst() == nullptr) {
        return 1;
    }

    const QVariant ready = rootObjects.constFirst()->property("ready");
    return ready.isValid() && ready.toBool() ? 0 : 1;
}
