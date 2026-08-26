#include "controllers/app_state_facade.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <utility>

#include "controllers/archive_controller.h"
#include "controllers/device_controller.h"
#include "controllers/explorer_settings_controller.h"
#include "controllers/file_operations_controller.h"
#include "controllers/navigation_controller.h"
#include "controllers/open_with_controller.h"
#include "controllers/recent_controller.h"
#include "controllers/selection_controller.h"
#include "controllers/sidebar_favorites_controller.h"
#include "models/directory_model.h"
#include "services/filesystem_service.h"
#include "services/icon_theme_service.h"
#include "services/mime_apps_service.h"
#include "services/wallpaper_service.h"

namespace Astrea::Explorer::Native::Backend {

AppStateFacade::AppStateFacade(AppStateFacadeDependencies dependencies, QObject *parent)
    : QObject(parent)
    , m_navigation(dependencies.navigation)
    , m_selection(dependencies.selection)
    , m_model(dependencies.model)
    , m_settingsController(dependencies.settings)
    , m_sidebarFavorites(dependencies.sidebarFavorites)
    , m_archive(dependencies.archive)
    , m_fileOperations(dependencies.fileOperations)
    , m_devices(dependencies.devices)
    , m_recentController(dependencies.recent)
    , m_filesystemService(dependencies.filesystem)
    , m_openWith(dependencies.openWith)
    , m_launchService(dependencies.launch)
    , m_wallpaperService(dependencies.wallpaper)
    , m_mimeAppsService(dependencies.mimeApps)
    , m_iconThemeService(dependencies.iconTheme)
    , m_runtimePaths(std::move(dependencies.runtimePaths))
{
    Q_ASSERT(m_navigation != nullptr);
    Q_ASSERT(m_selection != nullptr);
    Q_ASSERT(m_model != nullptr);

    if (m_iconThemeService != nullptr) {
        connect(
            m_iconThemeService,
            &Services::IconThemeService::themeChanged,
            this,
            &AppStateFacade::iconThemeChanged);
    }

    if (m_settingsController != nullptr) {
        connect(m_settingsController, &ExplorerSettingsController::showPreviewChanged, this, &AppStateFacade::showPreviewChanged);
        connect(m_settingsController, &ExplorerSettingsController::viewModeChanged, this, &AppStateFacade::viewModeChanged);
        connect(m_settingsController, &ExplorerSettingsController::sortFieldChanged, this, &AppStateFacade::sortFieldChanged);
        connect(m_settingsController, &ExplorerSettingsController::sortAscendingChanged, this, &AppStateFacade::sortAscChanged);
        connect(m_settingsController, &ExplorerSettingsController::showHiddenChanged, this, &AppStateFacade::showHiddenChanged);
        connect(m_settingsController, &ExplorerSettingsController::foldersFirstChanged, this, &AppStateFacade::foldersFirstChanged);
        connect(m_settingsController, &ExplorerSettingsController::groupingEnabledChanged, this, &AppStateFacade::groupingEnabledChanged);
        connect(m_settingsController, &ExplorerSettingsController::zoomLevelChanged, this, &AppStateFacade::zoomLevelChanged);
        connect(m_settingsController, &ExplorerSettingsController::autoMountDeviceIdsJsonChanged, this, &AppStateFacade::autoMountDeviceIdsJsonChanged);
    }
    if (m_settingsController == nullptr) {
        connect(
            m_navigation,
            &NavigationController::listingOptionsChanged,
            this,
            [this]() {
                emit showPreviewChanged();
                emit sortFieldChanged();
                emit sortAscChanged();
                emit showHiddenChanged();
                emit foldersFirstChanged();
            });
    }
    if (m_sidebarFavorites != nullptr) {
        connect(m_sidebarFavorites, &SidebarFavoritesController::favoritesJsonChanged, this, &AppStateFacade::sidebarFavoritesJsonChanged);
        connect(m_sidebarFavorites, &SidebarFavoritesController::hiddenDefaultFavoritesJsonChanged, this, &AppStateFacade::sidebarHiddenDefaultFavoritesJsonChanged);
        connect(m_sidebarFavorites, &SidebarFavoritesController::favoritesChanged, this, &AppStateFacade::sidebarFavoritesChanged);
    } else if (m_settingsController != nullptr) {
        connect(m_settingsController, &ExplorerSettingsController::sidebarFavoritesJsonChanged, this, &AppStateFacade::sidebarFavoritesJsonChanged);
        connect(m_settingsController, &ExplorerSettingsController::sidebarHiddenDefaultFavoritesJsonChanged, this, &AppStateFacade::sidebarHiddenDefaultFavoritesJsonChanged);
        connect(m_settingsController, &ExplorerSettingsController::sidebarFavoritesJsonChanged, this, &AppStateFacade::sidebarFavoritesChanged);
        connect(m_settingsController, &ExplorerSettingsController::sidebarHiddenDefaultFavoritesJsonChanged, this, &AppStateFacade::sidebarFavoritesChanged);
    }
    if (m_archive != nullptr) {
        connect(m_archive, &ArchiveController::stateChanged, this, &AppStateFacade::archiveStateChanged);
        connect(
            m_archive,
            &ArchiveController::operationFinished,
            this,
            &AppStateFacade::filesystemActionFinished);
    }

    connect(
        m_navigation,
        &NavigationController::currentPathChanged,
        this,
        &AppStateFacade::currentPathChanged);
    connect(
        m_navigation,
        &NavigationController::historyChanged,
        this,
        &AppStateFacade::historyChanged);
    connect(
        m_navigation,
        &NavigationController::tabsChanged,
        this,
        &AppStateFacade::tabsChanged);
    connect(
        m_navigation,
        &NavigationController::activeTabIndexChanged,
        this,
        &AppStateFacade::activeTabIndexChanged);
    connect(
        m_navigation,
        &NavigationController::loadingChanged,
        this,
        &AppStateFacade::loadingDirChanged);
    connect(
        m_navigation,
        &NavigationController::loadErrorChanged,
        this,
        &AppStateFacade::loadErrorChanged);
    connect(
        m_navigation,
        &NavigationController::remoteStateChanged,
        this,
        &AppStateFacade::remoteDirectoryActiveChanged);
    connect(
        m_navigation,
        &NavigationController::searchStateChanged,
        this,
        &AppStateFacade::searchStateChanged);
    connect(
        m_selection,
        &SelectionController::selectedFileChanged,
        this,
        &AppStateFacade::selectedFileChanged);
    connect(
        m_selection,
        &SelectionController::selectedFilesChanged,
        this,
        &AppStateFacade::selectedFilesChanged);
    connect(
        m_selection,
        &SelectionController::selectedPathsChanged,
        this,
        &AppStateFacade::selectedPathsChanged);
    connect(
        m_selection,
        &SelectionController::lastSelectedIndexChanged,
        this,
        &AppStateFacade::lastSelectedIndexChanged);
    connect(
        m_model,
        &QAbstractItemModel::modelReset,
        this,
        &AppStateFacade::handleModelChanged);
    connect(
        m_model,
        &QAbstractItemModel::dataChanged,
        this,
        [this]() { handleModelChanged(); });
    connect(
        m_model,
        &QAbstractItemModel::rowsRemoved,
        this,
        [this]() { handleModelChanged(); });

    connect(
        m_selection,
        &SelectionController::selectedPathsChanged,
        this,
        [this]() {
            if (m_fileOperations != nullptr) {
                m_fileOperations->setSelection(selectedPathsInCurrentFolder());
            }
        });

    if (m_fileOperations != nullptr) {
        connect(
            m_fileOperations,
            &FileOperationsController::clipboardChanged,
            this,
            &AppStateFacade::clipboardStateChanged);
        connect(
            m_fileOperations,
            &FileOperationsController::operationStateChanged,
            this,
            &AppStateFacade::fileOperationStateChanged);
        connect(
            m_fileOperations,
            &FileOperationsController::pasteConflictChanged,
            this,
            &AppStateFacade::pasteConflictStateChanged);
        connect(
            m_fileOperations,
            &FileOperationsController::imagePasted,
            this,
            [this](const QString &) { m_navigation->refreshCurrentFolder(); });
        connect(
            m_fileOperations,
            &FileOperationsController::operationFinished,
            this,
            [this](const FileOperationResult &result) {
                if (result.ok) {
                    m_navigation->refreshCurrentFolder();
                }
            });
    }
    if (m_devices != nullptr) {
        connect(
            m_devices,
            &DeviceController::devicesChanged,
            this,
            &AppStateFacade::deviceStateChanged);
        connect(
            m_devices,
            &DeviceController::errorChanged,
            this,
            &AppStateFacade::deviceStateChanged);
        connect(
            m_devices,
            &DeviceController::autoMountChanged,
            this,
            [this]() {
                emit deviceStateChanged();
            });
    }
    if (m_filesystemService != nullptr) {
        connect(
            m_filesystemService,
            &Services::FilesystemService::operationFinished,
            this,
            [this](const UtilityResult &result) {
                if (result.operation == QStringLiteral("warm-thumbnails")
                    && result.requestId != m_thumbnailWarmRequest) {
                    return;
                }
                if (m_archive != nullptr
                    && (result.operation == QStringLiteral("archive-extract")
                        || result.operation == QStringLiteral("archive-compress"))) {
                    return;
                }
                if (result.operation == QStringLiteral("install-appimage")) {
                    m_appImageInstallRunning = false;
                    emit archiveStateChanged();
                }
                const bool destructiveOperation =
                    result.operation == QStringLiteral("trash")
                    || result.operation == QStringLiteral("restore-trash")
                    || result.operation == QStringLiteral("empty-trash")
                    || result.operation == QStringLiteral("delete-permanently");
                if (destructiveOperation) {
                    m_navigation->refreshCurrentFolder();
                    if (result.operation == QStringLiteral("trash")
                        || result.operation == QStringLiteral("restore-trash")
                        || result.operation == QStringLiteral("delete-permanently")) {
                        QStringList completedPaths;
                        const QJsonArray items = result.data.value(QStringLiteral("items")).toArray();
                        for (const QJsonValue &value : items) {
                            const QJsonObject item = value.toObject();
                            const QString status = item.value(QStringLiteral("status")).toString();
                            if (status != QStringLiteral("trashed")
                                && status != QStringLiteral("restored")
                                && status != QStringLiteral("deleted")) {
                                continue;
                            }
                            const QString path = item.value(QStringLiteral("path")).toString();
                            if (!path.isEmpty()) {
                                completedPaths.append(path);
                            }
                        }
                        if (completedPaths.isEmpty() && result.ok) {
                            m_selection->clearSelection();
                        } else {
                            m_selection->removePaths(completedPaths);
                        }
                    }
                }
                emit filesystemActionFinished(
                    result.requestId,
                    result.operation,
                    result.ok,
                    result.data.toVariantMap(),
                    result.ok ? QString() : result.errorMessage);
            });
    }
    if (m_openWith != nullptr) {
        connect(
            m_openWith,
            &OpenWithController::applicationsChanged,
            this,
            [this]() {
                if (!m_openWithPath.isEmpty()) {
                    emit openWithReady(m_openWithPath, m_openWith->applicationList());
                }
            });
        connect(
            m_openWith,
            &OpenWithController::errorChanged,
            this,
            [this]() {
                if (!m_openWithPath.isEmpty() && !m_openWith->error().isEmpty()) {
                    emit openWithReady(m_openWithPath, {});
                }
            });
    }
    if (m_wallpaperService != nullptr) {
        connect(
            m_wallpaperService,
            &Services::WallpaperService::finished,
            this,
            [this](quint64, bool ok, const QString &error) {
                m_wallpaperApplyRunning = false;
                emit wallpaperStateChanged();
                emit filesystemActionFinished(0, QStringLiteral("wallpaper"), ok, {}, error);
            });
    }
}

AppStateFacade::AppStateFacade(
    NavigationController *navigation,
    SelectionController *selection,
    DirectoryModel *model,
    QObject *parent)
    : AppStateFacade(AppStateFacadeDependencies {navigation, selection, model}, parent)
{
}

QAbstractItemModel *AppStateFacade::fileModel() const
{
    return m_model;
}

QAbstractItemModel *AppStateFacade::sidebarFavoritesModel() const
{
    return m_sidebarFavorites == nullptr ? nullptr : m_sidebarFavorites->model();
}

QString AppStateFacade::homePath() const
{
    return QDir::homePath();
}

QString AppStateFacade::runtimeRoot() const
{
    return m_runtimePaths.root.isEmpty()
        ? qEnvironmentVariable("ASTREA_ROOT")
        : m_runtimePaths.root;
}

QString AppStateFacade::backendPath() const
{
    if (!m_runtimePaths.backendProgram.isEmpty()) {
        return m_runtimePaths.backendProgram;
    }
    return runtimeRoot().isEmpty()
        ? QString()
        : QDir(runtimeRoot()).filePath(QStringLiteral("Core/bridge/apps/explorer_backend"));
}

QString AppStateFacade::helperPath() const
{
    return {};
}

QString AppStateFacade::wallpaperManagerPath() const
{
    return {};
}

QString AppStateFacade::astreaLaunch() const
{
    if (!m_runtimePaths.launcherProgram.isEmpty()) {
        return m_runtimePaths.launcherProgram;
    }
    return runtimeRoot().isEmpty()
        ? QString()
        : QDir(runtimeRoot()).filePath(QStringLiteral("bin/astrea-launch"));
}

QString AppStateFacade::windowsRun() const
{
    if (!m_runtimePaths.windowsRunnerProgram.isEmpty()) {
        return m_runtimePaths.windowsRunnerProgram;
    }
    return runtimeRoot().isEmpty()
        ? QString()
        : QDir(runtimeRoot()).filePath(QStringLiteral("System/scripts/astrea-windows-run"));
}

QString AppStateFacade::networkRootPath() const
{
    const QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR");
    return QDir(runtimeDirectory.isEmpty()
                    ? QStringLiteral("/run/user/%1").arg(qEnvironmentVariable("UID"))
                    : runtimeDirectory)
        .filePath(QStringLiteral("gvfs"));
}

QString AppStateFacade::trashFilesPath() const
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty() || !dataHome.startsWith(QLatin1Char('/'))) {
        dataHome = QDir(homePath()).filePath(QStringLiteral(".local/share"));
    }
    return QDir(dataHome).filePath(QStringLiteral("Trash/files"));
}

QString AppStateFacade::trashInfoPath() const
{
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty() || !dataHome.startsWith(QLatin1Char('/'))) {
        dataHome = QDir(homePath()).filePath(QStringLiteral(".local/share"));
    }
    return QDir(dataHome).filePath(QStringLiteral("Trash/info"));
}

QString AppStateFacade::trashVirtualPath() const
{
    return QStringLiteral("trash://");
}

QString AppStateFacade::recentVirtualPath() const
{
    return QStringLiteral("recent://");
}

bool AppStateFacade::isPortalDialog() const
{
    return !qEnvironmentVariable("ASTREA_FILE_DIALOG_OPTIONS").isEmpty()
        || !qEnvironmentVariable("BENCH_FILE_DIALOG_OPTIONS").isEmpty();
}

QString AppStateFacade::currentPath() const
{
    return m_navigation->currentPath();
}

QStringList AppStateFacade::history() const
{
    return m_navigation->history();
}

int AppStateFacade::historyIdx() const
{
    return m_navigation->historyIndex();
}

QVariantList AppStateFacade::tabs() const
{
    return m_navigation->tabs();
}

QVariantList AppStateFacade::breadcrumbParts() const
{
    return m_navigation->breadcrumbParts();
}

int AppStateFacade::activeTabIndex() const
{
    return m_navigation->activeTabIndex();
}

bool AppStateFacade::loadingDir() const
{
    return m_navigation->loading();
}

QString AppStateFacade::loadError() const
{
    return m_navigation->loadError();
}

bool AppStateFacade::remoteDirectoryActive() const
{
    return m_navigation->remoteDirectoryActive();
}

bool AppStateFacade::searchActive() const
{
    return m_navigation->searchActive();
}

bool AppStateFacade::searchVisible() const
{
    return m_navigation->searchVisible();
}

QString AppStateFacade::searchQuery() const
{
    return m_navigation->searchQuery();
}

QString AppStateFacade::selectedFile() const
{
    return m_selection->selectedFile();
}

QStringList AppStateFacade::selectedFiles() const
{
    return m_selection->selectedFiles();
}

QStringList AppStateFacade::selectedPaths() const
{
    return m_selection->selectedPaths();
}

int AppStateFacade::lastSelectedIndex() const
{
    return m_selection->lastSelectedIndex();
}

int AppStateFacade::fileModelRevision() const
{
    return m_fileModelRevision;
}

bool AppStateFacade::fileModelFilling() const
{
    return m_navigation->loading();
}

bool AppStateFacade::showPreview() const
{
    return m_settingsController == nullptr ? m_navigation->previews() : m_settingsController->showPreview();
}

bool AppStateFacade::previewsEnabled() const
{
    return showPreview();
}

QString AppStateFacade::viewMode() const
{
    return m_settingsController == nullptr ? QStringLiteral("list") : m_settingsController->viewMode();
}

QString AppStateFacade::sortField() const
{
    return m_navigation->sortField();
}

bool AppStateFacade::sortAsc() const
{
    return m_navigation->sortAscending();
}

bool AppStateFacade::showHidden() const
{
    return m_navigation->showHidden();
}

bool AppStateFacade::foldersFirst() const
{
    return m_navigation->foldersFirst();
}

bool AppStateFacade::groupingEnabled() const
{
    return m_settingsController == nullptr ? true : m_settingsController->groupingEnabled();
}

double AppStateFacade::zoomLevel() const
{
    return m_settingsController == nullptr ? 1.0 : m_settingsController->zoomLevel();
}

QString AppStateFacade::autoMountDeviceIdsJson() const
{
    if (m_devices != nullptr) {
        return m_devices->autoMountDeviceIdsJson();
    }
    return m_settingsController == nullptr ? QStringLiteral("[]") : m_settingsController->autoMountDeviceIdsJson();
}

QString AppStateFacade::sidebarFavoritesJson() const
{
    return m_settingsController == nullptr ? QStringLiteral("[]") : m_settingsController->sidebarFavoritesJson();
}

QString AppStateFacade::sidebarHiddenDefaultFavoritesJson() const
{
    return m_settingsController == nullptr
        ? QStringLiteral("[]")
        : m_settingsController->sidebarHiddenDefaultFavoritesJson();
}

QVariantList AppStateFacade::sidebarFavorites() const
{
    return m_sidebarFavorites == nullptr ? QVariantList() : m_sidebarFavorites->favorites();
}

QVariantList AppStateFacade::sidebarHiddenDefaultFavorites() const
{
    return m_sidebarFavorites == nullptr
        ? QVariantList()
        : m_sidebarFavorites->hiddenDefaultFavorites();
}

int AppStateFacade::sidebarFavoritesRevision() const
{
    return m_sidebarFavorites == nullptr ? 0 : m_sidebarFavorites->revision();
}

QStringList AppStateFacade::defaultSidebarFavoritePaths() const
{
    return m_sidebarFavorites == nullptr
        ? QStringList()
        : m_sidebarFavorites->defaultFavoritePaths();
}

bool AppStateFacade::inTrashView() const
{
    return isTrashPath(currentPath());
}

bool AppStateFacade::dialogActive() const
{
    return m_dialogActive;
}

QString AppStateFacade::dialogMode() const
{
    return m_dialogMode;
}

QStringList AppStateFacade::dialogFilePatterns() const
{
    return m_dialogFilePatterns;
}

QStringList AppStateFacade::clipboardFiles() const
{
    return m_fileOperations == nullptr ? QStringList() : m_fileOperations->clipboardFiles();
}

QString AppStateFacade::clipboardMode() const
{
    return m_fileOperations == nullptr
        ? QStringLiteral("copy")
        : m_fileOperations->clipboardMode();
}

bool AppStateFacade::fileOperationRunning() const
{
    return m_fileOperations != nullptr && m_fileOperations->running();
}

double AppStateFacade::fileOperationProgress() const
{
    return m_fileOperations == nullptr ? 0.0 : m_fileOperations->operationProgress();
}

int AppStateFacade::fileOperationPercent() const
{
    return m_fileOperations == nullptr ? 0 : m_fileOperations->operationPercent();
}

QString AppStateFacade::fileOperationFileName() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationFileName();
}

QString AppStateFacade::fileOperationStatus() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationStatus();
}

QString AppStateFacade::fileOperationError() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationError();
}

QString AppStateFacade::fileOperationDestination() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationDestination();
}

int AppStateFacade::fileOperationDoneCount() const
{
    return m_fileOperations == nullptr ? 0 : m_fileOperations->operationDoneCount();
}

int AppStateFacade::fileOperationTotalCount() const
{
    return m_fileOperations == nullptr ? 0 : m_fileOperations->operationTotalCount();
}

QString AppStateFacade::fileOperationMode() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationMode();
}

QString AppStateFacade::fileOperationState() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->operationState();
}

QVariantList AppStateFacade::fileOperationItems() const
{
    return m_fileOperations == nullptr ? QVariantList() : m_fileOperations->operationItems();
}

bool AppStateFacade::pasteConflictVisible() const
{
    return m_fileOperations != nullptr && m_fileOperations->pasteConflictVisible();
}

QVariantList AppStateFacade::pasteConflictItems() const
{
    return m_fileOperations == nullptr ? QVariantList() : m_fileOperations->pasteConflictItems();
}

QString AppStateFacade::pendingPasteRename() const
{
    return m_fileOperations == nullptr ? QString() : m_fileOperations->pendingPasteRename();
}

QVariantList AppStateFacade::deviceModel() const
{
    QVariantList result;
    if (m_devices == nullptr) {
        return result;
    }
    for (const DeviceEntry &device : m_devices->devices()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), device.id);
        item.insert(QStringLiteral("devicePath"), device.devicePath);
        item.insert(QStringLiteral("title"), device.title);
        item.insert(QStringLiteral("subtitle"), device.subtitle);
        item.insert(QStringLiteral("mountPath"), device.mountPath);
        item.insert(QStringLiteral("desiredMountPath"), device.desiredMountPath);
        item.insert(QStringLiteral("mounted"), device.mounted);
        item.insert(QStringLiteral("canMount"), device.canMount);
        item.insert(QStringLiteral("canUnmount"), device.canUnmount);
        item.insert(QStringLiteral("canRemount"), device.canRemount);
        item.insert(QStringLiteral("removable"), device.removable);
        item.insert(QStringLiteral("icon"), device.icon);
        item.insert(QStringLiteral("autoMount"), m_devices->isAutoMount(device.id));
        result.append(item);
    }
    return result;
}

QString AppStateFacade::deviceError() const
{
    return m_devices == nullptr ? QString() : m_devices->error();
}

QString AppStateFacade::deviceOperationPath() const
{
    return m_devices == nullptr ? QString() : m_devices->operationPath();
}

QString AppStateFacade::deviceOperationType() const
{
    return m_devices == nullptr ? QString() : m_devices->operationType();
}

QString AppStateFacade::deviceOperationTargetMountPath() const
{
    return m_devices == nullptr ? QString() : m_devices->operationTargetMountPath();
}

bool AppStateFacade::deviceOperationOpenAfterMount() const
{
    return m_devices != nullptr && m_devices->operationOpenAfterMount();
}

QString AppStateFacade::lastUnmountedMountPath() const
{
    return m_devices == nullptr ? QString() : m_devices->lastUnmountedMountPath();
}

bool AppStateFacade::archiveExtractionRunning() const
{
    return m_archive != nullptr && m_archive->running();
}
double AppStateFacade::archiveExtractionProgress() const
{
    return m_archive == nullptr ? 0.0 : m_archive->progress();
}
int AppStateFacade::archiveExtractionPercent() const
{
    return m_archive == nullptr ? 0 : m_archive->percent();
}
QString AppStateFacade::archiveExtractionFileName() const
{
    return m_archive == nullptr ? QString() : m_archive->fileName();
}
QString AppStateFacade::archiveExtractionStatus() const
{
    return m_archive == nullptr ? QString() : m_archive->status();
}
QString AppStateFacade::archiveExtractionError() const
{
    return m_archive == nullptr ? QString() : m_archive->error();
}
QString AppStateFacade::archiveExtractionDestination() const
{
    return m_archive == nullptr ? QString() : m_archive->destination();
}
int AppStateFacade::archiveExtractionDoneCount() const
{
    return m_archive == nullptr ? 0 : m_archive->doneCount();
}
int AppStateFacade::archiveExtractionTotalCount() const
{
    return m_archive == nullptr ? 0 : m_archive->totalCount();
}
QString AppStateFacade::archiveExtractionRemainingText() const
{
    return m_archive == nullptr ? QString() : m_archive->remainingText();
}
bool AppStateFacade::archivePasswordPromptVisible() const
{
    return m_archive != nullptr && m_archive->passwordPromptVisible();
}
QString AppStateFacade::archivePasswordError() const
{
    return m_archive == nullptr ? QString() : m_archive->passwordError();
}
bool AppStateFacade::archiveConflictVisible() const
{
    return m_archive != nullptr && m_archive->conflictVisible();
}
QString AppStateFacade::archiveConflictDestination() const
{
    return m_archive == nullptr ? QString() : m_archive->conflictDestination();
}
QString AppStateFacade::archiveConflictName() const
{
    return m_archive == nullptr ? QString() : m_archive->conflictName();
}
bool AppStateFacade::appImageInstallRunning() const { return m_appImageInstallRunning; }
bool AppStateFacade::wallpaperApplyRunning() const { return m_wallpaperApplyRunning; }

void AppStateFacade::setShowPreview(bool showPreviewValue)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setShowPreview(showPreviewValue);
        return;
    }
    if (m_navigation->previews() == showPreviewValue) {
        return;
    }
    m_navigation->setPreviews(showPreviewValue);
}

void AppStateFacade::setPreviewsEnabled(bool enabled)
{
    setShowPreview(enabled);
}

void AppStateFacade::setViewMode(const QString &viewModeValue)
{
    if (m_settingsController == nullptr) {
        return;
    }
    m_settingsController->setViewMode(viewModeValue);
}

void AppStateFacade::setSortField(const QString &sortFieldValue)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setSortField(sortFieldValue);
        return;
    }
    m_navigation->setSortField(sortFieldValue);
}

void AppStateFacade::setSortAsc(bool sortAscendingValue)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setSortAscending(sortAscendingValue);
        return;
    }
    m_navigation->setSortAscending(sortAscendingValue);
}

void AppStateFacade::setShowHidden(bool showHiddenValue)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setShowHidden(showHiddenValue);
        return;
    }
    m_navigation->setShowHidden(showHiddenValue);
}

void AppStateFacade::setFoldersFirst(bool foldersFirstValue)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setFoldersFirst(foldersFirstValue);
        return;
    }
    m_navigation->setFoldersFirst(foldersFirstValue);
}

void AppStateFacade::setGroupingEnabled(bool groupingEnabledValue)
{
    if (m_settingsController == nullptr) {
        return;
    }
    m_settingsController->setGroupingEnabled(groupingEnabledValue);
}

void AppStateFacade::setZoomLevel(double zoomLevelValue)
{
    if (m_settingsController == nullptr) {
        return;
    }
    m_settingsController->setZoomLevel(zoomLevelValue);
}

void AppStateFacade::increaseZoom()
{
    setZoomLevel(zoomLevel() + 0.1);
}

void AppStateFacade::decreaseZoom()
{
    setZoomLevel(zoomLevel() - 0.1);
}

void AppStateFacade::resetZoom()
{
    setZoomLevel(1.0);
}

void AppStateFacade::setZoom(double level)
{
    setZoomLevel(level);
}

void AppStateFacade::setAutoMountDeviceIdsJson(const QString &json)
{
    if (m_settingsController != nullptr) {
        m_settingsController->setAutoMountDeviceIdsJson(json);
    } else if (m_devices != nullptr) {
        m_devices->setAutoMountDeviceIdsJson(json);
    }
}

void AppStateFacade::setSidebarFavoritesJson(const QString &json)
{
    if (m_settingsController == nullptr) {
        return;
    }
    m_settingsController->setSidebarFavoritesJson(json);
}

void AppStateFacade::setSidebarHiddenDefaultFavoritesJson(const QString &json)
{
    if (m_settingsController == nullptr) {
        return;
    }
    m_settingsController->setSidebarHiddenDefaultFavoritesJson(json);
}

void AppStateFacade::setDialogActive(bool active)
{
    if (m_dialogActive == active) {
        return;
    }
    m_dialogActive = active;
    emit dialogStateChanged();
}

void AppStateFacade::setDialogMode(const QString &mode)
{
    if (m_dialogMode == mode) {
        return;
    }
    m_dialogMode = mode;
    emit dialogStateChanged();
}

void AppStateFacade::setDialogFilePatterns(const QStringList &patterns)
{
    if (m_dialogFilePatterns == patterns) {
        return;
    }
    m_dialogFilePatterns = patterns;
    emit dialogStateChanged();
}

void AppStateFacade::setSearchQuery(const QString &query)
{
    m_navigation->setSearchQuery(query);
}

void AppStateFacade::setSelectedFile(const QString &name)
{
    if (name == m_selection->selectedFile()) {
        return;
    }
    if (name.isEmpty()) {
        m_selection->clearSelection();
    } else {
        m_selection->selectByName(name);
    }
}

BackendRequestId AppStateFacade::navigateTo(const QString &path)
{
    if (path == trashVirtualPath() || path == QStringLiteral("trash:///")) {
        return m_navigation->navigateTo(trashVirtualPath());
    }
    return m_navigation->navigateTo(path);
}

BackendRequestId AppStateFacade::submitSearch(
    const QString &root,
    const QString &query)
{
    return m_navigation->submitSearch(root, query);
}

void AppStateFacade::startSearch()
{
    m_navigation->startSearch();
}

void AppStateFacade::hideSearch()
{
    m_navigation->hideSearch();
}

void AppStateFacade::clearSearch()
{
    m_navigation->clearSearch();
}

void AppStateFacade::goBack()
{
    m_navigation->goBack();
}

void AppStateFacade::goForward()
{
    m_navigation->goForward();
}

void AppStateFacade::createTab(const QString &path)
{
    m_navigation->createTab(path);
}

void AppStateFacade::closeTab(int index)
{
    m_navigation->closeTab(index);
}

void AppStateFacade::switchTab(int index)
{
    m_navigation->switchTab(index);
}

void AppStateFacade::closeTabById(int tabId)
{
    m_navigation->closeTabById(tabId);
}

void AppStateFacade::switchTabById(int tabId)
{
    m_navigation->switchTabById(tabId);
}

int AppStateFacade::tabIndexById(int tabId) const
{
    return m_navigation->tabIndexById(tabId);
}

void AppStateFacade::moveTab(int fromIndex, int toIndex)
{
    m_navigation->moveTab(fromIndex, toIndex);
}

BackendRequestId AppStateFacade::refreshCurrentFolder()
{
    return m_navigation->refreshCurrentFolder();
}

void AppStateFacade::loadRecent()
{
    if (m_recentController != nullptr) {
        m_recentController->loadAsync();
    }
}

void AppStateFacade::recordRecentAccess(
    const QString &path,
    bool isDirectory,
    const QString &fileUrl)
{
    if (m_recentController == nullptr || path.isEmpty() || isRecentPath(path)
        || isTrashPath(path) || dialogActive()) {
        return;
    }

    DirectoryEntry entry;
    if (!m_model->entryForPath(path, &entry)) {
        const QFileInfo fileInfo(path);
        entry.fileName = fileInfo.fileName().isEmpty() ? path : fileInfo.fileName();
        entry.filePath = path;
        entry.fileUrl = fileUrl.isEmpty() ? QUrl::fromLocalFile(path) : QUrl(fileUrl);
        entry.fileIsDir = isDirectory;
        entry.fileExecutable = !isDirectory && fileInfo.isExecutable();
        entry.fileHidden = entry.fileName.startsWith(QLatin1Char('.'));
        entry.fileSize = fileInfo.exists() && !isDirectory ? fileInfo.size() : -1;
        entry.fileModified = fileInfo.exists() ? fileInfo.lastModified() : QDateTime();
        entry.fileKind = isDirectory
            ? QStringLiteral("Folder")
            : fileInfo.suffix().toUpper();
    }
    if (!fileUrl.isEmpty() && entry.fileUrl.isEmpty()) {
        entry.fileUrl = QUrl(fileUrl);
    }
    if (entry.fileUrl.isEmpty()) {
        entry.fileUrl = QUrl::fromLocalFile(path);
    }
    entry.fileIsDir = isDirectory || entry.fileIsDir;
    m_recentController->recordAccess(entry);
}

BackendRequestId AppStateFacade::createFolder(
    const QString &basePath,
    const QString &name)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->createFolder(basePath, name);
}

BackendRequestId AppStateFacade::renamePath(
    const QString &sourcePath,
    const QString &newName)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->renamePath(sourcePath, newName);
}

BackendRequestId AppStateFacade::requestDirectorySuggestions(
    const QString &basePath,
    const QString &prefix)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->suggestDirectories(basePath, prefix);
}

BackendRequestId AppStateFacade::checkExecutable(const QString &program)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->checkExecutable(program);
}

BackendRequestId AppStateFacade::requestProperties(const QString &path)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->properties(path);
}

BackendRequestId AppStateFacade::createDesktopShortcut(const QString &path)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->createDesktopShortcut(path);
}

BackendRequestId AppStateFacade::requestNetworkMountProbe(const QString &rootPath)
{
    return m_filesystemService == nullptr
        ? 0
        : m_filesystemService->networkMountProbe(rootPath);
}

void AppStateFacade::deleteSelected()
{
    if (m_filesystemService == nullptr) {
        return;
    }
    const QStringList paths = selectedPathsInCurrentFolder();
    if (paths.isEmpty()) {
        return;
    }
    if (inTrashView()) {
        QStringList metadataPaths;
        bool hasMetadata = false;
        for (const QString &path : paths) {
            DirectoryEntry entry;
            const QString metadataPath = m_model->entryForPath(path, &entry)
                ? entry.trashInfoPath
                : QString();
            metadataPaths.append(metadataPath);
            hasMetadata = hasMetadata || !metadataPath.isEmpty();
        }
        m_filesystemService->deletePermanently(
            paths,
            hasMetadata ? metadataPaths : QStringList());
    } else {
        m_filesystemService->trash(trashFilesPath(), trashInfoPath(), paths);
    }
}

void AppStateFacade::restoreSelected()
{
    if (m_filesystemService == nullptr || !inTrashView()) {
        return;
    }
    const QStringList paths = selectedPathsInCurrentFolder();
    if (paths.isEmpty()) {
        return;
    }
    for (const QString &path : paths) {
        DirectoryEntry entry;
        const QString metadataPath = m_model->entryForPath(path, &entry)
            && !entry.trashInfoPath.isEmpty()
            ? entry.trashInfoPath
            : trashInfoPath();
        m_filesystemService->restoreTrash(metadataPath, homePath(), {path});
    }
}

void AppStateFacade::emptyTrash()
{
    if (m_filesystemService != nullptr) {
        m_filesystemService->emptyTrash(trashFilesPath(), trashInfoPath());
    }
}

bool AppStateFacade::archiveWorkflowOccupied() const
{
    return m_archive != nullptr && m_archive->workflowOccupied();
}

void AppStateFacade::startArchiveExtraction(const QString &path, const QString &folderName)
{
    if (archiveWorkflowOccupied() || m_archive == nullptr) {
        return;
    }
    m_archive->startArchiveExtraction(path, folderName);
}

void AppStateFacade::submitArchivePassword(const QString &password)
{
    if (m_archive != nullptr) {
        m_archive->submitArchivePassword(password);
    }
}

void AppStateFacade::cancelArchivePassword()
{
    if (m_archive != nullptr) {
        m_archive->cancelArchivePassword();
    }
}

void AppStateFacade::submitArchiveConflict(const QString &policy)
{
    if (m_archive != nullptr) {
        m_archive->submitArchiveConflict(policy);
    }
}

void AppStateFacade::cancelArchiveConflict()
{
    if (m_archive != nullptr) {
        m_archive->cancelArchiveConflict();
    }
}
void AppStateFacade::startFolderCompression(const QString &path, const QString &format)
{
    if (archiveWorkflowOccupied() || m_archive == nullptr) {
        return;
    }
    m_archive->startFolderCompression(path, format);
}
void AppStateFacade::installAppImage(const QString &path)
{
    if (m_filesystemService != nullptr) {
        m_appImageInstallRunning = true;
        emit archiveStateChanged();
        m_filesystemService->installAppImage(path);
    }
}
void AppStateFacade::setAsWallpaper(const QString &path)
{
    if (m_wallpaperService != nullptr) {
        m_wallpaperApplyRunning = true;
        emit wallpaperStateChanged();
        m_wallpaperService->apply(path);
    }
}

QVariantList AppStateFacade::openWithApplications(const QString &path)
{
    if (m_openWith == nullptr) {
        return {};
    }
    m_openWithPath = path;
    m_openWith->discover(path);
    return {};
}

bool AppStateFacade::launchOpenWith(const QString &path, const QString &desktopFile)
{
    if (m_openWith == nullptr) {
        return false;
    }
    if (m_launchService == nullptr) {
        return false;
    }
    const OpenWithApplication application = m_openWith->applicationForId(desktopFile);
    if (application.desktopFile.isEmpty()) {
        return false;
    }
    const Services::LaunchResult result = m_launchService->launch(
        m_launchService->desktopLaunch(application.desktopFile, path));
    return result.started;
}

bool AppStateFacade::setDefaultOpenWith(const QString &path, const QString &desktopFile)
{
    if (path.isEmpty() || desktopFile.isEmpty() || m_mimeAppsService == nullptr) {
        return false;
    }
    const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
    if (mime.isEmpty()) {
        return false;
    }
    const OpenWithApplication application = m_openWith == nullptr
        ? OpenWithController::resolveDesktopEntry(desktopFile)
        : m_openWith->applicationForId(desktopFile);
    if (application.id.isEmpty()) {
        return false;
    }
    return m_mimeAppsService->setDefault(mime, application.id);
}

void AppStateFacade::openItem(
    const QString &path,
    bool isDirectory,
    const QString &fileUrl)
{
    Q_UNUSED(fileUrl);
    if (isDirectory) {
        navigateTo(path);
        return;
    }
    if (m_launchService == nullptr) {
        return;
    }
    Services::LaunchSpec spec;
    if (path.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)) {
        spec = m_launchService->desktopLaunch(path);
    } else if (path.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
               || path.endsWith(QStringLiteral(".msi"), Qt::CaseInsensitive)) {
        spec = m_launchService->windowsLaunch(path);
    } else {
        spec = m_launchService->fileLaunch(path);
    }
    m_launchService->launch(spec);
}

void AppStateFacade::openFile(const QString &path)
{
    openItem(path, false, QString());
}

void AppStateFacade::refreshPreviewMetadata()
{
    refreshCurrentFolder();
}

void AppStateFacade::requestThumbnailWarm(const QString &path, int offset, int limit)
{
    if (m_filesystemService == nullptr || path.isEmpty() || remoteDirectoryActive()) {
        return;
    }
    m_thumbnailWarmRequest = m_filesystemService->warmThumbnails(path, offset, limit);
}

QString AppStateFacade::themedIconSource(
    const QString &iconName,
    int size,
    const QString &themeName)
{
    Q_UNUSED(themeName);
    if (m_iconThemeService == nullptr) {
        return {};
    }
    return m_iconThemeService->iconSourceForNames({iconName}, size);
}

QString AppStateFacade::sidebarIconSource(const QString &iconName, int size)
{
    if (m_iconThemeService == nullptr) {
        return {};
    }
    return m_iconThemeService->symbolicIconSourceForNames({iconName}, size);
}

QString AppStateFacade::effectiveIconTheme() const
{
    return m_iconThemeService != nullptr ? m_iconThemeService->effectiveTheme() : QString();
}

quint64 AppStateFacade::iconThemeRevision() const
{
    return m_iconThemeService != nullptr ? m_iconThemeService->revision() : 0;
}

QString AppStateFacade::fileIconName(
    const QString &path,
    bool isDirectory,
    bool isExecutable) const
{
    if (m_iconThemeService == nullptr) {
        return {};
    }
    return m_iconThemeService->iconCandidatesForFile(path, isDirectory, isExecutable).value(0);
}

QString AppStateFacade::fileIconSource(
    const QString &path,
    bool isDirectory,
    bool isExecutable,
    int size,
    const QString &semanticIconName) const
{
    if (m_iconThemeService == nullptr) {
        return {};
    }
    return m_iconThemeService->fileIconSource(
        path,
        isDirectory,
        isExecutable,
        size,
        semanticIconName);
}

bool AppStateFacade::writePortalResult(const QString &json)
{
    const QString path = qEnvironmentVariable("ASTREA_FILE_DIALOG_RESULT_FILE").isEmpty()
        ? qEnvironmentVariable("BENCH_FILE_DIALOG_RESULT_FILE")
        : qEnvironmentVariable("ASTREA_FILE_DIALOG_RESULT_FILE");
    if (path.isEmpty() || json.isEmpty()) {
        return false;
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        return false;
    }
    output.write(json.toUtf8());
    output.write("\n");
    return output.commit();
}

BackendRequestId AppStateFacade::connectToNetwork(const QString &address)
{
    if (m_filesystemService == nullptr || address.isEmpty()) {
        return 0;
    }
    return m_filesystemService->networkMount(address);
}

void AppStateFacade::refreshDevices()
{
    if (m_devices != nullptr) {
        m_devices->refresh();
    }
}

void AppStateFacade::ensureAutoMountDevices()
{
    // Auto-mount policy is applied by the device controller after a refresh.
    refreshDevices();
}

bool AppStateFacade::replaceFileModel(const QVariantList &items)
{
    return m_navigation->replaceFileModel(items);
}

int AppStateFacade::updateFileModelMetadata(const QVariantList &items)
{
    return m_navigation->updateFileModelMetadata(items);
}

int AppStateFacade::removePathsFromFileModel(const QStringList &paths)
{
    return m_navigation->removePathsFromFileModel(paths);
}

QVariant AppStateFacade::selectedItem() const
{
    const QString selectedName = m_selection->selectedFile();
    if (selectedName.isEmpty()) {
        return {};
    }
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex index = m_model->index(row, 0);
        if (m_model->data(index, DirectoryModel::FileNameRole).toString() != selectedName) {
            continue;
        }
        QVariantMap item;
        const QHash<int, QByteArray> roles = m_model->roleNames();
        for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
            item.insert(
                QString::fromUtf8(role.value()),
                m_model->data(index, role.key()));
        }
        return item;
    }
    return {};
}

QString AppStateFacade::fileUrlForPath(const QString &path) const
{
    const QUrl url(path);
    return url.scheme().isEmpty() ? QUrl::fromLocalFile(path).toString() : path;
}

QString AppStateFacade::joinPath(
    const QString &directory,
    const QString &fileName) const
{
    return QDir(directory).filePath(fileName);
}

QStringList AppStateFacade::selectedPathsInCurrentFolder() const
{
    return m_selection->selectedPaths();
}

QString AppStateFacade::selectedUriListInCurrentFolder() const
{
    QStringList urls;
    for (const QString &path : selectedPathsInCurrentFolder()) {
        urls.append(QUrl::fromLocalFile(path).toString());
    }
    return urls.join(QLatin1Char('\n'));
}

bool AppStateFacade::fileMatchesDialogFilter(
    const QString &fileName,
    bool isDirectory) const
{
    if (isDirectory || m_dialogFilePatterns.isEmpty()) {
        return true;
    }
    return std::any_of(
        m_dialogFilePatterns.cbegin(),
        m_dialogFilePatterns.cend(),
        [&fileName](const QString &pattern) {
            const QRegularExpression expression(
                QRegularExpression::wildcardToRegularExpression(pattern),
                QRegularExpression::CaseInsensitiveOption);
            return expression.match(fileName).hasMatch();
        });
}

void AppStateFacade::dropFilePaths(
    const QStringList &paths,
    const QString &destination,
    const QString &mode)
{
    if (m_fileOperations == nullptr || paths.isEmpty() || destination.isEmpty()) {
        return;
    }
    m_fileOperations->transferFiles(paths, destination, mode);
}

void AppStateFacade::dropFiles(
    const QVariantList &urls,
    const QString &destination,
    const QString &mode)
{
    QStringList paths;
    for (const QVariant &value : urls) {
        const QUrl url = value.canConvert<QUrl>()
            ? value.toUrl()
            : QUrl(value.toString());
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        } else if (url.scheme().isEmpty() && !value.toString().isEmpty()) {
            paths.append(value.toString());
        }
    }
    dropFilePaths(paths, destination, mode);
}

void AppStateFacade::copySelected()
{
    if (m_fileOperations == nullptr) {
        return;
    }
    m_fileOperations->setSelection(selectedPathsInCurrentFolder());
    m_fileOperations->copySelection();
}

void AppStateFacade::cutSelected()
{
    if (m_fileOperations == nullptr) {
        return;
    }
    m_fileOperations->setSelection(selectedPathsInCurrentFolder());
    m_fileOperations->cutSelection();
}

BackendRequestId AppStateFacade::pasteFiles()
{
    if (m_fileOperations == nullptr) {
        return 0;
    }
    return m_fileOperations->pasteFiles(m_navigation->currentPath());
}

bool AppStateFacade::isCutPending(const QString &name) const
{
    return m_fileOperations != nullptr && m_fileOperations->isCutPending(name);
}

bool AppStateFacade::isCutPathPending(const QString &path) const
{
    return m_fileOperations != nullptr && m_fileOperations->isCutPathPending(path);
}

void AppStateFacade::resolvePasteConflict(const QString &policy)
{
    if (m_fileOperations != nullptr) {
        m_fileOperations->resolvePasteConflict(policy);
    }
}

void AppStateFacade::renamePasteConflict(const QString &name)
{
    if (m_fileOperations != nullptr) {
        m_fileOperations->renamePasteConflict(name);
    }
}

void AppStateFacade::cancelPasteConflict()
{
    if (m_fileOperations != nullptr) {
        m_fileOperations->cancelPasteConflict();
    }
}

void AppStateFacade::setPendingPasteRename(const QString &name)
{
    if (m_fileOperations != nullptr) {
        m_fileOperations->setPendingPasteRename(name);
    }
}

BackendRequestId AppStateFacade::requestMountDevice(
    const QString &devicePath,
    bool fromAutoMount,
    bool openAfterMount)
{
    return m_devices == nullptr
        ? 0
        : m_devices->requestMount(devicePath, fromAutoMount, openAfterMount);
}

BackendRequestId AppStateFacade::requestUnmountDevice(
    const QString &devicePath,
    const QString &mountPath)
{
    return m_devices == nullptr ? 0 : m_devices->requestUnmount(devicePath, mountPath);
}

BackendRequestId AppStateFacade::requestRemountDevice(
    const QString &devicePath,
    const QString &mountPath,
    bool openAfterMount)
{
    return m_devices == nullptr
        ? 0
        : m_devices->requestRemount(devicePath, mountPath, openAfterMount);
}

void AppStateFacade::toggleDeviceAutoMount(const QString &deviceId, bool enabled)
{
    if (m_devices != nullptr) {
        m_devices->setAutoMount(deviceId, enabled);
    }
}

bool AppStateFacade::isRecentPath(const QString &path) const
{
    return path == recentVirtualPath();
}

bool AppStateFacade::isTrashPath(const QString &path) const
{
    if (path == trashVirtualPath() || path == QStringLiteral("trash:///")) {
        return true;
    }
    QString normalized = path;
    while (normalized.size() > 1 && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized == trashFilesPath();
}

bool AppStateFacade::canPinSidebarFavorite(const QString &path) const
{
    return m_sidebarFavorites != nullptr && m_sidebarFavorites->canPin(path);
}

bool AppStateFacade::isSidebarFavorite(const QString &path) const
{
    return m_sidebarFavorites != nullptr && m_sidebarFavorites->isFavorite(path);
}

QVariantList AppStateFacade::visibleDefaultSidebarFavorites(
    const QVariantList &items) const
{
    return m_sidebarFavorites == nullptr
        ? QVariantList()
        : m_sidebarFavorites->visibleDefaults(items);
}

void AppStateFacade::pinSidebarFavorite(
    const QString &path,
    const QString &label,
    const QString &icon)
{
    if (m_sidebarFavorites != nullptr) {
        m_sidebarFavorites->pin(path, label, icon);
    }
}

void AppStateFacade::removeSidebarFavorite(const QString &path)
{
    if (m_sidebarFavorites != nullptr) {
        m_sidebarFavorites->remove(path);
    }
}

void AppStateFacade::moveSidebarFavorite(const QString &path, int targetIndex)
{
    if (m_sidebarFavorites != nullptr) {
        m_sidebarFavorites->move(path, targetIndex);
    }
}

bool AppStateFacade::beginSidebarFavoriteDrag(const QString &path)
{
    return m_sidebarFavorites != nullptr && m_sidebarFavorites->beginDrag(path);
}

bool AppStateFacade::previewSidebarFavoriteMove(const QString &path, int finalIndex)
{
    return m_sidebarFavorites != nullptr
        && m_sidebarFavorites->previewMove(path, finalIndex);
}

bool AppStateFacade::commitSidebarFavoriteDrag()
{
    return m_sidebarFavorites != nullptr && m_sidebarFavorites->commitDrag();
}

void AppStateFacade::cancelSidebarFavoriteDrag()
{
    if (m_sidebarFavorites != nullptr) {
        m_sidebarFavorites->cancelDrag();
    }
}

void AppStateFacade::announceContextMenuOpening(const QString &owner)
{
    emit contextMenuOpening(owner);
}

bool AppStateFacade::isSelected(const QString &name) const
{
    return m_selection->isSelected(name);
}

bool AppStateFacade::isPathSelected(const QString &path) const
{
    return m_selection->isPathSelected(path);
}

void AppStateFacade::clearSelection()
{
    m_selection->clearSelection();
}

void AppStateFacade::selectAll()
{
    m_selection->selectAll();
}

void AppStateFacade::selectByName(const QString &name)
{
    m_selection->selectByName(name);
}

void AppStateFacade::selectByPath(const QString &path)
{
    m_selection->selectByPath(path);
}

void AppStateFacade::prepareSelectionForDrag(const QString &name, int index)
{
    m_selection->prepareSelectionForDrag(name, index);
}

void AppStateFacade::handleSelection(
    const QString &name,
    int index,
    bool ctrlMode,
    bool shiftMode,
    bool preserveCurrentSelection)
{
    m_selection->handleSelection(
        name,
        index,
        ctrlMode,
        shiftMode,
        preserveCurrentSelection);
}

void AppStateFacade::handleModelChanged()
{
    ++m_fileModelRevision;
    emit fileModelRevisionChanged();
}

} // namespace Astrea::Explorer::Native::Backend
