import QtQuick 2.15

// Device enumeration and block-device operations are native. Network mount
// probing is also routed through the typed native filesystem API.
QtObject {
    id: deviceNet

    property QtObject app
    property var deviceModel: app ? app.nativeAppState.deviceModel : []
    property var autoMountDeviceIds: []
    property string autoMountDeviceIdsJson: app ? app.nativeAppState.autoMountDeviceIdsJson : "[]"
    property string deviceOperationPath: app ? app.nativeAppState.deviceOperationPath : ""
    property string deviceOperationType: app ? app.nativeAppState.deviceOperationType : ""
    property string deviceOperationTargetMountPath: app ? app.nativeAppState.deviceOperationTargetMountPath : ""
    property bool deviceOperationOpenAfterMount: app ? app.nativeAppState.deviceOperationOpenAfterMount : false
    property string lastUnmountedMountPath: app ? app.nativeAppState.lastUnmountedMountPath : ""
    property string deviceError: app ? app.nativeAppState.deviceError : ""
    property bool networkConnectVisible: false
    property string networkAddress: ""
    property string networkError: ""
    property bool networkConnecting: false
    property int networkProbeRequestId: 0
    property int networkMountRequestId: 0

    function showNetworkConnectDialog() {
        networkError = ""
        if (!networkAddress) networkAddress = "smb://"
        networkConnectVisible = true
    }
    function hideNetworkConnectDialog() {
        if (!networkConnecting) {
            networkConnectVisible = false
            networkError = ""
        }
    }
    function normalizedNetworkAddress() {
        var value = (networkAddress || "").trim()
        if (!value) return ""
        return value.indexOf("://") === -1 ? "smb://" + value : value
    }
    function openNetworkBrowser() {
        networkProbeRequestId = app ? app.requestNetworkMountProbe(app.networkRootPath) : 0
    }
    function connectToNetwork() {
        var address = normalizedNetworkAddress()
        if (!address || networkConnecting || !app || !app.nativeAppState) return
        networkError = ""
        networkConnecting = true
        networkMountRequestId = app.nativeAppState.connectToNetwork(address)
    }
    function loadSavedAutoMounts() {
        try {
            var parsed = JSON.parse(autoMountDeviceIdsJson || "[]")
            autoMountDeviceIds = Array.isArray(parsed) ? parsed : []
        } catch (error) {
            autoMountDeviceIds = []
        }
    }
    function saveAutoMounts() {}
    function isDeviceAutoMount(deviceId) {
        return autoMountDeviceIds.indexOf(deviceId) !== -1
    }
    function setDeviceAutoMount(deviceId, enabled) {
        if (app && app.nativeAppState) app.nativeAppState.toggleDeviceAutoMount(deviceId, enabled)
    }
    function toggleDeviceAutoMount(deviceId) {
        if (app && app.nativeAppState)
            app.nativeAppState.toggleDeviceAutoMount(deviceId, !isDeviceAutoMount(deviceId))
    }
    function syncDeviceAutoMountFlags() {}
    function replaceDeviceModel(items) {}
    function refreshDevices() { if (app && app.nativeAppState) app.nativeAppState.refreshDevices() }
    function scheduleStartupDeviceRefresh() { refreshDevices() }
    function ensureAutoMountDevices() { if (app && app.nativeAppState) app.nativeAppState.ensureAutoMountDevices() }
    function requestMountDevice(path, fromAutoMount, openAfterMount) {
        return app && app.nativeAppState ? app.nativeAppState.requestMountDevice(path, fromAutoMount, openAfterMount) : 0
    }
    function requestUnmountDevice(path, mountPath) {
        return app && app.nativeAppState ? app.nativeAppState.requestUnmountDevice(path, mountPath) : 0
    }
    function requestRemountDevice(path, mountPath, openAfterMount) {
        return app && app.nativeAppState ? app.nativeAppState.requestRemountDevice(path, mountPath, openAfterMount) : 0
    }
    function syncDeviceBusyFlags() {}

    property Connections filesystemConnections: Connections {
        target: deviceNet.app
        function onFilesystemActionFinished(requestId, operation, ok, data, error) {
            if (operation === "network-mount-probe" && requestId === deviceNet.networkProbeRequestId) {
                if (ok && Number(data.mountCount || 0) > 0)
                    deviceNet.app.navigateTo(deviceNet.app.networkRootPath)
                else
                    deviceNet.showNetworkConnectDialog()
            } else if (operation === "network-mount" && requestId === deviceNet.networkMountRequestId) {
                deviceNet.networkConnecting = false
                if (ok) {
                    deviceNet.networkConnectVisible = false
                    deviceNet.app.navigateTo(deviceNet.app.networkRootPath)
                } else {
                    deviceNet.networkError = error || "Não foi possível conectar ao servidor."
                }
            }
        }
    }
}
