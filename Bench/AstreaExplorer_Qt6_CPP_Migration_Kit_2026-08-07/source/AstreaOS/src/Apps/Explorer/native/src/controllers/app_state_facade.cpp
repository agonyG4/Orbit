#include "controllers/app_state_facade.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace Astrea::Explorer::Native::Backend {

AppStateFacade::AppStateFacade(
    NavigationController *navigation,
    SelectionController *selection,
    DirectoryModel *model,
    QObject *parent,
    Services::SettingsService *settingsService,
    FileOperationsController *fileOperations,
    DeviceController *devices,
    Runtime::ExplorerRuntimePaths runtimePaths,
    RecentController *recentController)
    : QObject(parent)
    , m_navigation(navigation)
    , m_selection(selection)
    , m_model(model)
    , m_settingsService(settingsService)
    , m_fileOperations(fileOperations)
    , m_devices(devices)
    , m_recentController(recentController)
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
        &SelectionController::selectedFilesChanged,
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
    if (!m_runtimePaths.helperProgram.isEmpty()) {
        return m_runtimePaths.helperProgram;
    }
    return runtimeRoot().isEmpty()
        ? QString()
        : QDir(runtimeRoot()).filePath(QStringLiteral("Apps/Explorer/explorer_helper.py"));
}

QString AppStateFacade::wallpaperManagerPath() const
{
    return runtimeRoot().isEmpty()
        ? QString()
        : QDir(runtimeRoot()).filePath(
              QStringLiteral("Core/bridge/wallpaper/wallpaper_manager.py"));
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
    return QDir(homePath()).filePath(QStringLiteral(".local/share/Trash/files"));
}

QString AppStateFacade::trashInfoPath() const
{
    return QDir(homePath()).filePath(QStringLiteral(".local/share/Trash/info"));
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
    QVariantList result;
    const QJsonDocument document = QJsonDocument::fromJson(
        m_settings.sidebarFavoritesJson.toUtf8());
    if (!document.isArray()) {
        return result;
    }

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
        if (item.value(QStringLiteral("label")).toString().isEmpty()) {
            item.insert(QStringLiteral("label"), QFileInfo(path).fileName());
        }
        if (item.value(QStringLiteral("icon")).toString().isEmpty()) {
            item.insert(QStringLiteral("icon"), QStringLiteral("inode-directory"));
        }
        result.append(item);
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
            result.append(value.toString());
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
    return {
        QDir(root).filePath(QStringLiteral("Área de trabalho")),
        QDir(root).filePath(QStringLiteral("Documentos")),
        QDir(root).filePath(QStringLiteral("Downloads")),
        QDir(root).filePath(QStringLiteral("Imagens")),
        QDir(root).filePath(QStringLiteral("Músicas")),
        QDir(root).filePath(QStringLiteral("Vídeos")),
        QDir(root).filePath(QStringLiteral("Público")),
        QDir(root).filePath(QStringLiteral("Modelos")),
    };
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
    QStringList paths;
    const QDir currentDirectory(m_navigation->currentPath());
    for (const QString &name : m_selection->selectedFiles()) {
        paths.append(currentDirectory.filePath(name));
    }
    return paths;
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
    m_fileOperations->setSelection(paths);
    if (mode == QStringLiteral("move")) {
        m_fileOperations->cutSelection();
    } else {
        m_fileOperations->copySelection();
    }
    m_fileOperations->pasteFiles(destination);
}

void AppStateFacade::dropFiles(
    const QVariantList &urls,
    const QString &destination,
    const QString &mode)
{
    QStringList paths;
    for (const QVariant &value : urls) {
        const QUrl url(value.toString());
        paths.append(url.isLocalFile() ? url.toLocalFile() : value.toString());
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
    for (const QVariant &value : sidebarFavorites()) {
        if (value.toMap().value(QStringLiteral("path")).toString() == path) {
            return true;
        }
    }
    const QVariantList hidden = sidebarHiddenDefaultFavorites();
    for (const QString &defaultPath : defaultSidebarFavoritePaths()) {
        if (defaultPath != path) {
            continue;
        }
        return std::none_of(
            hidden.cbegin(),
            hidden.cend(),
            [&path](const QVariant &value) { return value.toString() == path; });
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
    if (!canPinSidebarFavorite(path)) {
        return;
    }
    if (defaultSidebarFavoritePaths().contains(path)) {
        QVariantList hidden;
        for (const QVariant &value : sidebarHiddenDefaultFavorites()) {
            if (value.toString() != path) {
                hidden.append(value);
            }
        }
        setSidebarHiddenDefaultFavoritesJson(
            QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                  .toJson(QJsonDocument::Compact)));
        return;
    }
    if (isSidebarFavorite(path)) {
        return;
    }

    QVariantList favorites = sidebarFavorites();
    QVariantMap item;
    item.insert(QStringLiteral("label"), label.isEmpty() ? QFileInfo(path).fileName() : label);
    item.insert(QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("inode-directory") : icon);
    item.insert(QStringLiteral("path"), path);
    favorites.append(item);
    setSidebarFavoritesJson(
        QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(favorites))
                              .toJson(QJsonDocument::Compact)));
}

void AppStateFacade::removeSidebarFavorite(const QString &path)
{
    if (defaultSidebarFavoritePaths().contains(path)) {
        QVariantList hidden = sidebarHiddenDefaultFavorites();
        if (std::none_of(
                hidden.cbegin(),
                hidden.cend(),
                [&path](const QVariant &value) { return value.toString() == path; })) {
            hidden.append(path);
            setSidebarHiddenDefaultFavoritesJson(
                QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(hidden))
                                      .toJson(QJsonDocument::Compact)));
        }
        return;
    }

    QVariantList favorites;
    for (const QVariant &value : sidebarFavorites()) {
        if (value.toMap().value(QStringLiteral("path")).toString() != path) {
            favorites.append(value);
        }
    }
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
