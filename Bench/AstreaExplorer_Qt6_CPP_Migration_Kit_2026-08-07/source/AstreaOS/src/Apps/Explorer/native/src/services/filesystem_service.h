#pragma once

#include <QHash>
#include <QObject>

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Services {

class FilesystemService final : public QObject
{
    Q_OBJECT

public:
    explicit FilesystemService(
        Backend::IRustBackendClient *client,
        QObject *parent = nullptr);

    Backend::BackendRequestId createFolder(
        const QString &basePath,
        const QString &name);
    Backend::BackendRequestId renamePath(
        const QString &sourcePath,
        const QString &newName);
    Backend::BackendRequestId suggestDirectories(
        const QString &basePath,
        const QString &prefix);
    Backend::BackendRequestId checkExecutable(const QString &program);
    Backend::BackendRequestId properties(const QString &path);
    Backend::BackendRequestId createDesktopShortcut(const QString &path);
    Backend::BackendRequestId networkMountProbe(const QString &rootPath);
    Backend::BackendRequestId warmThumbnails(
        const QString &path,
        int offset,
        int limit);
    Backend::BackendRequestId archiveExtract(
        const QString &archivePath,
        const QString &destination,
        const QString &password,
        const QString &conflictPolicy);
    Backend::BackendRequestId archiveCompress(
        const QString &sourcePath,
        const QString &archivePath,
        const QString &format);
    Backend::BackendRequestId installAppImage(const QString &path);
    Backend::BackendRequestId networkMount(const QString &address);
    Backend::BackendRequestId trash(
        const QString &trashFiles,
        const QString &trashInfo,
        const QStringList &paths);
    Backend::BackendRequestId restoreTrash(
        const QString &trashInfo,
        const QString &fallbackDirectory,
        const QStringList &paths);
    Backend::BackendRequestId emptyTrash(
        const QString &trashFiles,
        const QString &trashInfo);

signals:
    void operationFinished(
        const Astrea::Explorer::Native::Backend::UtilityResult &result);

private:
    Backend::BackendRequestId request(
        const QString &operation,
        const QStringList &arguments);

    Backend::IRustBackendClient *m_client = nullptr;
    QHash<Backend::BackendRequestId, QString> m_pending;
};

} // namespace Astrea::Explorer::Native::Services
