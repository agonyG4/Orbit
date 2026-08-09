#include "controllers/navigation_controller.h"

#include <QVariantMap>
#include <QUrl>

namespace Astrea::Explorer::Native::Backend {

NavigationController::NavigationController(
    IRustBackendClient *client,
    DirectoryModel *model,
    DirectoryWatchService *watcher,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_model(model)
    , m_watcher(watcher)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_model != nullptr);
    Q_ASSERT(m_watcher != nullptr);

    qRegisterMetaType<BackendRequestId>();
    qRegisterMetaType<QVector<DirectoryEntry>>();
    qRegisterMetaType<BackendError>();

    connect(
        m_client,
        &IRustBackendClient::listReady,
        this,
        &NavigationController::handleListReady,
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::searchReady,
        this,
        &NavigationController::handleSearchReady,
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::failed,
        this,
        &NavigationController::handleBackendFailure,
        Qt::QueuedConnection);
    connect(
        m_watcher,
        &DirectoryWatchService::directoryChanged,
        this,
        &NavigationController::handleDirectoryChanged);
}

QString NavigationController::currentPath() const
{
    return m_currentPath;
}

QStringList NavigationController::history() const
{
    return m_history;
}

int NavigationController::historyIndex() const
{
    return m_historyIndex;
}

QVariantList NavigationController::tabs() const
{
    QVariantList result;
    result.reserve(m_tabs.size());
    for (const Tab &tab : m_tabs) {
        QVariantMap value;
        value.insert(QStringLiteral("id"), tab.id);
        value.insert(QStringLiteral("path"), tab.path);
        value.insert(QStringLiteral("history"), tab.history);
        value.insert(QStringLiteral("historyIdx"), tab.historyIndex);
        result.append(value);
    }
    return result;
}

QVariantList NavigationController::breadcrumbParts() const
{
    QVariantList result;
    if (m_currentPath.isEmpty()) {
        return result;
    }

    const QUrl currentUrl(m_currentPath);
    if (!currentUrl.scheme().isEmpty() && currentUrl.scheme() != QStringLiteral("file")) {
        QVariantMap part;
        part.insert(QStringLiteral("label"), m_currentPath);
        part.insert(QStringLiteral("path"), m_currentPath);
        result.append(part);
        return result;
    }

    const QString path = currentUrl.isLocalFile() ? currentUrl.toLocalFile() : m_currentPath;
    if (path == QStringLiteral("/")) {
        QVariantMap part;
        part.insert(QStringLiteral("label"), QStringLiteral("/"));
        part.insert(QStringLiteral("path"), QStringLiteral("/"));
        result.append(part);
        return result;
    }

    QString accumulated;
    const QStringList components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &component : components) {
        accumulated += QLatin1Char('/') + component;
        QVariantMap part;
        part.insert(QStringLiteral("label"), component);
        part.insert(QStringLiteral("path"), accumulated);
        result.append(part);
    }
    return result;
}

int NavigationController::tabCount() const
{
    return m_tabs.size();
}

int NavigationController::activeTabIndex() const
{
    return m_activeTabIndex;
}

bool NavigationController::loading() const
{
    return m_loading;
}

QString NavigationController::loadError() const
{
    return m_loadError;
}

bool NavigationController::searchActive() const
{
    return m_searchActive;
}

bool NavigationController::searchVisible() const
{
    return m_searchVisible;
}

QString NavigationController::searchQuery() const
{
    return m_searchQuery;
}

bool NavigationController::remoteDirectoryActive() const
{
    return m_remoteDirectoryActive;
}

bool NavigationController::showHidden() const
{
    return m_showHidden;
}

QString NavigationController::sortField() const
{
    return m_sortField;
}

bool NavigationController::sortAscending() const
{
    return m_sortAscending;
}

bool NavigationController::foldersFirst() const
{
    return m_foldersFirst;
}

bool NavigationController::previews() const
{
    return m_previews;
}

void NavigationController::setSearchQuery(const QString &query)
{
    if (m_searchQuery == query) {
        return;
    }
    m_searchQuery = query;
    emit searchStateChanged();
}

void NavigationController::setShowHidden(bool showHiddenValue)
{
    if (m_showHidden == showHiddenValue) {
        return;
    }
    m_showHidden = showHiddenValue;
    emit listingOptionsChanged();
    if (!m_currentPath.isEmpty()) {
        refreshCurrentFolder();
    }
}

void NavigationController::setSortField(const QString &sortFieldValue)
{
    if (m_sortField == sortFieldValue) {
        return;
    }
    m_sortField = sortFieldValue;
    emit listingOptionsChanged();
    if (!m_currentPath.isEmpty()) {
        refreshCurrentFolder();
    }
}

void NavigationController::setSortAscending(bool sortAscendingValue)
{
    if (m_sortAscending == sortAscendingValue) {
        return;
    }
    m_sortAscending = sortAscendingValue;
    emit listingOptionsChanged();
    if (!m_currentPath.isEmpty()) {
        refreshCurrentFolder();
    }
}

void NavigationController::setFoldersFirst(bool foldersFirstValue)
{
    if (m_foldersFirst == foldersFirstValue) {
        return;
    }
    m_foldersFirst = foldersFirstValue;
    emit listingOptionsChanged();
    if (!m_currentPath.isEmpty()) {
        refreshCurrentFolder();
    }
}

void NavigationController::setPreviews(bool previewsValue)
{
    if (m_previews == previewsValue) {
        return;
    }
    m_previews = previewsValue;
    emit listingOptionsChanged();
    if (!m_currentPath.isEmpty()) {
        refreshCurrentFolder();
    }
}

BackendRequestId NavigationController::navigateTo(const QString &path)
{
    if (path.isEmpty()) {
        return 0;
    }
    if (path == m_currentPath && m_historyIndex >= 0) {
        return 0;
    }

    cancelActiveRequest();
    clearSearchState();

    if (m_tabs.isEmpty()) {
        m_tabs.append({m_nextTabId++, path, {path}, 0});
        emit tabsChanged();
        m_activeTabIndex = 0;
        emit activeTabIndexChanged();
        m_history = {path};
        m_historyIndex = 0;
        emit historyChanged();
    } else {
        m_history = m_history.mid(0, m_historyIndex + 1);
        m_history.append(path);
        m_historyIndex = m_history.size() - 1;
        emit historyChanged();
    }

    setCurrentPath(path);
    if (!m_tabs.isEmpty()) {
        syncActiveTab();
    }
    return startList(path);
}

BackendRequestId NavigationController::submitSearch(
    const QString &root,
    const QString &query)
{
    const QString searchRoot = query.isEmpty() ? m_currentPath : root;
    const QString queryText = query.isEmpty() ? root : query;
    const QString trimmedQuery = queryText.trimmed();
    if (trimmedQuery.isEmpty()) {
        clearSearchState();
        m_searchVisible = false;
        emit searchStateChanged();
        return startList(m_currentPath);
    }

    const QString effectiveRoot = searchRoot.isEmpty() ? m_currentPath : searchRoot;
    if (effectiveRoot.isEmpty()) {
        return 0;
    }

    cancelActiveRequest();
    m_searchVisible = true;
    m_searchActive = true;
    m_searchQuery = trimmedQuery;
    m_searchRoot = effectiveRoot;
    emit searchStateChanged();
    return startSearch(effectiveRoot, trimmedQuery);
}

void NavigationController::startSearch()
{
    if (m_searchVisible) {
        return;
    }
    m_searchVisible = true;
    emit searchStateChanged();
}

void NavigationController::hideSearch()
{
    if (!m_searchVisible && !m_searchActive) {
        return;
    }
    m_searchVisible = false;
    clearSearchState();
    emit searchStateChanged();
}

void NavigationController::clearSearch()
{
    hideSearch();
}

void NavigationController::goBack()
{
    if (m_historyIndex <= 0) {
        return;
    }

    cancelActiveRequest();
    clearSearchState();
    --m_historyIndex;
    emit historyChanged();
    setCurrentPath(m_history.at(m_historyIndex));
    syncActiveTab();
    startList(m_currentPath);
}

void NavigationController::goForward()
{
    if (m_historyIndex < 0 || m_historyIndex >= m_history.size() - 1) {
        return;
    }

    cancelActiveRequest();
    clearSearchState();
    ++m_historyIndex;
    emit historyChanged();
    setCurrentPath(m_history.at(m_historyIndex));
    syncActiveTab();
    startList(m_currentPath);
}

void NavigationController::createTab(const QString &initialPath)
{
    const QString path = initialPath.isEmpty() ? m_currentPath : initialPath;
    if (path.isEmpty()) {
        return;
    }

    cancelActiveRequest();
    clearSearchState();
    if (!m_tabs.isEmpty()) {
        syncActiveTab();
    }

    m_tabs.append({m_nextTabId++, path, {path}, 0});
    emit tabsChanged();
    m_activeTabIndex = m_tabs.size() - 1;
    emit activeTabIndexChanged();
    restoreTab(m_tabs.constLast());
    startList(path);
}

void NavigationController::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs.size() <= 1) {
        return;
    }

    const bool wasActive = index == m_activeTabIndex;
    m_tabs.removeAt(index);
    emit tabsChanged();
    if (!wasActive) {
        if (index < m_activeTabIndex) {
            --m_activeTabIndex;
            emit activeTabIndexChanged();
        }
        return;
    }

    cancelActiveRequest();
    clearSearchState();
    m_activeTabIndex = qMin(index, m_tabs.size() - 1);
    emit activeTabIndexChanged();
    restoreTab(m_tabs.at(m_activeTabIndex));
    startList(m_currentPath);
}

void NavigationController::closeTabById(int tabId)
{
    closeTab(tabIndexById(tabId));
}

void NavigationController::switchTabById(int tabId)
{
    switchTab(tabIndexById(tabId));
}

int NavigationController::tabIndexById(int tabId) const
{
    for (int index = 0; index < m_tabs.size(); ++index) {
        if (m_tabs.at(index).id == tabId) {
            return index;
        }
    }
    return -1;
}

void NavigationController::moveTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size()
        || toIndex < 0 || toIndex >= m_tabs.size()
        || fromIndex == toIndex) {
        return;
    }

    const int activeTabId = m_tabs.at(m_activeTabIndex).id;
    m_tabs.move(fromIndex, toIndex);
    m_activeTabIndex = tabIndexById(activeTabId);
    emit tabsChanged();
    emit activeTabIndexChanged();
}

void NavigationController::switchTab(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_activeTabIndex) {
        return;
    }

    cancelActiveRequest();
    clearSearchState();
    syncActiveTab();
    m_activeTabIndex = index;
    emit activeTabIndexChanged();
    restoreTab(m_tabs.at(index));
    startList(m_currentPath);
}

BackendRequestId NavigationController::refreshCurrentFolder()
{
    if (m_currentPath.isEmpty()) {
        return 0;
    }
    cancelActiveRequest();
    if (m_searchActive) {
        return startSearch(m_searchRoot, m_searchQuery);
    }
    return startList(m_currentPath);
}

BackendRequestId NavigationController::startList(const QString &path)
{
    const quint64 generation = ++m_generation;
    setLoading(true);
    setLoadError(QString());
    m_remoteDirectoryActive = isRemotePath(path);
    emit remoteStateChanged();
    updateWatcher();

    m_model->applyEntries({}, generation);
    ListRequest request;
    request.path = path;
    request.showHidden = m_showHidden;
    request.sortField = m_sortField;
    request.sortAscending = m_sortAscending;
    request.foldersFirst = m_foldersFirst;
    request.previews = m_previews;
    const BackendRequestId requestId = m_client->list(request);
    m_pendingRequests.insert(requestId, {generation, path, RequestKind::List});
    m_activeRequest = requestId;
    return requestId;
}

BackendRequestId NavigationController::startSearch(
    const QString &root,
    const QString &query)
{
    const quint64 generation = ++m_generation;
    setLoading(true);
    setLoadError(QString());
    m_remoteDirectoryActive = isRemotePath(root);
    emit remoteStateChanged();
    updateWatcher();
    m_model->applyEntries({}, generation);

    SearchRequest request;
    request.rootPath = root;
    request.query = query;
    request.showHidden = m_showHidden;
    request.sortField = m_sortField;
    request.sortAscending = m_sortAscending;
    request.foldersFirst = m_foldersFirst;
    const BackendRequestId requestId = m_client->search(request);
    m_pendingRequests.insert(requestId, {generation, root, RequestKind::Search});
    m_activeRequest = requestId;
    return requestId;
}

void NavigationController::cancelActiveRequest()
{
    if (m_activeRequest == 0) {
        return;
    }

    const BackendRequestId requestId = m_activeRequest;
    m_activeRequest = 0;
    m_pendingRequests.remove(requestId);
    m_client->cancel(requestId);
}

void NavigationController::clearSearchState()
{
    if (!m_searchActive && m_searchQuery.isEmpty() && m_searchRoot.isEmpty()) {
        m_searchVisible = false;
        return;
    }
    m_searchVisible = false;
    m_searchActive = false;
    m_searchQuery.clear();
    m_searchRoot.clear();
    emit searchStateChanged();
}

void NavigationController::syncActiveTab()
{
    if (m_activeTabIndex < 0 || m_activeTabIndex >= m_tabs.size()) {
        return;
    }
    Tab &tab = m_tabs[m_activeTabIndex];
    tab.path = m_currentPath;
    tab.history = m_history;
    tab.historyIndex = m_historyIndex;
}

void NavigationController::restoreTab(const Tab &tab)
{
    m_history = tab.history;
    m_historyIndex = tab.historyIndex;
    setCurrentPath(tab.path);
    emit historyChanged();
}

void NavigationController::updateWatcher()
{
    if (m_remoteDirectoryActive || m_searchActive || m_currentPath.isEmpty()) {
        m_watcher->clear();
        return;
    }
    m_watcher->watchLocalDirectory(m_currentPath);
}

void NavigationController::updateRemoteState(const QVector<DirectoryEntry> &entries)
{
    bool remote = isRemotePath(m_currentPath);
    for (const DirectoryEntry &entry : entries) {
        if (entry.fileRemote) {
            remote = true;
            break;
        }
    }
    if (m_remoteDirectoryActive == remote) {
        updateWatcher();
        return;
    }
    m_remoteDirectoryActive = remote;
    emit remoteStateChanged();
    updateWatcher();
}

bool NavigationController::isRemotePath(const QString &path)
{
    if (path.startsWith(QStringLiteral("recent://"))) {
        return false;
    }

    const QUrl url(path);
    if (!url.scheme().isEmpty() && url.scheme() != QStringLiteral("file")) {
        return true;
    }
    if (path.contains(QStringLiteral("/gvfs/"), Qt::CaseInsensitive)) {
        return true;
    }

    const QStringList components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &component : components) {
        if (component.compare(QStringLiteral("rclone"), Qt::CaseInsensitive) == 0
            || component.startsWith(QStringLiteral("rclone-"), Qt::CaseInsensitive)
            || component.startsWith(QStringLiteral("rclone_"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void NavigationController::setCurrentPath(const QString &path)
{
    if (m_currentPath == path) {
        return;
    }
    m_currentPath = path;
    emit currentPathChanged();
}

void NavigationController::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void NavigationController::setLoadError(const QString &error)
{
    if (m_loadError == error) {
        return;
    }
    m_loadError = error;
    emit loadErrorChanged();
}

void NavigationController::handleListReady(
    BackendRequestId requestId,
    const QVector<DirectoryEntry> &entries)
{
    const auto pendingIt = m_pendingRequests.find(requestId);
    if (pendingIt == m_pendingRequests.end() || pendingIt->kind != RequestKind::List) {
        return;
    }
    const PendingRequest pending = pendingIt.value();
    m_pendingRequests.erase(pendingIt);
    if (requestId != m_activeRequest || pending.generation != m_generation
        || pending.path != m_currentPath || m_searchActive) {
        return;
    }

    m_activeRequest = 0;
    updateRemoteState(entries);
    m_model->applyEntries(entries, pending.generation);
    setLoading(false);
    setLoadError(QString());
}

void NavigationController::handleSearchReady(
    BackendRequestId requestId,
    const QVector<DirectoryEntry> &entries)
{
    const auto pendingIt = m_pendingRequests.find(requestId);
    if (pendingIt == m_pendingRequests.end() || pendingIt->kind != RequestKind::Search) {
        return;
    }
    const PendingRequest pending = pendingIt.value();
    m_pendingRequests.erase(pendingIt);
    if (requestId != m_activeRequest || pending.generation != m_generation
        || !m_searchActive || pending.path != m_searchRoot) {
        return;
    }

    m_activeRequest = 0;
    updateRemoteState(entries);
    m_model->applyEntries(entries, pending.generation);
    setLoading(false);
    setLoadError(QString());
}

void NavigationController::handleBackendFailure(const BackendError &error)
{
    const auto pendingIt = m_pendingRequests.find(error.requestId);
    if (pendingIt == m_pendingRequests.end()) {
        return;
    }
    const PendingRequest pending = pendingIt.value();
    m_pendingRequests.erase(pendingIt);
    if (error.requestId != m_activeRequest || pending.generation != m_generation) {
        return;
    }

    m_activeRequest = 0;
    m_model->applyEntries({}, pending.generation);
    setLoading(false);
    setLoadError(error.message);
    emit navigationFailed(error);
}

void NavigationController::handleDirectoryChanged(const QString &path)
{
    if (path != m_currentPath || m_searchActive || m_remoteDirectoryActive) {
        return;
    }
    refreshCurrentFolder();
}

} // namespace Astrea::Explorer::Native::Backend
