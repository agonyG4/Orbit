#include "services/directory_watch_service.h"

namespace Astrea::Explorer::Native::Backend {

DirectoryWatchService::DirectoryWatchService(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(260);

    connect(
        &m_watcher,
        &QFileSystemWatcher::directoryChanged,
        this,
        &DirectoryWatchService::handleDirectoryChanged);
    connect(&m_debounce, &QTimer::timeout, this, [this]() {
        if (!m_watchedPath.isEmpty()) {
            emit directoryChanged(m_watchedPath);
        }
    });
}

void DirectoryWatchService::watchLocalDirectory(const QString &path)
{
    if (path == m_watchedPath && m_watcher.directories().contains(path)) {
        return;
    }

    clear();
    if (path.isEmpty() || !m_watcher.addPath(path)) {
        return;
    }

    m_watchedPath = path;
    emit watchedPathChanged();
}

void DirectoryWatchService::clear()
{
    m_debounce.stop();
    const QStringList directories = m_watcher.directories();
    if (!directories.isEmpty()) {
        m_watcher.removePaths(directories);
    }

    if (m_watchedPath.isEmpty()) {
        return;
    }

    m_watchedPath.clear();
    emit watchedPathChanged();
}

QString DirectoryWatchService::watchedPath() const
{
    return m_watchedPath;
}

void DirectoryWatchService::handleDirectoryChanged(const QString &path)
{
    if (path != m_watchedPath) {
        return;
    }
    m_debounce.start();
}

} // namespace Astrea::Explorer::Native::Backend
