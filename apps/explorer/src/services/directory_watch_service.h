#pragma once

#include <QFileSystemWatcher>
#include <QTimer>

namespace Astrea::Explorer::Native::Backend {

class DirectoryWatchService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString watchedPath READ watchedPath NOTIFY watchedPathChanged)

public:
    explicit DirectoryWatchService(QObject *parent = nullptr);

    void watchLocalDirectory(const QString &path);
    void clear();
    QString watchedPath() const;

signals:
    void directoryChanged(const QString &path);
    void watchedPathChanged();

private slots:
    void handleDirectoryChanged(const QString &path);

private:
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_watchedPath;
};

} // namespace Astrea::Explorer::Native::Backend
