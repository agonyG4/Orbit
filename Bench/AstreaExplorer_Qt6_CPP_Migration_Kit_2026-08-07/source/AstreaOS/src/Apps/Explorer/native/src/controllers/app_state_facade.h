#pragma once

#include <QAbstractItemModel>
#include <QStringList>

#include "controllers/navigation_controller.h"
#include "controllers/selection_controller.h"

namespace Astrea::Explorer::Native::Backend {

class AppStateFacade final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *fileModel READ fileModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIdx READ historyIdx NOTIFY historyChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex NOTIFY activeTabIndexChanged)
    Q_PROPERTY(bool loadingDir READ loadingDir NOTIFY loadingDirChanged)
    Q_PROPERTY(QString loadError READ loadError NOTIFY loadErrorChanged)
    Q_PROPERTY(bool remoteDirectoryActive READ remoteDirectoryActive NOTIFY remoteDirectoryActiveChanged)
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(QString selectedFile READ selectedFile NOTIFY selectedFileChanged)
    Q_PROPERTY(QStringList selectedFiles READ selectedFiles NOTIFY selectedFilesChanged)
    Q_PROPERTY(int lastSelectedIndex READ lastSelectedIndex NOTIFY lastSelectedIndexChanged)
    Q_PROPERTY(int fileModelRevision READ fileModelRevision NOTIFY fileModelRevisionChanged)
    Q_PROPERTY(bool fileModelFilling READ fileModelFilling CONSTANT)

public:
    AppStateFacade(
        NavigationController *navigation,
        SelectionController *selection,
        DirectoryModel *model,
        QObject *parent = nullptr);

    QAbstractItemModel *fileModel() const;
    QString currentPath() const;
    QStringList history() const;
    int historyIdx() const;
    int activeTabIndex() const;
    bool loadingDir() const;
    QString loadError() const;
    bool remoteDirectoryActive() const;
    bool searchActive() const;
    QString searchQuery() const;
    QString selectedFile() const;
    QStringList selectedFiles() const;
    int lastSelectedIndex() const;
    int fileModelRevision() const;
    bool fileModelFilling() const;

    Q_INVOKABLE BackendRequestId navigateTo(const QString &path);
    Q_INVOKABLE BackendRequestId submitSearch(const QString &root, const QString &query);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void createTab(const QString &path = QString());
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE BackendRequestId refreshCurrentFolder();

    Q_INVOKABLE bool isSelected(const QString &name) const;
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void selectByName(const QString &name);
    Q_INVOKABLE void handleSelection(
        const QString &name,
        int index,
        bool ctrlMode,
        bool shiftMode,
        bool preserveCurrentSelection);

signals:
    void currentPathChanged();
    void historyChanged();
    void activeTabIndexChanged();
    void loadingDirChanged();
    void loadErrorChanged();
    void remoteDirectoryActiveChanged();
    void searchStateChanged();
    void selectedFileChanged();
    void selectedFilesChanged();
    void lastSelectedIndexChanged();
    void fileModelRevisionChanged();

private slots:
    void handleModelReset();

private:
    NavigationController *m_navigation = nullptr;
    SelectionController *m_selection = nullptr;
    DirectoryModel *m_model = nullptr;
    int m_fileModelRevision = 0;
};

} // namespace Astrea::Explorer::Native::Backend
