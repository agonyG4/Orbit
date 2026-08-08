#include "controllers/app_state_facade.h"

namespace Astrea::Explorer::Native::Backend {

AppStateFacade::AppStateFacade(
    NavigationController *navigation,
    SelectionController *selection,
    DirectoryModel *model,
    QObject *parent)
    : QObject(parent)
    , m_navigation(navigation)
    , m_selection(selection)
    , m_model(model)
{
    Q_ASSERT(m_navigation != nullptr);
    Q_ASSERT(m_selection != nullptr);
    Q_ASSERT(m_model != nullptr);

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

} // namespace Astrea::Explorer::Native::Backend
