#include <QSignalSpy>
#include <QtTest>

#include "backend/fake_backend_client.h"
#include "controllers/device_controller.h"

using namespace Astrea::Explorer::Native::Backend;

class DeviceControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsTypedDevices();
    void persistsAutoMountIds();
    void completesMountAndUnmountOperations();
    void rejectsEmptyOperations();
};

DeviceEntry deviceEntry(const QString &path)
{
    DeviceEntry device;
    device.id = QStringLiteral("path:") + path;
    device.devicePath = path;
    device.title = QStringLiteral("USB");
    device.subtitle = QStringLiteral("32 GB · FAT32");
    device.icon = QStringLiteral("drive-removable-media");
    device.removable = true;
    device.canMount = true;
    return device;
}

void DeviceControllerTest::loadsTypedDevices()
{
    FakeRustBackendClient client;
    DeviceController controller(&client);
    QSignalSpy changedSpy(&controller, &DeviceController::devicesChanged);

    const BackendRequestId requestId = controller.refresh();
    QCOMPARE(client.deviceRequests().constLast(), QStringList({QStringLiteral("devices")}));

    client.completeDevices(requestId, {deviceEntry(QStringLiteral("/dev/sdb1"))});
    QTRY_COMPARE(changedSpy.count(), 1);
    QCOMPARE(controller.devices().size(), 1);
    QCOMPARE(controller.devices().constFirst().devicePath, QStringLiteral("/dev/sdb1"));
    QCOMPARE(controller.devices().constFirst().title, QStringLiteral("USB"));
    QCOMPARE(controller.devices().constFirst().subtitle, QStringLiteral("32 GB · FAT32"));
    QCOMPARE(controller.devices().constFirst().icon, QStringLiteral("drive-removable-media"));
    QCOMPARE(controller.loading(), false);
    QCOMPARE(controller.error(), QString());
}

void DeviceControllerTest::persistsAutoMountIds()
{
    FakeRustBackendClient client;
    DeviceController controller(&client, nullptr, QStringLiteral("[\"usb-1\"]"));

    QVERIFY(controller.isAutoMount(QStringLiteral("usb-1")));
    QVERIFY(!controller.isAutoMount(QStringLiteral("usb-2")));
    controller.setAutoMount(QStringLiteral("usb-2"), true);
    QVERIFY(controller.isAutoMount(QStringLiteral("usb-2")));
    controller.setAutoMount(QStringLiteral("usb-1"), false);
    QVERIFY(!controller.isAutoMount(QStringLiteral("usb-1")));
    QCOMPARE(controller.autoMountDeviceIdsJson(), QStringLiteral("[\"usb-2\"]"));
}

void DeviceControllerTest::completesMountAndUnmountOperations()
{
    FakeRustBackendClient client;
    DeviceController controller(&client);
    QSignalSpy operationSpy(&controller, &DeviceController::operationFinished);

    const BackendRequestId mountId = controller.requestMount(
        QStringLiteral("/dev/sdb1"),
        false,
        true);
    QCOMPARE(controller.operationType(), QStringLiteral("mount"));
    client.completeDeviceOperation(
        mountId,
        DeviceOperationResult {true, QStringLiteral("/run/media/user/USB"), QStringLiteral("mounted")});
    QTRY_COMPARE(operationSpy.count(), 1);
    QCOMPARE(controller.operationType(), QString());
    QCOMPARE(controller.lastMountPath(), QStringLiteral("/run/media/user/USB"));
    QCOMPARE(controller.operationError(), QString());

    const BackendRequestId unmountId = controller.requestUnmount(
        QStringLiteral("/dev/sdb1"),
        QStringLiteral("/run/media/user/USB"));
    client.completeDeviceOperation(
        unmountId,
        DeviceOperationResult {true, QString(), QStringLiteral("unmounted")});
    QTRY_COMPARE(operationSpy.count(), 2);
    QCOMPARE(controller.lastUnmountedMountPath(), QStringLiteral("/run/media/user/USB"));
}

void DeviceControllerTest::rejectsEmptyOperations()
{
    FakeRustBackendClient client;
    DeviceController controller(&client);

    QCOMPARE(controller.requestMount(QString(), false, false), BackendRequestId(0));
    QCOMPARE(controller.requestUnmount(QString(), QString()), BackendRequestId(0));
    QCOMPARE(controller.requestRemount(QString(), QString(), false), BackendRequestId(0));
}

QTEST_GUILESS_MAIN(DeviceControllerTest)

#include "tst_device_controller.moc"
