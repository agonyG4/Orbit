#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
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
};

} // namespace Astrea::Explorer::Native::Backend

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::BackendError)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::BackendTransportError)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Backend::DirectoryEntry)
Q_DECLARE_METATYPE(QVector<Astrea::Explorer::Native::Backend::DirectoryEntry>)
