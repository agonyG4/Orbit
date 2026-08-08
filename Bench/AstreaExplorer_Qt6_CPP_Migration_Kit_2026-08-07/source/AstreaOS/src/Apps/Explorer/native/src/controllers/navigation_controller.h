#pragma once

#include <QHash>
#include <QStringList>

#include "backend/rust_backend_client.h"
#include "models/directory_model.h"
#include "services/directory_watch_service.h"

namespace Astrea::Explorer::Native::Backend {

class NavigationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabsChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex NOTIFY activeTabIndexChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(bool remoteDirectoryActive READ remoteDirectoryActive NOTIFY remoteStateChanged)
    Q_PROPERTY(bool showHidden READ showHidden NOTIFY listingOptionsChanged)
    Q_PROPERTY(QString sortField READ sortField NOTIFY listingOptionsChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY listingOptionsChanged)
    Q_PROPERTY(bool foldersFirst READ foldersFirst NOTIFY listingOptionsChanged)
    Q_PROPERTY(bool previews READ previews NOTIFY listingOptionsChanged)

public:
    NavigationController(
        IRustBackendClient *client,
        DirectoryModel *model,
        DirectoryWatchService *watcher,
        QObject *parent = nullptr);

    QString currentPath() const;
    QStringList history() const;
    int historyIndex() const;
    int tabCount() const;
    int activeTabIndex() const;
    bool loading() const;
    QString loadError() const;
    bool searchActive() const;
    QString searchQuery() const;
    bool remoteDirectoryActive() const;
    bool showHidden() const;
    QString sortField() const;
    bool sortAscending() const;
    bool foldersFirst() const;
    bool previews() const;

    void setShowHidden(bool showHidden);
    void setSortField(const QString &sortField);
    void setSortAscending(bool sortAscending);
    void setFoldersFirst(bool foldersFirst);
    void setPreviews(bool previews);

    Q_INVOKABLE BackendRequestId navigateTo(const QString &path);
    Q_INVOKABLE BackendRequestId submitSearch(const QString &root, const QString &query);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void createTab(const QString &initialPath = QString());
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE BackendRequestId refreshCurrentFolder();

signals:
    void currentPathChanged();
    void historyChanged();
    void tabsChanged();
    void activeTabIndexChanged();
    void loadingChanged();
    void loadErrorChanged();
    void searchStateChanged();
    void remoteStateChanged();
    void listingOptionsChanged();
    void navigationFailed(const Astrea::Explorer::Native::Backend::BackendError &error);

private slots:
    void handleListReady(
        BackendRequestId requestId,
        const QVector<DirectoryEntry> &entries);
    void handleSearchReady(
        BackendRequestId requestId,
        const QVector<DirectoryEntry> &entries);
    void handleBackendFailure(const BackendError &error);
    void handleDirectoryChanged(const QString &path);

private:
    enum class RequestKind
    {
        List,
        Search,
    };

    struct PendingRequest
    {
        quint64 generation = 0;
        QString path;
        RequestKind kind = RequestKind::List;
    };

    struct Tab
    {
        int id = 0;
        QString path;
        QStringList history;
        int historyIndex = -1;
    };

    BackendRequestId startList(const QString &path);
    BackendRequestId startSearch(const QString &root, const QString &query);
    void cancelActiveRequest();
    void clearSearchState();
    void syncActiveTab();
    void restoreTab(const Tab &tab);
    void updateWatcher();
    void updateRemoteState(const QVector<DirectoryEntry> &entries);
    static bool isRemotePath(const QString &path);
    void setCurrentPath(const QString &path);
    void setLoading(bool loading);
    void setLoadError(const QString &error);

    IRustBackendClient *m_client = nullptr;
    DirectoryModel *m_model = nullptr;
    DirectoryWatchService *m_watcher = nullptr;
    QHash<BackendRequestId, PendingRequest> m_pendingRequests;
    BackendRequestId m_activeRequest = 0;
    quint64 m_generation = 0;
    QString m_currentPath;
    QStringList m_history;
    int m_historyIndex = -1;
    QVector<Tab> m_tabs;
    int m_activeTabIndex = 0;
    int m_nextTabId = 1;
    bool m_loading = false;
    QString m_loadError;
    bool m_searchActive = false;
    QString m_searchQuery;
    QString m_searchRoot;
    bool m_remoteDirectoryActive = false;
    bool m_showHidden = false;
    QString m_sortField {QStringLiteral("name")};
    bool m_sortAscending = true;
    bool m_foldersFirst = true;
    bool m_previews = true;
};

} // namespace Astrea::Explorer::Native::Backend
