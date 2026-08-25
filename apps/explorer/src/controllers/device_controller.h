#pragma once

#include <QSet>
#include <QStringList>
#include <QVector>

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Backend {

class DeviceController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString operationPath READ operationPath NOTIFY operationChanged)
    Q_PROPERTY(QString operationType READ operationType NOTIFY operationChanged)
    Q_PROPERTY(QString operationTargetMountPath READ operationTargetMountPath NOTIFY operationChanged)
    Q_PROPERTY(bool operationOpenAfterMount READ operationOpenAfterMount NOTIFY operationChanged)
    Q_PROPERTY(QString lastMountPath READ lastMountPath NOTIFY operationChanged)
    Q_PROPERTY(QString lastUnmountedMountPath READ lastUnmountedMountPath NOTIFY operationChanged)
    Q_PROPERTY(QString operationError READ operationError NOTIFY operationChanged)
    Q_PROPERTY(QString autoMountDeviceIdsJson READ autoMountDeviceIdsJson NOTIFY autoMountChanged)

public:
    explicit DeviceController(
        IRustBackendClient *client,
        QObject *parent = nullptr,
        QString autoMountDeviceIdsJson = QStringLiteral("[]"));

    const QVector<DeviceEntry> &devices() const;
    bool loading() const;
    QString error() const;
    QString operationPath() const;
    QString operationType() const;
    QString operationTargetMountPath() const;
    bool operationOpenAfterMount() const;
    QString lastMountPath() const;
    QString lastUnmountedMountPath() const;
    QString operationError() const;
    QString autoMountDeviceIdsJson() const;
    bool isAutoMount(const QString &deviceId) const;

    BackendRequestId refresh();
    BackendRequestId requestMount(
        const QString &devicePath,
        bool fromAutoMount,
        bool openAfterMount);
    BackendRequestId requestUnmount(
        const QString &devicePath,
        const QString &mountPath);
    BackendRequestId requestRemount(
        const QString &devicePath,
        const QString &mountPath,
        bool openAfterMount);

public slots:
    void setAutoMount(const QString &deviceId, bool enabled);

signals:
    void devicesChanged();
    void loadingChanged();
    void errorChanged();
    void operationChanged();
    void autoMountChanged();
    void operationFinished(
        const Astrea::Explorer::Native::Backend::DeviceOperationResult &result);

private slots:
    void handleDevicesReady(
        BackendRequestId requestId,
        const QVector<DeviceEntry> &devices);
    void handleDeviceOperationReady(
        BackendRequestId requestId,
        const DeviceOperationResult &result);
    void handleBackendFailure(const BackendError &error);

private:
    void setLoading(bool loading);
    void setError(const QString &error);
    BackendRequestId requestOperation(
        const QString &devicePath,
        const QString &operationType,
        const QString &targetMountPath,
        bool openAfterMount);
    void clearOperation();
    void loadAutoMountIds(const QString &json);
    void updateAutoMountJson();

    IRustBackendClient *m_client = nullptr;
    QVector<DeviceEntry> m_devices;
    BackendRequestId m_refreshRequest = 0;
    BackendRequestId m_operationRequest = 0;
    bool m_loading = false;
    QString m_error;
    QString m_operationPath;
    QString m_operationType;
    QString m_operationTargetMountPath;
    bool m_operationOpenAfterMount = false;
    QString m_lastMountPath;
    QString m_lastUnmountedMountPath;
    QString m_operationError;
    QSet<QString> m_autoMountIds;
    QString m_autoMountDeviceIdsJson {QStringLiteral("[]")};
};

} // namespace Astrea::Explorer::Native::Backend
