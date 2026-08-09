#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace Astrea::Explorer::Native::Runtime {

struct ExplorerRuntimePaths
{
    QString root;
    QString explorerDirectory;
    QString explorerMainQml;
    QString backendProgram;
    QString helperProgram;
    QString launcherProgram;
    QString windowsRunnerProgram;
    QStringList importPaths;
    QStringList diagnostics;
    bool valid = false;
};

class ExplorerRuntimeResolver final
{
public:
    static ExplorerRuntimePaths resolve(
        const QString &executableDirectory,
        const QString &homeDirectory,
        const QProcessEnvironment &environment);
};

} // namespace Astrea::Explorer::Native::Runtime
