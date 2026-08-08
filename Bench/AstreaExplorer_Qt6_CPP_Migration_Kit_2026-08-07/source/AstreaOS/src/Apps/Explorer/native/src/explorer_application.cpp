#include "explorer_application.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <QVariant>

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
