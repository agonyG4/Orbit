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

namespace Astrea::Explorer::Native::Backend {

namespace {

QString normalizeFavoritePath(const QString &path)
{
    if (path.isEmpty() || !path.startsWith(QLatin1Char('/'))) {
        return path;
    }
    return QDir::cleanPath(path);
}

QString xdgUserDirectory(const QString &key, const QString &fallback, const QString &home)
{
    const QString configPath = QDir(home).filePath(QStringLiteral(".config/user-dirs.dirs"));
    QFile config(configPath);
    if (!config.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QDir(home).filePath(fallback);
    }
    const QByteArray contents = config.readAll();
    const QRegularExpression expression(
        QStringLiteral("^%1=\\\"([^\\\"]+)\\\"$").arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = expression.match(QString::fromUtf8(contents));
    if (!match.hasMatch()) {
        return QDir(home).filePath(fallback);
    }
    QString value = match.captured(1);
    value.replace(QStringLiteral("$HOME"), home);
    return QDir::cleanPath(value);
}

QVariantMap defaultFavoriteItem(const QString &path, const QString &home)
{
    const QStringList defaults {
        xdgUserDirectory(QStringLiteral("XDG_DESKTOP_DIR"), QStringLiteral("Desktop"), home),
        xdgUserDirectory(QStringLiteral("XDG_DOCUMENTS_DIR"), QStringLiteral("Documents"), home),
        xdgUserDirectory(QStringLiteral("XDG_DOWNLOAD_DIR"), QStringLiteral("Downloads"), home),
        xdgUserDirectory(QStringLiteral("XDG_PICTURES_DIR"), QStringLiteral("Pictures"), home),
        xdgUserDirectory(QStringLiteral("XDG_MUSIC_DIR"), QStringLiteral("Music"), home),
        xdgUserDirectory(QStringLiteral("XDG_VIDEOS_DIR"), QStringLiteral("Videos"), home),
        xdgUserDirectory(QStringLiteral("XDG_PUBLICSHARE_DIR"), QStringLiteral("Public"), home),
        xdgUserDirectory(QStringLiteral("XDG_TEMPLATES_DIR"), QStringLiteral("Templates"), home),
    };
    const QStringList labels {
        QStringLiteral("Desktop"), QStringLiteral("Documents"), QStringLiteral("Downloads"),
        QStringLiteral("Pictures"), QStringLiteral("Music"), QStringLiteral("Videos"),
        QStringLiteral("Public"), QStringLiteral("Templates"),
    };
    const QStringList icons {
        QStringLiteral("user-desktop"), QStringLiteral("folder-documents"),
        QStringLiteral("folder-downloads"), QStringLiteral("folder-pictures"),
        QStringLiteral("folder-music"), QStringLiteral("folder-videos"),
        QStringLiteral("folder-publicshare"), QStringLiteral("folder-templates"),
    };
    QVariantMap item;
    const int index = defaults.indexOf(path);
    item.insert(QStringLiteral("path"), path);
    item.insert(QStringLiteral("label"), index >= 0 ? labels.at(index) : QFileInfo(path).fileName());
    item.insert(QStringLiteral("icon"), index >= 0 ? icons.at(index) : QStringLiteral("inode-directory"));
    item.insert(
        QStringLiteral("id"),
        index >= 0 ? QStringLiteral("builtin:%1").arg(index)
                   : QStringLiteral("custom:%1").arg(path));
    item.insert(QStringLiteral("builtIn"), index >= 0);
    return item;
}

} // namespace

AppStateFacade::AppStateFacade(
    NavigationController *navigation,
    SelectionController *selection,
    DirectoryModel *model,
    QObject *parent,
    Services::SettingsService *settingsService,
    FileOperationsController *fileOperations,
    DeviceController *devices,
    Runtime::ExplorerRuntimePaths runtimePaths,
    RecentController *recentController,
    Services::FilesystemService *filesystemService,
    OpenWithController *openWith,
    Services::LaunchService *launchService,
    Services::WallpaperService *wallpaperService,
    Services::MimeAppsService *mimeAppsService)
    : QObject(parent)
    , m_navigation(navigation)
    , m_selection(selection)
    , m_model(model)
    , m_settingsService(settingsService)
    , m_fileOperations(fileOperations)
    , m_devices(devices)
    , m_recentController(recentController)
    , m_filesystemService(filesystemService)
    , m_openWith(openWith)
    , m_launchService(launchService)
    , m_wallpaperService(wallpaperService)
    , m_mimeAppsService(mimeAppsService)
    , m_runtimePaths(std::move(runtimePaths))
{
    Q_ASSERT(m_navigation != nullptr);
    Q_ASSERT(m_selection != nullptr);
    Q_ASSERT(m_model != nullptr);

    if (m_settingsService != nullptr) {
        m_settings = m_settingsService->load();
    }
    m_navigation->setShowHidden(m_settings.showHidden);
    m_navigation->setSortField(m_settings.sortField);
    m_navigation->setSortAscending(m_settings.sortAscending);
    m_navigation->setFoldersFirst(m_settings.foldersFirst);
    m_navigation->setPreviews(m_settings.showPreview);

    connect(
        m_navigation,
        &NavigationController::listingOptionsChanged,
        this,
        &AppStateFacade::handleListingOptionsChanged);
    connect(
        m_navigation,
        &NavigationController::currentPathChanged,
        this,
        &AppStateFacade::persistCurrentPath);

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
                emit autoMountDeviceIdsJsonChanged();
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
                if ((result.operation == QStringLiteral("archive-extract")
                     || result.operation == QStringLiteral("archive-compress"))
                    && result.requestId == m_archiveRequest) {
                    m_archiveRunning = false;
                    m_archivePercent = result.ok ? 100 : 0;
                    m_archiveProgress = result.ok ? 1.0 : 0.0;
                    m_archiveDoneCount = result.ok ? 1 : 0;
                    m_archiveTotalCount = 1;
                    m_archiveError = result.ok ? QString() : result.errorMessage;
                    m_archiveStatus = result.ok ? QStringLiteral("Concluído") : QStringLiteral("Falha");
                    if (result.ok) {
                        QJsonValue resultPath = result.data.value(QStringLiteral("destination"));
                        if (!resultPath.isString()) {
                            resultPath = result.data.value(QStringLiteral("path"));
                        }
                        m_archiveDestinationResult = resultPath.toString();
                        if (result.operation == QStringLiteral("archive-extract")
                            && !m_archiveDestinationResult.isEmpty()) {
                            m_navigation->navigateTo(m_archiveDestinationResult);
                        }
                    }
                    emit archiveStateChanged();
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

QAbstractItemModel *AppStateFacade::fileModel() const
{
    return m_model;
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
    return m_settings.showPreview;
}

bool AppStateFacade::previewsEnabled() const
{
    return m_settings.showPreview;
}

QString AppStateFacade::viewMode() const
{
    return m_settings.viewMode;
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
    return m_settings.groupingEnabled;
}

double AppStateFacade::zoomLevel() const
{
    return m_settings.zoomLevel;
}

QString AppStateFacade::autoMountDeviceIdsJson() const
{
    return m_devices == nullptr
        ? m_settings.autoMountDeviceIdsJson
        : m_devices->autoMountDeviceIdsJson();
}

QString AppStateFacade::sidebarFavoritesJson() const
{
    return m_settings.sidebarFavoritesJson;
}

QString AppStateFacade::sidebarHiddenDefaultFavoritesJson() const
{
    return m_settings.sidebarHiddenDefaultFavoritesJson;
}

QVariantList AppStateFacade::sidebarFavorites() const
{
    QVariantList persisted;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_settings.sidebarFavoritesJson.toUtf8());
    if (document.isArray()) {
        for (const QJsonValue &value : document.array()) {
            QVariantMap item;
            if (value.isObject()) {
                item = value.toObject().toVariantMap();
            } else if (value.isString()) {
                item.insert(QStringLiteral("path"), value.toString());
            }
            const QString path = item.value(QStringLiteral("path")).toString();
            if (path.isEmpty()) {
                continue;
            }
            item.insert(QStringLiteral("path"), normalizeFavoritePath(path));
            if (item.value(QStringLiteral("label")).toString().isEmpty()) {
                item.insert(
                    QStringLiteral("label"),
                    QFileInfo(item.value(QStringLiteral("path")).toString()).fileName());
            }
            if (item.value(QStringLiteral("icon")).toString().isEmpty()) {
                item.insert(QStringLiteral("icon"), QStringLiteral("inode-directory"));
            }
            persisted.append(item);
        }
    }

    const QVariantList hidden = sidebarHiddenDefaultFavorites();
    const QStringList defaults = defaultSidebarFavoritePaths();
    QVariantList result;
    QStringList seen;
    for (const QVariant &value : persisted) {
        QVariantMap item;
        item = value.toMap();
        const QString path = normalizeFavoritePath(
            item.value(QStringLiteral("path")).toString());
        if (path.isEmpty() || seen.contains(path)) {
            continue;
        }
        if (defaults.contains(path)) {
            if (std::any_of(hidden.cbegin(), hidden.cend(), [&path](const QVariant &value) {
                    return normalizeFavoritePath(value.toString()) == path;
                })) {
                continue;
            }
            item = defaultFavoriteItem(path, homePath());
        }
        seen.append(path);
        result.append(item);
    }
    for (const QString &path : defaults) {
        if (seen.contains(path)
            || std::any_of(hidden.cbegin(), hidden.cend(), [&path](const QVariant &value) {
                   return value.toString() == path;
               })) {
            continue;
        }
        result.append(defaultFavoriteItem(path, homePath()));
        seen.append(path);
    }
    return result;
}

QVariantList AppStateFacade::sidebarHiddenDefaultFavorites() const
{
    QVariantList result;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_settings.sidebarHiddenDefaultFavoritesJson.toUtf8());
    if (!document.isArray()) {
        return result;
    }
    for (const QJsonValue &value : document.array()) {
        if (value.isString()) {
            result.append(normalizeFavoritePath(value.toString()));
        }
    }
    return result;
}

int AppStateFacade::sidebarFavoritesRevision() const
{
    return m_sidebarFavoritesRevision;
}

QStringList AppStateFacade::defaultSidebarFavoritePaths() const
{
    const QString root = homePath();
    const QStringList candidates {
        xdgUserDirectory(QStringLiteral("XDG_DESKTOP_DIR"), QStringLiteral("Desktop"), root),
        xdgUserDirectory(QStringLiteral("XDG_DOCUMENTS_DIR"), QStringLiteral("Documents"), root),
        xdgUserDirectory(QStringLiteral("XDG_DOWNLOAD_DIR"), QStringLiteral("Downloads"), root),
        xdgUserDirectory(QStringLiteral("XDG_PICTURES_DIR"), QStringLiteral("Pictures"), root),
        xdgUserDirectory(QStringLiteral("XDG_MUSIC_DIR"), QStringLiteral("Music"), root),
        xdgUserDirectory(QStringLiteral("XDG_VIDEOS_DIR"), QStringLiteral("Videos"), root),
        xdgUserDirectory(QStringLiteral("XDG_PUBLICSHARE_DIR"), QStringLiteral("Public"), root),
        xdgUserDirectory(QStringLiteral("XDG_TEMPLATES_DIR"), QStringLiteral("Templates"), root),
    };
    QStringList result;
    for (const QString &path : candidates) {
        const QString normalized = normalizeFavoritePath(path);
        if (!normalized.isEmpty() && !result.contains(normalized)) {
            result.append(normalized);
        }
    }
    return result;
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

bool AppStateFacade::archiveExtractionRunning() const { return m_archiveRunning; }
double AppStateFacade::archiveExtractionProgress() const { return m_archiveProgress; }
int AppStateFacade::archiveExtractionPercent() const { return m_archivePercent; }
QString AppStateFacade::archiveExtractionFileName() const { return m_archiveFileName; }
QString AppStateFacade::archiveExtractionStatus() const { return m_archiveStatus; }
QString AppStateFacade::archiveExtractionError() const { return m_archiveError; }
QString AppStateFacade::archiveExtractionDestination() const { return m_archiveDestinationResult; }
int AppStateFacade::archiveExtractionDoneCount() const { return m_archiveDoneCount; }
int AppStateFacade::archiveExtractionTotalCount() const { return m_archiveTotalCount; }
QString AppStateFacade::archiveExtractionRemainingText() const { return m_archiveRunning ? QStringLiteral("Aguardando...") : QString(); }
bool AppStateFacade::archivePasswordPromptVisible() const { return m_archivePasswordPrompt; }
QString AppStateFacade::archivePasswordError() const { return m_archivePasswordError; }
bool AppStateFacade::archiveConflictVisible() const { return m_archiveConflict; }
QString AppStateFacade::archiveConflictDestination() const { return m_archiveConflictDestination; }
QString AppStateFacade::archiveConflictName() const { return m_archiveConflictName; }
bool AppStateFacade::appImageInstallRunning() const { return m_appImageInstallRunning; }
bool AppStateFacade::wallpaperApplyRunning() const { return m_wallpaperApplyRunning; }

void AppStateFacade::setShowPreview(bool showPreviewValue)
{
    if (m_settings.showPreview == showPreviewValue) {
        return;
    }
    m_settings.showPreview = showPreviewValue;
    persistSettings();
    m_navigation->setPreviews(showPreviewValue);
    emit showPreviewChanged();
}

void AppStateFacade::setPreviewsEnabled(bool enabled)
{
    setShowPreview(enabled);
}

void AppStateFacade::setViewMode(const QString &viewModeValue)
{
    if (m_settings.viewMode == viewModeValue) {
        return;
    }
    m_settings.viewMode = viewModeValue;
    persistSettings();
    emit viewModeChanged();
}

void AppStateFacade::setSortField(const QString &sortFieldValue)
{
    if (m_navigation->sortField() == sortFieldValue) {
        return;
    }
    m_settings.sortField = sortFieldValue;
    persistSettings();
    m_navigation->setSortField(sortFieldValue);
}

void AppStateFacade::setSortAsc(bool sortAscendingValue)
{
    if (m_navigation->sortAscending() == sortAscendingValue) {
        return;
    }
    m_settings.sortAscending = sortAscendingValue;
    persistSettings();
    m_navigation->setSortAscending(sortAscendingValue);
}

void AppStateFacade::setShowHidden(bool showHiddenValue)
{
    if (m_navigation->showHidden() == showHiddenValue) {
        return;
    }
    m_settings.showHidden = showHiddenValue;
    persistSettings();
    m_navigation->setShowHidden(showHiddenValue);
}

void AppStateFacade::setFoldersFirst(bool foldersFirstValue)
{
    if (m_navigation->foldersFirst() == foldersFirstValue) {
        return;
    }
    m_settings.foldersFirst = foldersFirstValue;
    persistSettings();
    m_navigation->setFoldersFirst(foldersFirstValue);
}

void AppStateFacade::setGroupingEnabled(bool groupingEnabledValue)
{
    if (m_settings.groupingEnabled == groupingEnabledValue) {
        return;
    }
    m_settings.groupingEnabled = groupingEnabledValue;
    persistSettings();
    emit groupingEnabledChanged();
}

void AppStateFacade::setZoomLevel(double zoomLevelValue)
{
    zoomLevelValue = qBound(0.75, zoomLevelValue, 2.0);
    if (qFuzzyCompare(m_settings.zoomLevel, zoomLevelValue)) {
        return;
    }
    m_settings.zoomLevel = zoomLevelValue;
    persistSettings();
    emit zoomLevelChanged();
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
    if (m_settings.autoMountDeviceIdsJson == json) {
        return;
    }
    m_settings.autoMountDeviceIdsJson = json;
    persistSettings();
    emit autoMountDeviceIdsJsonChanged();
}

void AppStateFacade::setSidebarFavoritesJson(const QString &json)
{
    if (m_settings.sidebarFavoritesJson == json) {
        return;
    }
    m_settings.sidebarFavoritesJson = json;
    persistSettings();
    emit sidebarFavoritesJsonChanged();
    ++m_sidebarFavoritesRevision;
    emit sidebarFavoritesChanged();
}

void AppStateFacade::setSidebarHiddenDefaultFavoritesJson(const QString &json)
{
    if (m_settings.sidebarHiddenDefaultFavoritesJson == json) {
        return;
    }
    m_settings.sidebarHiddenDefaultFavoritesJson = json;
    persistSettings();
    emit sidebarHiddenDefaultFavoritesJsonChanged();
    ++m_sidebarFavoritesRevision;
    emit sidebarFavoritesChanged();
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

void AppStateFacade::startArchiveExtraction(const QString &path, const QString &folderName)
{
    if (m_filesystemService == nullptr || path.isEmpty()) {
        return;
    }
    m_archivePath = path;
    const QString defaultName = QFileInfo(path).completeBaseName();
    m_archiveDestination = QDir(m_navigation->currentPath()).filePath(
        folderName.isEmpty() ? defaultName : folderName);
    m_archiveConflictPolicy = QStringLiteral("keep-both");
    m_archiveFileName = QFileInfo(path).fileName();
    m_archiveStatus = QStringLiteral("Extraindo...");
    m_archiveError.clear();
    m_archiveDestinationResult.clear();
    m_archiveRunning = true;
    m_archivePasswordPrompt = false;
    m_archiveConflict = false;
    m_archivePercent = 0;
    m_archiveProgress = 0.0;
    m_archiveDoneCount = 0;
    m_archiveTotalCount = 1;
    emit archiveStateChanged();
    m_archiveRequest = m_filesystemService->archiveExtract(
        m_archivePath, m_archiveDestination, QString(), m_archiveConflictPolicy);
}

void AppStateFacade::submitArchivePassword(const QString &password)
{
    if (m_filesystemService == nullptr || m_archivePath.isEmpty()) {
        return;
    }
    m_archivePasswordPrompt = false;
    m_archivePasswordError.clear();
    m_archiveRunning = true;
    emit archiveStateChanged();
    m_archiveRequest = m_filesystemService->archiveExtract(
        m_archivePath, m_archiveDestination, password, m_archiveConflictPolicy);
}

void AppStateFacade::cancelArchivePassword()
{
    m_archivePasswordPrompt = false;
    emit archiveStateChanged();
}
void AppStateFacade::submitArchiveConflict(const QString &policy)
{
    m_archiveConflictPolicy = policy.isEmpty() ? QStringLiteral("keep-both") : policy;
    m_archiveConflict = false;
    submitArchivePassword(QString());
}
void AppStateFacade::cancelArchiveConflict()
{
    m_archiveConflict = false;
    emit archiveStateChanged();
}
void AppStateFacade::startFolderCompression(const QString &path, const QString &format)
{
    if (m_filesystemService == nullptr || path.isEmpty()) {
        return;
    }
    const QString suffix = format.isEmpty() ? QStringLiteral("tar.gz") : format;
    m_archivePath = path;
    m_archiveFileName = QFileInfo(path).fileName();
    m_archiveDestination = QDir(m_navigation->currentPath()).filePath(
        m_archiveFileName + QStringLiteral(".") + suffix);
    m_archiveStatus = QStringLiteral("Comprimindo...");
    m_archiveError.clear();
    m_archiveRunning = true;
    m_archivePercent = 0;
    m_archiveProgress = 0.0;
    emit archiveStateChanged();
    m_archiveRequest = m_filesystemService->archiveCompress(
        path, m_archiveDestination, suffix);
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
    Q_UNUSED(size);
    if (iconName.isEmpty() || themeName.isEmpty()) {
        return {};
    }
    return QUrl::fromLocalFile(QDir(homePath()).filePath(
        QStringLiteral(".local/share/icons/%1/mimes/scalable/%2.svg")
            .arg(themeName, iconName))).toString();
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
    return path.startsWith(QLatin1Char('/')) && !isTrashPath(path);
}

bool AppStateFacade::isSidebarFavorite(const QString &path) const
{
    const QString normalizedPath = normalizeFavoritePath(path);
    for (const QVariant &value : sidebarFavorites()) {
        if (normalizeFavoritePath(value.toMap().value(QStringLiteral("path")).toString())
            == normalizedPath) {
            return true;
        }
    }
    const QVariantList hidden = sidebarHiddenDefaultFavorites();
    for (const QString &defaultPath : defaultSidebarFavoritePaths()) {
        if (defaultPath != normalizedPath) {
            continue;
        }
        return std::none_of(
            hidden.cbegin(),
            hidden.cend(),
            [&normalizedPath](const QVariant &value) {
                return normalizeFavoritePath(value.toString()) == normalizedPath;
            });
    }
    return false;
}

QVariantList AppStateFacade::visibleDefaultSidebarFavorites(
    const QVariantList &items) const
{
    const QVariantList hidden = sidebarHiddenDefaultFavorites();
    QVariantList visible;
    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        const QString path = item.value(QStringLiteral("path")).toString();
        const bool isHidden = std::any_of(
            hidden.cbegin(),
            hidden.cend(),
            [&path](const QVariant &hiddenPath) { return hiddenPath.toString() == path; });
        if (!isHidden) {
            visible.append(item);
        }
    }
    return visible;
}

void AppStateFacade::pinSidebarFavorite(
    const QString &path,
    const QString &label,
    const QString &icon)
{
    const QString normalizedPath = normalizeFavoritePath(path);
    if (!canPinSidebarFavorite(normalizedPath)) {
        return;
    }
    if (defaultSidebarFavoritePaths().contains(normalizedPath)) {
        QVariantList hidden;
        for (const QVariant &value : sidebarHiddenDefaultFavorites()) {
            if (normalizeFavoritePath(value.toString()) != normalizedPath) {
                hidden.append(value);
            }
        }
        setSidebarHiddenDefaultFavoritesJson(
            QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                  .toJson(QJsonDocument::Compact)));
        return;
    }
    if (isSidebarFavorite(normalizedPath)) {
        return;
    }

    QVariantList favorites = sidebarFavorites();
    QVariantMap item;
    item.insert(QStringLiteral("label"), label.isEmpty() ? QFileInfo(normalizedPath).fileName() : label);
    item.insert(QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("inode-directory") : icon);
    item.insert(QStringLiteral("path"), normalizedPath);
    favorites.append(item);
    setSidebarFavoritesJson(
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(favorites))
                              .toJson(QJsonDocument::Compact)));
}

void AppStateFacade::removeSidebarFavorite(const QString &path)
{
    const QString normalizedPath = normalizeFavoritePath(path);
    if (defaultSidebarFavoritePaths().contains(normalizedPath)) {
        QVariantList hidden = sidebarHiddenDefaultFavorites();
        if (std::none_of(
                hidden.cbegin(),
                hidden.cend(),
                [&normalizedPath](const QVariant &value) {
                    return normalizeFavoritePath(value.toString()) == normalizedPath;
                })) {
            hidden.append(normalizedPath);
            setSidebarHiddenDefaultFavoritesJson(
                QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                      .toJson(QJsonDocument::Compact)));
        }
        return;
    }

    QVariantList favorites;
    for (const QVariant &value : sidebarFavorites()) {
        if (normalizeFavoritePath(value.toMap().value(QStringLiteral("path")).toString())
            != normalizedPath) {
            favorites.append(value);
        }
    }
    setSidebarFavoritesJson(
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(favorites))
                              .toJson(QJsonDocument::Compact)));
}

void AppStateFacade::moveSidebarFavorite(const QString &path, int targetIndex)
{
    const QString normalizedPath = normalizeFavoritePath(path);
    QVariantList favorites = sidebarFavorites();
    int currentIndex = -1;
    for (int index = 0; index < favorites.size(); ++index) {
        if (normalizeFavoritePath(
                favorites.at(index).toMap().value(QStringLiteral("path")).toString())
            == normalizedPath) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex < 0 || favorites.size() < 2) {
        return;
    }
    targetIndex = qBound(0, targetIndex, favorites.size() - 1);
    if (currentIndex == targetIndex) {
        return;
    }
    const QVariant item = favorites.takeAt(currentIndex);
    favorites.insert(targetIndex, item);
    setSidebarFavoritesJson(
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(favorites))
                              .toJson(QJsonDocument::Compact)));
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

void AppStateFacade::handleListingOptionsChanged()
{
    m_settings.showHidden = m_navigation->showHidden();
    m_settings.sortField = m_navigation->sortField();
    m_settings.sortAscending = m_navigation->sortAscending();
    m_settings.foldersFirst = m_navigation->foldersFirst();
    persistSettings();
    emit sortFieldChanged();
    emit sortAscChanged();
    emit showHiddenChanged();
    emit foldersFirstChanged();
}

void AppStateFacade::persistCurrentPath()
{
    m_settings.currentPath = m_navigation->currentPath();
    persistSettings();
}

void AppStateFacade::persistSettings()
{
    if (m_settingsService != nullptr) {
        m_settingsService->save(m_settings);
    }
}

} // namespace Astrea::Explorer::Native::Backend
