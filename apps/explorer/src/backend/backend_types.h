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
    bool previews = true;
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
    QStringList fileIconNames;
    QUrl fileIconFileUrl;
    QString fileIconFileVersion;
    QStringList fileEmblemNames;
    bool fileIconMetadataReady = false;
    bool fileIsSymlink = false;
    bool fileSymlinkBroken = false;
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

struct DirectoryMetricsRequest
{
    QStringList paths;
};

struct DirectoryMetricsProgress
{
    BackendRequestId requestId = 0;
    QString operation;
    QString state;
    qint64 bytes = 0;
    qint64 fileCount = 0;
    qint64 directoryCount = 0;
    qint64 unreadableCount = 0;
    qint64 scannedEntryCount = 0;
};

struct DirectoryMetricsResult
{
    BackendRequestId requestId = 0;
    QString operation;
    QString state;
    qint64 bytes = 0;
    qint64 fileCount = 0;
    qint64 directoryCount = 0;
    qint64 unreadableCount = 0;
    qint64 scannedEntryCount = 0;
    QString errorCode;
    QString errorMessage;
};

struct ArchiveOperationRequest
{
    QString kind;
    QStringList sources;
    QString archivePath;
    QString destination;
    QString format;
    QString profile;
    QString password;
    QString conflictPolicy {QStringLiteral("keep-both")};
    QString destinationMode {QStringLiteral("new-directory")};
};

struct ArchiveCapability
{
    QString id;
    QString label;
    QString extension;
    bool createSupported = false;
    bool extractSupported = false;
    QStringList profiles;
    QString createProvider;
    QString extractProvider;
    bool createPasswordSupported = false;
    bool extractPasswordSupported = false;
};

struct ArchiveOperationProgress
{
    BackendRequestId requestId = 0;
    QString operation;
    QString phase;
    int doneCount = 0;
    int totalCount = 0;
    qint64 bytesDone = -1;
    qint64 bytesTotal = -1;
    double progress = 0.0;
    int percent = 0;
    QString currentPath;
    QString currentName;
    QString statusText;
};

struct ArchiveOperationResult
{
    BackendRequestId requestId = 0;
    QString operation;
    QString state;
    QString errorCode;
    QString errorMessage;
    QString destination;
    QString phase;
    int doneCount = 0;
    int totalCount = 0;
    qint64 bytesDone = -1;
    qint64 bytesTotal = -1;
    double progress = 0.0;
    int percent = 0;
    QVector<ArchiveCapability> capabilities;
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
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DirectoryMetricsRequest)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DirectoryMetricsProgress)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DirectoryMetricsResult)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::ArchiveOperationRequest)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::ArchiveCapability)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::ArchiveCapability>)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::ArchiveOperationProgress)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::ArchiveOperationResult)
