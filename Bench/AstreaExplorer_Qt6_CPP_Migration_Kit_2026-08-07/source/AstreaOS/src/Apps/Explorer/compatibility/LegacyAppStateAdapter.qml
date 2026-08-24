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
    readonly property string effectiveIconTheme: ""
    readonly property int iconThemeRevision: 0
    readonly property var sidebarFavoritesModel: null

    signal fileOperationChanged(var snapshot)
    signal archiveOperationChanged(var snapshot)

    function currentFileOperationSnapshot() {
        return {
            running: false, progress: 0, percent: 0, fileName: "", status: "",
            error: "", destination: "", doneCount: 0, totalCount: 0,
            mode: "", state: "", items: []
        }
    }

    function currentArchiveOperationSnapshot() {
        return {
            running: false, progress: 0, percent: 0, fileName: "", status: "",
            error: "", destination: "", doneCount: 0, totalCount: 0,
            remainingText: ""
        }
    }

    function themedIconSource(iconName, size, themeName) { return "" }
    function sidebarIconSource(iconName, size) { return "" }
    function fileIconName(path, isDirectory, isExecutable) {
        return isDirectory ? "inode-directory" : "application-x-generic"
    }
    function fileIconSource(path, isDirectory, isExecutable, size, semanticIconName) { return "" }
    function prepareSelectionForDrag(name, index) {}

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
    function beginSidebarFavoriteDrag(path) { return false }
    function previewSidebarFavoriteMove(path, index) { return false }
    function commitSidebarFavoriteDrag() { return false }
    function cancelSidebarFavoriteDrag() {}
}
