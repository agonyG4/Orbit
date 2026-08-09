#include "runtime/explorer_runtime_paths.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

namespace Astrea::Explorer::Native::Runtime {
namespace {

const QStringList kRequiredFiles {
    QStringLiteral("Apps/Explorer/Main.qml"),
    QStringLiteral("Apps/Explorer/qmldir"),
    QStringLiteral("Apps/Explorer/Theme.qml"),
    QStringLiteral("Apps/Explorer/AppState.qml"),
    QStringLiteral("Apps/Explorer/compatibility/NativeAppStateAdapter.qml"),
    QStringLiteral("Apps/Explorer/compatibility/LegacyAppStateAdapter.qml"),
    QStringLiteral("Apps/Explorer/state/NavigationState.qml"),
    QStringLiteral("Apps/Explorer/components/layout/Sidebar.qml"),
    QStringLiteral("Apps/Explorer/components/layout/Toolbar.qml"),
    QStringLiteral("Apps/Explorer/components/layout/PreviewPanel.qml"),
    QStringLiteral("Apps/Explorer/components/layout/StatusBar.qml"),
    QStringLiteral("Apps/Explorer/components/views/FileListView.qml"),
    QStringLiteral("Apps/Explorer/components/views/FileIconView.qml"),
    QStringLiteral("Apps/Explorer/components/common/NavButton.qml"),
    QStringLiteral("Apps/Explorer/components/common/FileContextMenu.qml"),
    QStringLiteral("Apps/Explorer/AstreaFiles/qmldir"),
    QStringLiteral("Apps/Explorer/AstreaFiles/DragDropSupport.js"),
    QStringLiteral("Apps/Explorer/AstreaFiles/ui/OperationProgressCard.qml"),
    QStringLiteral("Apps/Explorer/AstreaI18n/qmldir"),
    QStringLiteral("Apps/Explorer/AstreaI18n/I18n.qml"),
    QStringLiteral("Apps/Explorer/AstreaComponents/qmldir"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Theme.qml"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Borealis/qmldir"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Borealis/Theme.qml"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Borealis/State.qml"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Borealis/Tokens.qml"),
    QStringLiteral("Apps/Explorer/AstreaComponents/theme/Borealis/Shell.qml"),
    QStringLiteral("Apps/Explorer/QuickshellComponents/AppIcon.qml"),
    QStringLiteral("Core/components/Theme.qml"),
    QStringLiteral("Core/components/qmldir"),
    QStringLiteral("Core/components/theme/Theme.qml"),
    QStringLiteral("Core/components/theme/Borealis/qmldir"),
    QStringLiteral("Core/components/theme/Borealis/Theme.qml"),
    QStringLiteral("Features/Files/DragDropSupport.js"),
    QStringLiteral("Features/Files/qmldir"),
    QStringLiteral("System/i18n/I18n.qml"),
    QStringLiteral("System/i18n/qmldir"),
};

const QStringList kRequiredDirectories {
    QStringLiteral("Apps"),
    QStringLiteral("Apps/Explorer"),
    QStringLiteral("Apps/Explorer/components"),
    QStringLiteral("Apps/Explorer/components/common"),
    QStringLiteral("Apps/Explorer/components/layout"),
    QStringLiteral("Apps/Explorer/components/views"),
    QStringLiteral("Apps/Explorer/state"),
    QStringLiteral("Apps/Explorer/AstreaComponents"),
    QStringLiteral("Apps/Explorer/AstreaFiles"),
    QStringLiteral("Apps/Explorer/AstreaI18n"),
    QStringLiteral("Apps/Explorer/QuickshellComponents"),
    QStringLiteral("Core"),
    QStringLiteral("Core/components"),
    QStringLiteral("Features"),
    QStringLiteral("Features/Files"),
    QStringLiteral("System"),
    QStringLiteral("System/i18n"),
    QStringLiteral("Quickshell"),
    QStringLiteral("Quickshell/components"),
};

QString absoluteCleanPath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool isRequiredFilePresent(const QString &root, const QString &relativePath)
{
    const QFileInfo pathInfo(QDir(root).filePath(relativePath));
    return pathInfo.isFile();
}

bool isRequiredDirectoryPresent(const QString &root, const QString &relativePath)
{
    const QFileInfo pathInfo(QDir(root).filePath(relativePath));
    return pathInfo.isDir();
}

ExplorerRuntimePaths fromCandidate(
    const QString &candidate,
    const QString &origin,
    QStringList *diagnostics)
{
    ExplorerRuntimePaths result;
    result.root = absoluteCleanPath(candidate);
    if (result.root.isEmpty()) {
        if (diagnostics != nullptr) {
            diagnostics->append(origin + QStringLiteral(" is empty"));
        }
        return result;
    }

    QStringList missing;
    for (const QString &relativePath : kRequiredFiles) {
        if (!isRequiredFilePresent(result.root, relativePath)) {
            missing.append(relativePath);
        }
    }
    for (const QString &relativePath : kRequiredDirectories) {
        if (!isRequiredDirectoryPresent(result.root, relativePath)) {
            missing.append(relativePath);
        }
    }
    if (!missing.isEmpty()) {
        if (diagnostics != nullptr) {
            diagnostics->append(
                origin + QStringLiteral(" rejected at ") + result.root
                + QStringLiteral("; missing: ") + missing.join(QStringLiteral(", ")));
        }
        result.root.clear();
        return result;
    }

    const QDir rootDir(result.root);
    result.explorerDirectory = rootDir.filePath(QStringLiteral("Apps/Explorer"));
    result.explorerMainQml = rootDir.filePath(QStringLiteral("Apps/Explorer/Main.qml"));
    result.importPaths = {
        result.root,
        rootDir.filePath(QStringLiteral("Apps")),
        rootDir.filePath(QStringLiteral("Core")),
        rootDir.filePath(QStringLiteral("Features")),
        rootDir.filePath(QStringLiteral("System")),
    };

    const QStringList backendCandidates {
        rootDir.filePath(QStringLiteral("Core/bridge/apps/explorer_backend")),
        rootDir.filePath(QStringLiteral("Core/bridge/explorer_backend")),
    };
    for (const QString &path : backendCandidates) {
        if (QFileInfo(path).isFile()) {
            result.backendProgram = path;
            break;
        }
    }

    const QStringList optionalPaths {
        rootDir.filePath(QStringLiteral("Apps/Explorer/explorer_helper.py")),
        rootDir.filePath(QStringLiteral("bin/astrea-launch")),
        rootDir.filePath(QStringLiteral("System/scripts/astrea-windows-run")),
    };
    if (QFileInfo(optionalPaths.at(0)).isFile()) {
        result.helperProgram = optionalPaths.at(0);
    }
    if (QFileInfo(optionalPaths.at(1)).isFile()) {
        result.launcherProgram = optionalPaths.at(1);
    }
    if (QFileInfo(optionalPaths.at(2)).isFile()) {
        result.windowsRunnerProgram = optionalPaths.at(2);
    }

    result.valid = true;
    return result;
}

ExplorerRuntimePaths invalidExplicitRoot(
    const QString &value,
    QStringList diagnostics)
{
    ExplorerRuntimePaths result;
    result.diagnostics = std::move(diagnostics);
    result.diagnostics.append(
        QStringLiteral("ASTREA_ROOT was explicitly supplied and is invalid: ")
        + (value.isEmpty() ? QStringLiteral("<empty>") : value));
    return result;
}

} // namespace

ExplorerRuntimePaths ExplorerRuntimeResolver::resolve(
    const QString &executableDirectory,
    const QString &homeDirectory,
    const QProcessEnvironment &environment)
{
    QStringList diagnostics;

    if (environment.contains(QStringLiteral("ASTREA_ROOT"))) {
        const QString explicitRoot = environment.value(QStringLiteral("ASTREA_ROOT"));
        ExplorerRuntimePaths result = fromCandidate(
            explicitRoot,
            QStringLiteral("ASTREA_ROOT"),
            &diagnostics);
        if (result.valid) {
            result.diagnostics = std::move(diagnostics);
        } else {
            return invalidExplicitRoot(explicitRoot, std::move(diagnostics));
        }
        return result;
    }

    const QString installedRoot = QDir(homeDirectory).filePath(QStringLiteral(".local/share/Astrea"));
    ExplorerRuntimePaths installed = fromCandidate(
        installedRoot,
        QStringLiteral("installed Astrea root"),
        &diagnostics);
    if (installed.valid) {
        installed.diagnostics = std::move(diagnostics);
        return installed;
    }

    QString developmentCandidate = absoluteCleanPath(executableDirectory);
    while (!developmentCandidate.isEmpty()) {
        ExplorerRuntimePaths development = fromCandidate(
            developmentCandidate,
            QStringLiteral("development candidate"),
            &diagnostics);
        if (development.valid) {
            development.diagnostics = std::move(diagnostics);
            return development;
        }

        const QString parent = QDir::cleanPath(QDir(developmentCandidate).filePath(QStringLiteral("..")));
        if (parent == developmentCandidate) {
            break;
        }
        developmentCandidate = parent;
    }

    ExplorerRuntimePaths result;
    result.diagnostics = std::move(diagnostics);
    result.diagnostics.append(QStringLiteral("no valid Astrea runtime root found"));
    return result;
}

} // namespace Astrea::Explorer::Native::Runtime
