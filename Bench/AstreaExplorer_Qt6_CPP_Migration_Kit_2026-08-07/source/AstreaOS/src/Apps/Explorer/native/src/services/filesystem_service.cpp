#include "services/filesystem_service.h"

namespace Astrea::Explorer::Native::Services {

using namespace Astrea::Explorer::Native::Backend;

FilesystemService::FilesystemService(IRustBackendClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    qRegisterMetaType<UtilityResult>();
    connect(
        m_client,
        &IRustBackendClient::utilityReady,
        this,
        [this](BackendRequestId requestId, const UtilityResult &result) {
            if (!m_pending.contains(requestId)) {
                return;
            }
            m_pending.remove(requestId);
            emit operationFinished(result);
        },
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::failed,
        this,
        [this](const BackendError &error) {
            const auto pending = m_pending.constFind(error.requestId);
            if (pending == m_pending.constEnd()) {
                return;
            }
            UtilityResult result;
            result.requestId = error.requestId;
            result.operation = pending.value();
            result.errorCode = error.code;
            result.errorMessage = error.message;
            m_pending.remove(error.requestId);
            emit operationFinished(result);
        },
        Qt::QueuedConnection);
}

BackendRequestId FilesystemService::createFolder(
    const QString &basePath,
    const QString &name)
{
    return request(QStringLiteral("create-folder"), {basePath, name});
}

BackendRequestId FilesystemService::renamePath(
    const QString &sourcePath,
    const QString &newName)
{
    return request(QStringLiteral("rename"), {sourcePath, newName});
}

BackendRequestId FilesystemService::suggestDirectories(
    const QString &basePath,
    const QString &prefix)
{
    return request(QStringLiteral("suggest-dirs"), {basePath, prefix});
}

BackendRequestId FilesystemService::checkExecutable(const QString &program)
{
    return request(QStringLiteral("which"), {program});
}

BackendRequestId FilesystemService::properties(const QString &path)
{
    return request(QStringLiteral("properties"), {path});
}

BackendRequestId FilesystemService::createDesktopShortcut(const QString &path)
{
    return request(QStringLiteral("create-desktop-shortcut"), {path});
}

BackendRequestId FilesystemService::networkMountProbe(const QString &rootPath)
{
    return request(QStringLiteral("network-mount-probe"), {rootPath});
}

BackendRequestId FilesystemService::warmThumbnails(
    const QString &path,
    int offset,
    int limit)
{
    return request(
        QStringLiteral("warm-thumbnails"),
        {path, QStringLiteral("0"), QStringLiteral("name"), QStringLiteral("1"),
         QStringLiteral("1"), QString::number(qMax(0, offset)), QString::number(qMax(1, limit))});
}

BackendRequestId FilesystemService::archiveExtract(
    const QString &archivePath,
    const QString &destination,
    const QString &password,
    const QString &conflictPolicy)
{
    return request(
        QStringLiteral("archive-extract"),
        {archivePath, destination, password, conflictPolicy});
}

BackendRequestId FilesystemService::archiveCompress(
    const QString &sourcePath,
    const QString &archivePath,
    const QString &format)
{
    return request(QStringLiteral("archive-compress"), {sourcePath, archivePath, format});
}

BackendRequestId FilesystemService::installAppImage(const QString &path)
{
    return request(QStringLiteral("install-appimage"), {path});
}

BackendRequestId FilesystemService::networkMount(const QString &address)
{
    return request(QStringLiteral("network-mount"), {address});
}

BackendRequestId FilesystemService::trash(
    const QString &trashFiles,
    const QString &trashInfo,
    const QStringList &paths)
{
    QStringList arguments {trashFiles, trashInfo};
    arguments.append(paths);
    return request(QStringLiteral("trash"), arguments);
}

BackendRequestId FilesystemService::restoreTrash(
    const QString &trashInfo,
    const QString &fallbackDirectory,
    const QStringList &paths)
{
    QStringList arguments {trashInfo, fallbackDirectory};
    arguments.append(paths);
    return request(QStringLiteral("restore-trash"), arguments);
}

BackendRequestId FilesystemService::emptyTrash(
    const QString &trashFiles,
    const QString &trashInfo)
{
    return request(QStringLiteral("empty-trash"), {trashFiles, trashInfo});
}

BackendRequestId FilesystemService::deletePermanently(
    const QStringList &paths,
    const QStringList &metadataPaths)
{
    QStringList arguments = paths;
    if (!metadataPaths.isEmpty()) {
        arguments.append(QStringLiteral("--metadata"));
        arguments.append(metadataPaths);
    }
    return request(QStringLiteral("delete-permanently"), arguments);
}

BackendRequestId FilesystemService::request(
    const QString &operation,
    const QStringList &arguments)
{
    UtilityRequest request;
    request.operation = operation;
    request.arguments = arguments;
    const BackendRequestId requestId = m_client->utility(request);
    m_pending.insert(requestId, operation);
    return requestId;
}

} // namespace Astrea::Explorer::Native::Services
