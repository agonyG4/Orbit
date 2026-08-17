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
        &BackendTransport::streamed,
        this,
        &RustBackendClient::handleStreamed,
        Qt::QueuedConnection);
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
            const int streamedProgressCount = pending->streamedProgressCount;
            m_pendingRequests.erase(pending);

            if (kind == RequestKind::Devices) {
                BackendError error;
                const QVector<DeviceEntry> devices = decodeDevices(requestId, payload, &error);
                if (!error.code.isEmpty()) {
                    emit failed(error);
                } else {
                    emit devicesReady(requestId, devices);
                }
                return;
            }

            if (kind == RequestKind::DeviceOperation) {
                BackendError error;
                const DeviceOperationResult result = decodeDeviceOperation(
                    requestId,
                    payload,
                    &error);
                if (!error.code.isEmpty()) {
                    emit failed(error);
                } else {
                    emit deviceOperationReady(requestId, result);
                }
                return;
            }

            if (kind == RequestKind::FileOperation) {
                BackendError error;
                QVector<FileOperationProgress> progresses;
                const FileOperationResult result = decodeFileOperation(
                    requestId,
                    payload,
                    &error,
                    &progresses);
                if (!error.code.isEmpty()) {
                    emit failed(error);
                } else {
                    for (int index = streamedProgressCount; index < progresses.size(); ++index) {
                        const FileOperationProgress &progress = progresses.at(index);
                        emit fileOperationProgress(requestId, progress);
                    }
                    emit fileOperationReady(requestId, result);
                }
                return;
            }

            if (kind == RequestKind::Utility) {
                BackendError error;
                const UtilityResult result = decodeUtility(requestId, payload, &error);
                if (!error.code.isEmpty()) {
                    emit failed(error);
                } else {
                    emit utilityReady(requestId, result);
                }
                return;
            }

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
        },
        Qt::QueuedConnection);
    connect(
        m_transport,
        &BackendTransport::failed,
        this,
        [this](
            BackendRequestId requestId,
            const BackendTransportError &transportError) {
            const auto pending = m_pendingRequests.find(requestId);
            if (pending == m_pendingRequests.end()) {
                return;
            }

            const RequestKind kind = pending->kind;
            const int streamedProgressCount = pending->streamedProgressCount;
            m_pendingRequests.erase(pending);

            if (kind == RequestKind::FileOperation && !transportError.stdoutData.isEmpty()) {
                BackendError decodeError;
                QVector<FileOperationProgress> progresses;
                const FileOperationResult result = decodeFileOperation(
                    requestId,
                    transportError.stdoutData,
                    &decodeError,
                    &progresses);
                if (decodeError.code.isEmpty()) {
                    for (int index = streamedProgressCount; index < progresses.size(); ++index) {
                        emit fileOperationProgress(requestId, progresses.at(index));
                    }
                    emit fileOperationReady(requestId, result);
                    return;
                }
            }

            BackendError error;
            error.code = transportError.code;
            error.message = transportError.message;
            error.requestId = requestId;
            emit failed(error);
        },
        Qt::QueuedConnection);
}

void RustBackendClient::handleStreamed(
    BackendRequestId requestId,
    const QByteArray &payload)
{
    auto pending = m_pendingRequests.find(requestId);
    if (pending == m_pendingRequests.end() || pending->kind != RequestKind::FileOperation) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    if (document.object().value(QStringLiteral("event")).toString()
        != QStringLiteral("progress")) {
        return;
    }

    BackendError error;
    FileOperationProgress progress;
    if (!decodeFileOperationProgress(requestId, document.object(), &progress, &error)) {
        m_pendingRequests.erase(pending);
        emit failed(error);
        return;
    }
    ++pending->streamedProgressCount;
    emit fileOperationProgress(requestId, progress);
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

BackendRequestId RustBackendClient::devices()
{
    const BackendRequestId requestId = m_transport->start({QStringLiteral("devices")});
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::Devices});
    return requestId;
}

BackendRequestId RustBackendClient::mount(const QString &devicePath)
{
    const BackendRequestId requestId = m_transport->start(
        {QStringLiteral("mount"), devicePath});
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::DeviceOperation});
    return requestId;
}

BackendRequestId RustBackendClient::unmount(const QString &devicePath)
{
    const BackendRequestId requestId = m_transport->start(
        {QStringLiteral("unmount"), devicePath});
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::DeviceOperation});
    return requestId;
}

BackendRequestId RustBackendClient::remount(const QString &devicePath)
{
    const BackendRequestId requestId = m_transport->start(
        {QStringLiteral("remount"), devicePath});
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::DeviceOperation});
    return requestId;
}

BackendRequestId RustBackendClient::fileOperation(const FileOperationRequest &request)
{
    const BackendRequestId requestId = m_transport->start(fileOperationArguments(request));
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::FileOperation});
    return requestId;
}

BackendRequestId RustBackendClient::utility(const UtilityRequest &request)
{
    const BackendRequestId requestId = m_transport->start(utilityArguments(request));
    m_pendingRequests.insert(requestId, PendingRequest {RequestKind::Utility});
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

QStringList RustBackendClient::fileOperationArguments(
    const FileOperationRequest &request) const
{
    QStringList arguments {
        QStringLiteral("file-op"),
        QStringLiteral("--json-events"),
        request.mode,
        request.destination,
        request.conflictPolicy,
    };
    if (!request.rename.isEmpty()) {
        arguments.append(QStringLiteral("--rename"));
        arguments.append(request.rename);
    }
    arguments.append(QStringLiteral("--progress"));
    arguments.append(request.progressMode);
    arguments.append(request.sources);
    return arguments;
}

QStringList RustBackendClient::utilityArguments(const UtilityRequest &request) const
{
    QStringList arguments {QStringLiteral("utility"), request.operation};
    arguments.append(request.arguments);
    return arguments;
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

QVector<DeviceEntry> RustBackendClient::decodeDevices(
    BackendRequestId requestId,
    const QByteArray &payload,
    BackendError *error) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                parseError.error == QJsonParseError::NoError
                    ? QStringLiteral("top-level JSON must be an array")
                    : QStringLiteral("JSON parse failed: %1").arg(parseError.errorString()));
        }
        return {};
    }

    QVector<DeviceEntry> devices;
    devices.reserve(document.array().size());
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = makeDecodeError(requestId, QStringLiteral("array entries must be objects"));
            }
            return {};
        }

        const QJsonObject object = value.toObject();
        auto requireString = [&](const QString &key, QString *target) -> bool {
            const QJsonValue field = object.value(key);
            if (!field.isString()) {
                if (error != nullptr) {
                    *error = makeDecodeError(
                        requestId,
                        jsonValueError(key, QStringLiteral("a string")));
                }
                return false;
            }
            *target = field.toString();
            return true;
        };
        auto requireBool = [&](const QString &key, bool *target) -> bool {
            const QJsonValue field = object.value(key);
            if (!field.isBool()) {
                if (error != nullptr) {
                    *error = makeDecodeError(
                        requestId,
                        jsonValueError(key, QStringLiteral("a boolean")));
                }
                return false;
            }
            *target = field.toBool();
            return true;
        };

        DeviceEntry device;
        if (!requireString(QStringLiteral("id"), &device.id)
            || !requireString(QStringLiteral("devicePath"), &device.devicePath)
            || !requireString(QStringLiteral("title"), &device.title)
            || !requireString(QStringLiteral("subtitle"), &device.subtitle)
            || !requireString(QStringLiteral("mountPath"), &device.mountPath)
            || !requireString(QStringLiteral("desiredMountPath"), &device.desiredMountPath)
            || !requireBool(QStringLiteral("mounted"), &device.mounted)
            || !requireBool(QStringLiteral("canMount"), &device.canMount)
            || !requireBool(QStringLiteral("canUnmount"), &device.canUnmount)
            || !requireBool(QStringLiteral("canRemount"), &device.canRemount)
            || !requireBool(QStringLiteral("removable"), &device.removable)
            || !requireString(QStringLiteral("icon"), &device.icon)) {
            return {};
        }
        devices.append(device);
    }
    return devices;
}

DeviceOperationResult RustBackendClient::decodeDeviceOperation(
    BackendRequestId requestId,
    const QByteArray &payload,
    BackendError *error) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                parseError.error == QJsonParseError::NoError
                    ? QStringLiteral("top-level JSON must be an object")
                    : QStringLiteral("JSON parse failed: %1").arg(parseError.errorString()));
        }
        return {};
    }

    const QJsonObject object = document.object();
    const QJsonValue okValue = object.value(QStringLiteral("ok"));
    const QJsonValue mountPathValue = object.value(QStringLiteral("mountPath"));
    const QJsonValue messageValue = object.value(QStringLiteral("message"));
    if (!okValue.isBool() || !mountPathValue.isString() || !messageValue.isString()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                QStringLiteral("device operation fields have incompatible types"));
        }
        return {};
    }

    DeviceOperationResult result;
    result.ok = okValue.toBool();
    result.mountPath = mountPathValue.toString();
    result.message = messageValue.toString();
    return result;
}

bool RustBackendClient::decodeFileOperationProgress(
    BackendRequestId requestId,
    const QJsonObject &object,
    FileOperationProgress *progress,
    BackendError *error) const
{
    const QJsonValue done = object.value(QStringLiteral("done"));
    const QJsonValue total = object.value(QStringLiteral("total"));
    const QJsonValue percent = object.value(QStringLiteral("percent"));
    const QJsonValue path = object.value(QStringLiteral("path"));
    const QJsonValue name = object.value(QStringLiteral("name"));
    if (!done.isDouble() || !total.isDouble() || !percent.isDouble()
        || !path.isString() || !name.isString()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                QStringLiteral("file operation progress fields have incompatible types"));
        }
        return false;
    }

    if (progress == nullptr) {
        return true;
    }
    progress->requestId = requestId;
    progress->mode = object.value(QStringLiteral("mode")).toString();
    progress->doneCount = done.toInt();
    progress->totalCount = total.toInt();
    progress->percent = percent.toInt();
    progress->path = path.toString();
    progress->fileName = name.toString();
    progress->doneBytes = object.value(QStringLiteral("bytesDone")).toInteger();
    progress->totalBytes = object.value(QStringLiteral("bytesTotal")).toInteger();
    return true;
}

FileOperationResult RustBackendClient::decodeFileOperation(
    BackendRequestId requestId,
    const QByteArray &payload,
    BackendError *error,
    QVector<FileOperationProgress> *progresses) const
{
    FileOperationResult result;
    result.requestId = requestId;
    bool terminal = false;
    const QList<QByteArray> lines = payload.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error != nullptr) {
                *error = makeDecodeError(
                    requestId,
                    parseError.error == QJsonParseError::NoError
                        ? QStringLiteral("file operation event must be a JSON object")
                        : QStringLiteral("JSON parse failed: %1").arg(parseError.errorString()));
            }
            return {};
        }

        const QJsonObject object = document.object();
        const QString event = object.value(QStringLiteral("event")).toString();
        if (event == QStringLiteral("progress")) {
            FileOperationProgress progress;
            if (!decodeFileOperationProgress(requestId, object, &progress, error)) {
                if (error != nullptr) {
                    return {};
                }
                return {};
            }
            if (progresses != nullptr) {
                progresses->append(progress);
            }
            continue;
        }

        if (event == QStringLiteral("item")) {
            FileOperationItemResult item;
            item.source = object.value(QStringLiteral("source")).toString();
            item.target = object.value(QStringLiteral("target")).toString();
            item.status = object.value(QStringLiteral("status")).toString();
            item.errorCode = object.value(QStringLiteral("errorCode")).toString();
            item.errorMessage = object.value(QStringLiteral("message")).toString();
            if (item.source.isEmpty() || item.target.isEmpty() || item.status.isEmpty()) {
                if (error != nullptr) {
                    *error = makeDecodeError(
                        requestId,
                        QStringLiteral("file operation item fields are incomplete"));
                }
                return {};
            }
            result.items.append(item);
            continue;
        }

        if (event == QStringLiteral("done")) {
            result.ok = true;
            result.mode = object.value(QStringLiteral("mode")).toString();
            result.destination = object.value(QStringLiteral("destination")).toString();
            result.doneCount = object.value(QStringLiteral("done")).toInt();
            result.totalCount = object.value(QStringLiteral("total")).toInt();
            result.percent = object.value(QStringLiteral("percent")).toInt();
            result.state = object.value(QStringLiteral("state")).toString();
            terminal = true;
            continue;
        }

        if (event == QStringLiteral("error")) {
            result.ok = false;
            result.mode = object.value(QStringLiteral("mode")).toString();
            result.errorCode = object.value(QStringLiteral("code")).toString();
            result.errorMessage = object.value(QStringLiteral("message")).toString();
            terminal = true;
            continue;
        }

        if (event != QStringLiteral("start")) {
            if (error != nullptr) {
                *error = makeDecodeError(
                    requestId,
                    QStringLiteral("unknown file operation event '%1'").arg(event));
            }
            return {};
        }
    }

    if (!terminal) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                QStringLiteral("file operation did not produce a terminal event"));
        }
        return {};
    }
    return result;
}

UtilityResult RustBackendClient::decodeUtility(
    BackendRequestId requestId,
    const QByteArray &payload,
    BackendError *error) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                parseError.error == QJsonParseError::NoError
                    ? QStringLiteral("utility result must be a JSON object")
                    : QStringLiteral("JSON parse failed: %1").arg(parseError.errorString()));
        }
        return {};
    }

    const QJsonObject object = document.object();
    const QJsonValue okValue = object.value(QStringLiteral("ok"));
    const QJsonValue operationValue = object.value(QStringLiteral("operation"));
    if (!okValue.isBool() || !operationValue.isString()) {
        if (error != nullptr) {
            *error = makeDecodeError(
                requestId,
                QStringLiteral("utility result has incompatible fields"));
        }
        return {};
    }

    UtilityResult result;
    result.requestId = requestId;
    result.operation = operationValue.toString();
    result.ok = okValue.toBool();
    result.data = object;
    result.errorCode = object.value(QStringLiteral("errorCode")).toString();
    result.errorMessage = object.value(QStringLiteral("error")).toString();
    return result;
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
    entry.trashItemId = object.value(QStringLiteral("trashItemId")).toString();
    entry.trashInfoPath = object.value(QStringLiteral("trashInfoPath")).toString();
    entry.trashLocationId = object.value(QStringLiteral("trashLocationId")).toString();
    entry.trashOriginalPath = object.value(QStringLiteral("trashOriginalPath")).toString();
    entry.trashDeletionDate = QDateTime::fromString(
        object.value(QStringLiteral("trashDeletionDate")).toString(), Qt::ISODate);
    entry.trashMountTopdir = object.value(QStringLiteral("trashMountTopdir")).toString();
    entry.trashAvailable = object.value(QStringLiteral("trashAvailable")).toBool(false);
    entry.trashOrphanState = object.value(QStringLiteral("trashOrphanState")).toString();
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
