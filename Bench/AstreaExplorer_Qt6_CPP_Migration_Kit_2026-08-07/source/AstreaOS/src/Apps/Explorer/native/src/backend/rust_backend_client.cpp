#include "backend/rust_backend_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimeZone>

namespace Astrea::Explorer::Native::Backend {

namespace {

QString boolArg(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

QString jsonValueError(const QString &fieldName, const QString &expectedType)
{
    return QStringLiteral("field '%1' must be %2").arg(fieldName, expectedType);
}

} // namespace

IRustBackendClient::IRustBackendClient(QObject *parent)
    : QObject(parent)
{
}

IRustBackendClient::~IRustBackendClient() = default;

RustBackendClient::RustBackendClient(BackendTransport *transport, QObject *parent)
    : IRustBackendClient(parent)
    , m_transport(transport)
{
    Q_ASSERT(m_transport != nullptr);

    connect(
        m_transport,
        &BackendTransport::completed,
        this,
        [this](BackendRequestId requestId, const QByteArray &payload) {
            const auto pending = m_pendingRequests.find(requestId);
            if (pending == m_pendingRequests.end()) {
                return;
            }

            const RequestKind kind = pending->kind;
            m_pendingRequests.erase(pending);

            BackendError error;
            const QVector<DirectoryEntry> entries =
                decodeEntries(requestId, payload, &error);
            if (!error.code.isEmpty()) {
                emit failed(error);
                return;
            }

            if (kind == RequestKind::List) {
                emit listReady(requestId, entries);
            } else {
                emit searchReady(requestId, entries);
            }
        });
    connect(
        m_transport,
        &BackendTransport::failed,
        this,
        [this](
            BackendRequestId requestId,
            const BackendTransportError &transportError) {
            if (!m_pendingRequests.remove(requestId)) {
                return;
            }

            BackendError error;
            error.code = transportError.code;
            error.message = transportError.message;
            error.requestId = requestId;
            emit failed(error);
        });
}

BackendRequestId RustBackendClient::list(const ListRequest &request)
{
    const BackendRequestId requestId = m_transport->start(listArguments(request));
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::List});
    return requestId;
}

BackendRequestId RustBackendClient::search(const SearchRequest &request)
{
    const BackendRequestId requestId = m_transport->start(searchArguments(request));
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::Search});
    return requestId;
}

void RustBackendClient::cancel(BackendRequestId requestId)
{
    if (!m_pendingRequests.contains(requestId)) {
        return;
    }
    m_transport->cancel(requestId);
}

QStringList RustBackendClient::listArguments(const ListRequest &request) const
{
    QStringList arguments {
        QStringLiteral("list"),
        request.path,
        boolArg(request.showHidden),
        request.sortField,
        boolArg(request.sortAscending),
        boolArg(request.foldersFirst),
    };

    if (!request.previews) {
        arguments.append(QStringLiteral("--preview-mode"));
        arguments.append(QStringLiteral("none"));
    }

    return arguments;
}

QStringList RustBackendClient::searchArguments(const SearchRequest &request) const
{
    return {
        QStringLiteral("search"),
        request.rootPath,
        request.query,
        boolArg(request.showHidden),
        request.sortField,
        boolArg(request.sortAscending),
        boolArg(request.foldersFirst),
    };
}

QVector<DirectoryEntry> RustBackendClient::decodeEntries(
    BackendRequestId requestId,
    const QByteArray &payload,
    BackendError *error) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                QStringLiteral("JSON parse failed: %1").arg(parseError.errorString()));
        }
        return {};
    }

    if (!document.isArray()) {
        if (error != nullptr) {
            *error = makeDecodeError(requestId, QStringLiteral("top-level JSON must be an array"));
        }
        return {};
    }

    QVector<DirectoryEntry> entries;
    const QJsonArray array = document.array();
    entries.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = makeDecodeError(requestId, QStringLiteral("array entries must be objects"));
            }
            return {};
        }

        BackendError decodeError;
        decodeError.requestId = requestId;
        const DirectoryEntry entry = decodeEntry(value.toObject(), &decodeError);
        if (!decodeError.code.isEmpty()) {
            if (error != nullptr) {
                *error = decodeError;
            }
            return {};
        }
        entries.append(entry);
    }

    return entries;
}

DirectoryEntry RustBackendClient::decodeEntry(
    const QJsonObject &object,
    BackendError *error) const
{
    auto fail = [error](const QString &message) {
        if (error != nullptr) {
            error->code = QStringLiteral("decode_error");
            error->message = message;
        }
    };

    auto requireString = [&](const QString &key, QString *target) -> bool {
        const QJsonValue value = object.value(key);
        if (!value.isString()) {
            fail(jsonValueError(key, QStringLiteral("a string")));
            return false;
        }
        *target = value.toString();
        return true;
    };
    auto requireBool = [&](const QString &key, bool *target) -> bool {
        const QJsonValue value = object.value(key);
        if (!value.isBool()) {
            fail(jsonValueError(key, QStringLiteral("a boolean")));
            return false;
        }
        *target = value.toBool();
        return true;
    };
    auto requireInteger = [&](const QString &key, qint64 *target) -> bool {
        const QJsonValue value = object.value(key);
        if (!value.isDouble()) {
            fail(jsonValueError(key, QStringLiteral("an integer-like number")));
            return false;
        }
        *target = value.toInteger();
        return true;
    };

    DirectoryEntry entry;
    QString fileUrl;
    QString previewUrl;
    qint64 modifiedMs = 0;

    if (!requireString(QStringLiteral("fileName"), &entry.fileName)
        || !requireString(QStringLiteral("filePath"), &entry.filePath)
        || !requireString(QStringLiteral("fileUrl"), &fileUrl)
        || !requireBool(QStringLiteral("fileIsDir"), &entry.fileIsDir)
        || !requireBool(QStringLiteral("fileExecutable"), &entry.fileExecutable)
        || !requireBool(QStringLiteral("fileHidden"), &entry.fileHidden)
        || !requireInteger(QStringLiteral("fileSize"), &entry.fileSize)
        || !requireInteger(QStringLiteral("fileModified"), &modifiedMs)
        || !requireString(QStringLiteral("fileKind"), &entry.fileKind)
        || !requireString(QStringLiteral("filePreviewUrl"), &previewUrl)
        || !requireBool(QStringLiteral("fileRemote"), &entry.fileRemote)
        || !requireBool(QStringLiteral("fileMetadataLimited"), &entry.fileMetadataLimited)
        || !requireString(QStringLiteral("fileFilesystem"), &entry.fileFilesystem)) {
        return {};
    }

    entry.fileUrl = QUrl(fileUrl);
    entry.filePreviewUrl = QUrl(previewUrl);
    entry.fileModified = QDateTime::fromMSecsSinceEpoch(modifiedMs, QTimeZone::UTC);
    return entry;
}

BackendError RustBackendClient::makeDecodeError(
    BackendRequestId requestId,
    const QString &message) const
{
    BackendError error;
    error.code = QStringLiteral("decode_error");
    error.message = message;
    error.requestId = requestId;
    return error;
}

} // namespace Astrea::Explorer::Native::Backend
