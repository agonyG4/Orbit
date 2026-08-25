#include "controllers/device_controller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QtGlobal>

namespace Astrea::Explorer::Native::Backend {

DeviceController::DeviceController(
    IRustBackendClient *client,
    QObject *parent,
    QString autoMountDeviceIdsJson)
    : QObject(parent)
    , m_client(client)
{
    Q_ASSERT(m_client != nullptr);
    qRegisterMetaType<QVector<DeviceEntry>>();
    qRegisterMetaType<DeviceOperationResult>();
    loadAutoMountIds(autoMountDeviceIdsJson);

    connect(
        m_client,
        &IRustBackendClient::devicesReady,
        this,
        &DeviceController::handleDevicesReady,
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::deviceOperationReady,
        this,
        &DeviceController::handleDeviceOperationReady,
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::failed,
        this,
        &DeviceController::handleBackendFailure,
        Qt::QueuedConnection);
}

const QVector<DeviceEntry> &DeviceController::devices() const
{
    return m_devices;
}

bool DeviceController::loading() const
{
    return m_loading;
}

QString DeviceController::error() const
{
    return m_error;
}

QString DeviceController::operationPath() const
{
    return m_operationPath;
}

QString DeviceController::operationType() const
{
    return m_operationType;
}

QString DeviceController::operationTargetMountPath() const
{
    return m_operationTargetMountPath;
}

bool DeviceController::operationOpenAfterMount() const
{
    return m_operationOpenAfterMount;
}

QString DeviceController::lastMountPath() const
{
    return m_lastMountPath;
}

QString DeviceController::lastUnmountedMountPath() const
{
    return m_lastUnmountedMountPath;
}

QString DeviceController::operationError() const
{
    return m_operationError;
}

QString DeviceController::autoMountDeviceIdsJson() const
{
    return m_autoMountDeviceIdsJson;
}

bool DeviceController::isAutoMount(const QString &deviceId) const
{
    return m_autoMountIds.contains(deviceId);
}

BackendRequestId DeviceController::refresh()
{
    if (m_refreshRequest != 0 || m_operationRequest != 0) {
        return 0;
    }
    setError(QString());
    setLoading(true);
    m_refreshRequest = m_client->devices();
    return m_refreshRequest;
}

BackendRequestId DeviceController::requestMount(
    const QString &devicePath,
    bool fromAutoMount,
    bool openAfterMount)
{
    return requestOperation(
        devicePath,
        fromAutoMount ? QStringLiteral("mount-auto") : QStringLiteral("mount"),
        QString(),
        openAfterMount);
}

BackendRequestId DeviceController::requestUnmount(
    const QString &devicePath,
    const QString &mountPath)
{
    return requestOperation(
        devicePath,
        QStringLiteral("unmount"),
        mountPath,
        false);
}

BackendRequestId DeviceController::requestRemount(
    const QString &devicePath,
    const QString &mountPath,
    bool openAfterMount)
{
    return requestOperation(
        devicePath,
        QStringLiteral("remount"),
        mountPath,
        openAfterMount);
}

void DeviceController::setAutoMount(const QString &deviceId, bool enabled)
{
    if (deviceId.isEmpty()) {
        return;
    }
    const bool changed = enabled ? !m_autoMountIds.contains(deviceId)
                                 : m_autoMountIds.remove(deviceId) > 0;
    if (!changed) {
        return;
    }
    if (enabled) {
        m_autoMountIds.insert(deviceId);
    }
    updateAutoMountJson();
    emit autoMountChanged();
}

void DeviceController::setAutoMountDeviceIdsJson(const QString &json)
{
    const QString previous = m_autoMountDeviceIdsJson;
    loadAutoMountIds(json);
    if (m_autoMountDeviceIdsJson != previous) {
        emit autoMountChanged();
    }
}

void DeviceController::handleDevicesReady(
    BackendRequestId requestId,
    const QVector<DeviceEntry> &devices)
{
    if (requestId == 0 || requestId != m_refreshRequest) {
        return;
    }
    m_refreshRequest = 0;
    m_devices = devices;
    setError(QString());
    setLoading(false);
    emit devicesChanged();
}

void DeviceController::handleDeviceOperationReady(
    BackendRequestId requestId,
    const DeviceOperationResult &result)
{
    if (requestId == 0 || requestId != m_operationRequest) {
        return;
    }
    m_operationRequest = 0;
    m_operationError = result.ok ? QString() : result.message;
    if (result.ok && !result.mountPath.isEmpty()) {
        m_lastMountPath = result.mountPath;
    }
    if (result.ok && m_operationType == QStringLiteral("unmount")) {
        m_lastUnmountedMountPath = m_operationTargetMountPath;
    }
    clearOperation();
    emit operationFinished(result);
}

void DeviceController::handleBackendFailure(const BackendError &error)
{
    if (error.requestId == m_refreshRequest) {
        m_refreshRequest = 0;
        setLoading(false);
        setError(error.message);
        return;
    }
    if (error.requestId != m_operationRequest) {
        return;
    }
    m_operationRequest = 0;
    m_operationError = error.message;
    clearOperation();
    emit operationFinished(DeviceOperationResult {false, {}, error.message});
}

void DeviceController::setLoading(bool loadingValue)
{
    if (m_loading == loadingValue) {
        return;
    }
    m_loading = loadingValue;
    emit loadingChanged();
}

void DeviceController::setError(const QString &errorValue)
{
    if (m_error == errorValue) {
        return;
    }
    m_error = errorValue;
    emit errorChanged();
}

BackendRequestId DeviceController::requestOperation(
    const QString &devicePath,
    const QString &operationTypeValue,
    const QString &targetMountPath,
    bool openAfterMount)
{
    if (devicePath.isEmpty() || m_operationRequest != 0 || m_refreshRequest != 0) {
        return 0;
    }
    m_operationPath = devicePath;
    m_operationType = operationTypeValue;
    m_operationTargetMountPath = targetMountPath;
    m_operationOpenAfterMount = openAfterMount;
    m_operationError.clear();
    emit operationChanged();

    if (operationTypeValue == QStringLiteral("unmount")) {
        m_operationRequest = m_client->unmount(devicePath);
    } else if (operationTypeValue == QStringLiteral("remount")) {
        m_operationRequest = m_client->remount(devicePath);
    } else {
        m_operationRequest = m_client->mount(devicePath);
    }
    return m_operationRequest;
}

void DeviceController::clearOperation()
{
    m_operationPath.clear();
    m_operationType.clear();
    m_operationTargetMountPath.clear();
    m_operationOpenAfterMount = false;
    emit operationChanged();
}

void DeviceController::loadAutoMountIds(const QString &json)
{
    m_autoMountIds.clear();
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        m_autoMountDeviceIdsJson = QStringLiteral("[]");
        return;
    }

    for (const QJsonValue &value : document.array()) {
        if (value.isString() && !value.toString().isEmpty()) {
            m_autoMountIds.insert(value.toString());
        }
    }
    updateAutoMountJson();
}

void DeviceController::updateAutoMountJson()
{
    QStringList ids = m_autoMountIds.values();
    ids.sort(Qt::CaseSensitive);
    QJsonArray values;
    for (const QString &id : ids) {
        values.append(id);
    }
    m_autoMountDeviceIdsJson = QString::fromUtf8(
        QJsonDocument(values).toJson(QJsonDocument::Compact));
}

} // namespace Astrea::Explorer::Native::Backend
