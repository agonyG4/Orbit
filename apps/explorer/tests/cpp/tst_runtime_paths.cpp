#include <QFile>
#include <QDir>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/explorer_runtime_paths.h"

using Astrea::Explorer::Native::Runtime::ExplorerRuntimePaths;
using Astrea::Explorer::Native::Runtime::ExplorerRuntimeResolver;

namespace {

void createRuntimeRoot(const QString &root, bool optionalFiles = true)
{
    const QDir runtime(root);
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/components/common")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/components/layout")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/components/views")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/state")));
    QVERIFY(runtime.mkpath(QStringLiteral("Core/components")));
    QVERIFY(runtime.mkpath(QStringLiteral("Features/Files")));
    QVERIFY(runtime.mkpath(QStringLiteral("System/i18n")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/AstreaComponents")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/AstreaFiles")));
    QVERIFY(runtime.mkpath(QStringLiteral("Apps/Explorer/AstreaI18n")));

    const QStringList requiredFiles {
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
        QStringLiteral("Apps/Explorer/PortalDialog.qml"),
        QStringLiteral("Apps/Explorer/AstreaComponents/AppIcon.qml"),
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
    for (const QString &relativePath : requiredFiles) {
        QVERIFY(runtime.mkpath(QFileInfo(relativePath).path()));
        QFile file(runtime.filePath(relativePath));
        QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(relativePath));
        file.write("fixture");
    }

    if (!optionalFiles) {
        return;
    }

    QVERIFY(runtime.mkpath(QStringLiteral("Core/bridge/apps")));
    QVERIFY(runtime.mkpath(QStringLiteral("bin")));
    QVERIFY(runtime.mkpath(QStringLiteral("System/scripts")));
    const QStringList optionalPaths {
        QStringLiteral("Core/bridge/apps/explorer_backend"),
        QStringLiteral("bin/astrea-launch"),
        QStringLiteral("System/scripts/astrea-windows-run"),
    };
    for (const QString &relativePath : optionalPaths) {
        QFile file(runtime.filePath(relativePath));
        QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(relativePath));
        file.write("fixture");
        if (relativePath != QStringLiteral("System/scripts/astrea-windows-run")) {
            QVERIFY(file.setPermissions(
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        }
    }
}

QProcessEnvironment environmentWithoutRoot()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("ASTREA_ROOT"));
    return environment;
}

} // namespace

class RuntimePathsTest final : public QObject
{
    Q_OBJECT

private slots:
    void explicitRootWins();
    void explicitEmptyRootIsRejectedWithoutFallback();
    void invalidExplicitRootIsRejectedWithoutFallback();
    void invalidInstalledRootFallsThroughToDevelopment();
    void installedPrefixRootBeatsUserAndDevelopment();
    void validInstalledRootBeatsDevelopment();
    void developmentRootIsFoundFromExecutableAncestors();
    void optionalRuntimePathsStayUnderResolvedRoot();
    void resourceAndExecutableCapabilitiesAreSeparate();
    void missingBackendDoesNotReportNormalRuntimeReady();
};

void RuntimePathsTest::explicitRootWins()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString explicitRoot = fixture.filePath(QStringLiteral("explicit"));
    const QString installedRoot = fixture.filePath(QStringLiteral("home/.local/share/Astrea"));
    const QString developmentRoot = fixture.filePath(QStringLiteral("development"));
    createRuntimeRoot(explicitRoot);
    createRuntimeRoot(installedRoot);
    createRuntimeRoot(developmentRoot);

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(QStringLiteral("ASTREA_ROOT"), explicitRoot);
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("development/build/native")),
        fixture.filePath(QStringLiteral("home")),
        environment);

    QVERIFY(result.valid);
    QCOMPARE(result.root, QDir::cleanPath(explicitRoot));
}

void RuntimePathsTest::explicitEmptyRootIsRejectedWithoutFallback()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    createRuntimeRoot(fixture.filePath(QStringLiteral("home/.local/share/Astrea")));

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(QStringLiteral("ASTREA_ROOT"), QString());
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("development/build")),
        fixture.filePath(QStringLiteral("home")),
        environment);

    QVERIFY(!result.valid);
    QVERIFY(result.diagnostics.join(QLatin1Char('\n')).contains(QStringLiteral("ASTREA_ROOT")));
}

void RuntimePathsTest::invalidExplicitRootIsRejectedWithoutFallback()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    createRuntimeRoot(fixture.filePath(QStringLiteral("home/.local/share/Astrea")));

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(
        QStringLiteral("ASTREA_ROOT"),
        fixture.filePath(QStringLiteral("explicit-incomplete")));
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("development/build")),
        fixture.filePath(QStringLiteral("home")),
        environment);

    QVERIFY(!result.valid);
    QCOMPARE(result.root, QString());
}

void RuntimePathsTest::invalidInstalledRootFallsThroughToDevelopment()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString developmentRoot = fixture.filePath(QStringLiteral("checkout/runtime"));
    createRuntimeRoot(developmentRoot);
    QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("home/.local/share/Astrea"))));
    QFile incomplete(fixture.filePath(QStringLiteral("home/.local/share/Astrea/README")));
    QVERIFY(incomplete.open(QIODevice::WriteOnly));

    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("checkout/runtime/build/native")),
        fixture.filePath(QStringLiteral("home")),
        environmentWithoutRoot());

    QVERIFY(result.valid);
    QCOMPARE(result.root, QDir::cleanPath(developmentRoot));
}

void RuntimePathsTest::installedPrefixRootBeatsUserAndDevelopment()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString prefix = fixture.filePath(QStringLiteral("prefix"));
    const QString prefixRoot = QDir(prefix).filePath(QStringLiteral("share/Astrea"));
    const QString userRoot = fixture.filePath(QStringLiteral("home/.local/share/Astrea"));
    const QString developmentRoot = fixture.filePath(QStringLiteral("checkout/runtime"));
    QVERIFY(QDir().mkpath(QDir(prefix).filePath(QStringLiteral("bin"))));
    createRuntimeRoot(prefixRoot);
    createRuntimeRoot(userRoot);
    createRuntimeRoot(developmentRoot);

    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        QDir(prefix).filePath(QStringLiteral("bin")),
        fixture.filePath(QStringLiteral("home")),
        environmentWithoutRoot());

    QVERIFY(result.valid);
    QCOMPARE(result.root, QDir::cleanPath(prefixRoot));
}

void RuntimePathsTest::validInstalledRootBeatsDevelopment()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString installedRoot = fixture.filePath(QStringLiteral("home/.local/share/Astrea"));
    const QString developmentRoot = fixture.filePath(QStringLiteral("checkout/runtime"));
    createRuntimeRoot(installedRoot);
    createRuntimeRoot(developmentRoot);

    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("checkout/runtime/build/native")),
        fixture.filePath(QStringLiteral("home")),
        environmentWithoutRoot());

    QVERIFY(result.valid);
    QCOMPARE(result.root, QDir::cleanPath(installedRoot));
}

void RuntimePathsTest::developmentRootIsFoundFromExecutableAncestors()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString developmentRoot = fixture.filePath(QStringLiteral("checkout/runtime"));
    createRuntimeRoot(developmentRoot);

    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("checkout/runtime/src/Apps/Explorer/native/build")),
        fixture.filePath(QStringLiteral("home-without-installed-root")),
        environmentWithoutRoot());

    QVERIFY(result.valid);
    QCOMPARE(result.root, QDir::cleanPath(developmentRoot));
    QCOMPARE(
        result.explorerMainQml,
        QDir(developmentRoot).filePath(QStringLiteral("Apps/Explorer/Main.qml")));
}

void RuntimePathsTest::optionalRuntimePathsStayUnderResolvedRoot()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString root = fixture.filePath(QStringLiteral("runtime"));
    createRuntimeRoot(root);

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(QStringLiteral("ASTREA_ROOT"), root);
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("unused")),
        fixture.filePath(QStringLiteral("unused-home")),
        environment);

    QVERIFY(result.valid);
    QVERIFY(result.backendProgram.startsWith(QDir::cleanPath(root) + QLatin1Char('/')));
    QVERIFY(result.launcherProgram.startsWith(QDir::cleanPath(root) + QLatin1Char('/')));
    QVERIFY(result.windowsRunnerProgram.isEmpty());
    QVERIFY(!result.windowsRunnerAvailable);
    QVERIFY(!result.importPaths.isEmpty());
}

void RuntimePathsTest::resourceAndExecutableCapabilitiesAreSeparate()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString root = fixture.filePath(QStringLiteral("runtime"));
    createRuntimeRoot(root);
    QVERIFY(QFile::remove(QDir(root).filePath(QStringLiteral("System/scripts/astrea-windows-run"))));

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(QStringLiteral("ASTREA_ROOT"), root);
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("unused")),
        fixture.filePath(QStringLiteral("unused-home")),
        environment);

    QVERIFY(result.resourceRootValid);
    QVERIFY(result.backendAvailable);
    QVERIFY(result.launchAvailable);
    QVERIFY(!result.windowsRunnerAvailable);
    QVERIFY(result.normalRuntimeReady);
    QVERIFY(result.portalRuntimeReady);
}

void RuntimePathsTest::missingBackendDoesNotReportNormalRuntimeReady()
{
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString root = fixture.filePath(QStringLiteral("runtime"));
    createRuntimeRoot(root);
    QVERIFY(QFile::remove(QDir(root).filePath(QStringLiteral("Core/bridge/apps/explorer_backend"))));

    QProcessEnvironment environment = environmentWithoutRoot();
    environment.insert(QStringLiteral("ASTREA_ROOT"), root);
    const ExplorerRuntimePaths result = ExplorerRuntimeResolver::resolve(
        fixture.filePath(QStringLiteral("unused")),
        fixture.filePath(QStringLiteral("unused-home")),
        environment);

    QVERIFY(result.resourceRootValid);
    QVERIFY(result.valid);
    QVERIFY(!result.backendAvailable);
    QVERIFY(!result.normalRuntimeReady);
    QVERIFY(!result.portalRuntimeReady);
    QVERIFY(result.diagnostics.join(QLatin1Char('\n')).contains(QStringLiteral("backend")));
}

QTEST_APPLESS_MAIN(RuntimePathsTest)

#include "tst_runtime_paths.moc"
