#include "controllers/app_state_facade.h"

namespace Astrea::Explorer::Native::Backend {

AppStateFacade::AppStateFacade(
    NavigationController *navigation,
    SelectionController *selection,
    DirectoryModel *model,
    QObject *parent,
    Services::SettingsService *settingsService)
    : QObject(parent)
    , m_navigation(navigation)
    , m_selection(selection)
    , m_model(model)
    , m_settingsService(settingsService)
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
        &AppStateFacade::handleModelReset);
}

QAbstractItemModel *AppStateFacade::fileModel() const
{
    return m_model;
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
    return false;
}

bool AppStateFacade::showPreview() const
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
    return m_settings.autoMountDeviceIdsJson;
}

QString AppStateFacade::sidebarFavoritesJson() const
{
    return m_settings.sidebarFavoritesJson;
}

QString AppStateFacade::sidebarHiddenDefaultFavoritesJson() const
{
    return m_settings.sidebarHiddenDefaultFavoritesJson;
}

void AppStateFacade::setShowPreview(bool showPreviewValue)
{
    if (m_settings.showPreview == showPreviewValue) {
        return;
    }
    m_settings.showPreview = showPreviewValue;
    persistSettings();
    emit showPreviewChanged();
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
    if (qFuzzyCompare(m_settings.zoomLevel, zoomLevelValue)) {
        return;
    }
    m_settings.zoomLevel = zoomLevelValue;
    persistSettings();
    emit zoomLevelChanged();
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
}

void AppStateFacade::setSidebarHiddenDefaultFavoritesJson(const QString &json)
{
    if (m_settings.sidebarHiddenDefaultFavoritesJson == json) {
        return;
    }
    m_settings.sidebarHiddenDefaultFavoritesJson = json;
    persistSettings();
    emit sidebarHiddenDefaultFavoritesJsonChanged();
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

BackendRequestId AppStateFacade::refreshCurrentFolder()
{
    return m_navigation->refreshCurrentFolder();
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

void AppStateFacade::handleModelReset()
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
