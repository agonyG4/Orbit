import QtQml 2.15

// The native executable is the only supported production runtime. This
// compatibility object intentionally contains no runtime or process access.
QtObject {
    id: root
    property QtObject app
    readonly property bool nativeFacade: false
    readonly property bool isPortalDialog: false
    readonly property string homePath: ""
    readonly property string runtimeRoot: ""
    readonly property string backendPath: ""
    readonly property string helperPath: ""
    readonly property string wallpaperManagerPath: ""
    readonly property string astreaLaunch: ""
    readonly property string windowsRun: ""
    readonly property string networkRootPath: ""
    readonly property string trashFilesPath: ""
    readonly property string trashInfoPath: ""
    readonly property string recentVirtualPath: "recent://"

    // Keep startup calls safe while the public AppState Loader resolves the
    // native adapter. The legacy object must remain inert and must never
    // recreate the removed Process-based implementation.
    function loadRecent() {}
    function recordRecentAccess(path, isDirectory, fileUrl) {}
    function navigateTo(path) { return 0 }
    function refreshCurrentFolder() { return 0 }
    function goBack() {}
    function goForward() {}
    function isCutPathPending(path) { return false }
    function refreshDevices() {}
    function ensureAutoMountDevices() {}
    function toggleDeviceAutoMount(deviceId, enabled) {}
    function requestMountDevice(path, fromAutoMount, openAfterMount) { return 0 }
    function requestUnmountDevice(path, mountPath) { return 0 }
    function requestRemountDevice(path, mountPath, openAfterMount) { return 0 }
    function connectToNetwork(address) { return 0 }
}
