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
    QStringLiteral("Apps/Explorer/PortalDialog.qml"),
    QStringLiteral("Astrea/Components/qmldir"),
    QStringLiteral("Astrea/Components/theme/Theme.qml"),
    QStringLiteral("Astrea/Components/theme/Borealis/qmldir"),
    QStringLiteral("Astrea/Components/theme/Borealis/Theme.qml"),
    QStringLiteral("Astrea/Components/theme/Borealis/State.qml"),
    QStringLiteral("Astrea/Components/theme/Borealis/Tokens.qml"),
    QStringLiteral("Astrea/Components/theme/Borealis/Shell.qml"),
    QStringLiteral("Astrea/Components/AppIcon.qml"),
    QStringLiteral("Astrea/Files/qmldir"),
    QStringLiteral("Astrea/Files/DragDropSupport.js"),
    QStringLiteral("Astrea/Files/ui/OperationProgressCard.qml"),
    QStringLiteral("Astrea/I18n/qmldir"),
    QStringLiteral("Astrea/I18n/I18n.qml"),
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
    QStringLiteral("Astrea"),
    QStringLiteral("Astrea/Components"),
    QStringLiteral("Astrea/Files"),
    QStringLiteral("Astrea/I18n"),
    QStringLiteral("Core"),
    QStringLiteral("Core/components"),
    QStringLiteral("Features"),
    QStringLiteral("Features/Files"),
    QStringLiteral("System"),
    QStringLiteral("System/i18n"),
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

bool isExecutablePresent(const QString &path)
{
    const QFileInfo pathInfo(path);
    return pathInfo.isFile() && pathInfo.isExecutable();
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
        if (isExecutablePresent(path)) {
            result.backendProgram = path;
            result.backendAvailable = true;
            break;
        }
    }

    const QStringList optionalPaths {
        rootDir.filePath(QStringLiteral("bin/astrea-launch")),
        rootDir.filePath(QStringLiteral("System/scripts/astrea-windows-run")),
    };
    if (isExecutablePresent(optionalPaths.at(0))) {
        result.launcherProgram = optionalPaths.at(0);
        result.launchAvailable = true;
    }
    if (isExecutablePresent(optionalPaths.at(1))) {
        result.windowsRunnerProgram = optionalPaths.at(1);
        result.windowsRunnerAvailable = true;
    }

    result.resourceRootValid = true;
    result.valid = true;
    result.normalRuntimeReady = result.backendAvailable && result.launchAvailable;
    result.portalRuntimeReady = result.backendAvailable;
    if (!result.backendAvailable && diagnostics != nullptr) {
        diagnostics->append(
            origin + QStringLiteral(" accepted resource root but missing executable Explorer backend"));
    }
    if (!result.launchAvailable && diagnostics != nullptr) {
        diagnostics->append(
            origin + QStringLiteral(" accepted resource root but missing executable astrea-launch"));
    }
    if (!result.windowsRunnerAvailable && diagnostics != nullptr) {
        diagnostics->append(
            origin + QStringLiteral(" optional Windows runner is unavailable"));
    }
    return result;
}

ExplorerRuntimePaths invalidExplicitRoot(
    const QString &value,
    QStringList diagnostics)
{
    ExplorerRuntimePaths result;
    result.resourceRootValid = false;
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

    const QString installedPrefixRoot = QDir(executableDirectory).filePath(
        QStringLiteral("../share/Astrea"));
    ExplorerRuntimePaths prefixInstalled = fromCandidate(
        installedPrefixRoot,
        QStringLiteral("installed prefix Astrea root"),
        &diagnostics);
    if (prefixInstalled.valid) {
        prefixInstalled.diagnostics = std::move(diagnostics);
        return prefixInstalled;
    }

    const QString developmentRoot = environment.value(
        QStringLiteral("ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT"));
    if (!developmentRoot.trimmed().isEmpty()) {
        ExplorerRuntimePaths development = fromCandidate(
            developmentRoot,
            QStringLiteral("configured development Astrea root"),
            &diagnostics);
        if (development.valid) {
            development.diagnostics = std::move(diagnostics);
            return development;
        }
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

    ExplorerRuntimePaths result;
    result.diagnostics = std::move(diagnostics);
    result.diagnostics.append(QStringLiteral("no valid Astrea runtime root found"));
    return result;
}

} // namespace Astrea::Explorer::Native::Runtime
