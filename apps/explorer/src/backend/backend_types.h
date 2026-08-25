#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace Astrea::Explorer::Native::Backend {

using BackendRequestId = quint64;

struct ListRequest
{
    QString path;
    bool showHidden = false;
    QString sortField {QStringLiteral("name")};
    bool sortAscending = true;
    bool foldersFirst = true;
    bool previews = true;
};

struct SearchRequest
{
    QString rootPath;
    QString query;
    bool showHidden = false;
    QString sortField {QStringLiteral("name")};
    bool sortAscending = true;
    bool foldersFirst = true;
};

struct DeviceEntry
{
    QString id;
    QString devicePath;
    QString title;
    QString subtitle;
    QString mountPath;
    QString desiredMountPath;
    bool mounted = false;
    bool canMount = false;
    bool canUnmount = false;
    bool canRemount = false;
    bool removable = false;
    QString icon;
};

struct DeviceOperationResult
{
    bool ok = false;
    QString mountPath;
    QString message;
};

struct FileOperationRequest
{
    QString mode;
    QString destination;
    QString conflictPolicy {QStringLiteral("keep-both")};
    QString rename;
    QString progressMode {QStringLiteral("items")};
    QStringList sources;
};

struct FileOperationProgress
{
    BackendRequestId requestId = 0;
    QString mode;
    int doneCount = 0;
    int totalCount = 0;
    int percent = 0;
    QString path;
    QString fileName;
    qint64 doneBytes = 0;
    qint64 totalBytes = 0;
};

struct FileOperationItemResult
{
    QString source;
    QString target;
    QString status;
    QString errorCode;
    QString errorMessage;
};

struct FileOperationResult
{
    BackendRequestId requestId = 0;
    bool ok = false;
    QString mode;
    QString destination;
    int doneCount = 0;
    int totalCount = 0;
    int percent = 0;
    QString errorCode;
    QString errorMessage;
    QString state;
    QVector<FileOperationItemResult> items;
};

struct DirectoryEntry
{
    QString fileName;
    QString filePath;
    QUrl fileUrl;
    bool fileIsDir = false;
    bool fileExecutable = false;
    bool fileHidden = false;
    qint64 fileSize = 0;
    QDateTime fileModified;
    QString fileKind;
    QUrl filePreviewUrl;
    bool fileRemote = false;
    bool fileMetadataLimited = false;
    QString fileFilesystem;
    qint64 lastAccessed = 0;
    QString recentSource;
    QString fileIconName;
    QString trashItemId;
    QString trashInfoPath;
    QString trashLocationId;
    QString trashOriginalPath;
    QDateTime trashDeletionDate;
    QString trashMountTopdir;
    bool trashAvailable = false;
    QString trashOrphanState;
};

struct BackendError
{
    QString code;
    QString message;
    BackendRequestId requestId = 0;
};

struct BackendTransportError
{
    QString code;
    QString message;
    BackendRequestId requestId = 0;
    int exitCode = -1;
    QByteArray stderrData;
    QByteArray stdoutData;
};

struct UtilityRequest
{
    QString operation;
    QStringList arguments;
};

struct UtilityResult
{
    BackendRequestId requestId = 0;
    QString operation;
    bool ok = false;
    QJsonObject data;
    QString errorCode;
    QString errorMessage;
};

} // namespace Astrea::Explorer::Native::Backend

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::BackendError)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::BackendTransportError)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DirectoryEntry)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::DirectoryEntry>)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DeviceEntry)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::DeviceEntry>)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DeviceOperationResult)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::FileOperationRequest)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::FileOperationProgress)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::FileOperationItemResult)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::FileOperationItemResult>)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::FileOperationResult)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::UtilityRequest)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::UtilityResult)
