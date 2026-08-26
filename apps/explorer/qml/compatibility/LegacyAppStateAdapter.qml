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
    readonly property var clipboardFiles: []
    readonly property string clipboardMode: "copy"
    readonly property bool fileOperationRunning: false
    readonly property real fileOperationProgress: 0
    readonly property int fileOperationPercent: 0
    readonly property string fileOperationFileName: ""
    readonly property string fileOperationStatus: ""
    readonly property string fileOperationError: ""
    readonly property string fileOperationDestination: ""
    readonly property int fileOperationDoneCount: 0
    readonly property int fileOperationTotalCount: 0
    readonly property string fileOperationMode: ""
    readonly property string fileOperationState: ""
    readonly property var fileOperationItems: []
    readonly property bool pasteConflictVisible: false
    readonly property var pasteConflictItems: []
    readonly property string pendingPasteRename: ""
    readonly property bool archiveExtractionRunning: false
    readonly property real archiveExtractionProgress: 0
    readonly property int archiveExtractionPercent: 0
    readonly property string archiveExtractionFileName: ""
    readonly property string archiveExtractionStatus: ""
    readonly property string archiveExtractionError: ""
    readonly property string archiveExtractionDestination: ""
    readonly property int archiveExtractionDoneCount: 0
    readonly property int archiveExtractionTotalCount: 0
    readonly property string archiveExtractionRemainingText: ""
    readonly property bool archivePasswordPromptVisible: false
    readonly property string archivePasswordError: ""
    readonly property bool archiveConflictVisible: false
    readonly property string archiveConflictDestination: ""
    readonly property string archiveConflictName: ""
    readonly property bool appImageInstallRunning: false
    readonly property bool wallpaperApplyRunning: false

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
