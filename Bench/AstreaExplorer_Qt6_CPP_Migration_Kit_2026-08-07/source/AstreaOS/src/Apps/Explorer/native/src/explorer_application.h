#pragma once

#include <QStringList>

class QQmlApplicationEngine;

namespace Astrea::Explorer::Native::Runtime {
struct ExplorerRuntimePaths;
}

class ExplorerApplication final
{
public:
    int run(int argc, char **argv);

private:
    bool loadExplorerQml(
        QQmlApplicationEngine &engine,
        const Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths &paths) const;
    bool loadBootstrap(QQmlApplicationEngine &engine) const;
    int runSelfTest(QQmlApplicationEngine &engine) const;

    mutable QStringList m_runtimeWarnings;
};
