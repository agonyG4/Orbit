import QtQuick 2.15

// File operation workflow state is projected by the native AppStateFacade.
// QML retains the aggregate snapshot shape consumed by presentation.
QtObject {
    id: ops

    property QtObject app
    readonly property var bridge: app ? app.nativeAppState : null
    signal fileOperationChanged(var snapshot)
    signal archiveOperationChanged(var snapshot)

    function currentFileOperationSnapshot() {
        var source = bridge || ops
        return {
            running: source.fileOperationRunning,
            progress: source.fileOperationProgress,
            percent: source.fileOperationPercent,
            fileName: source.fileOperationFileName,
            status: source.fileOperationStatus,
            error: source.fileOperationError,
            destination: source.fileOperationDestination,
            doneCount: source.fileOperationDoneCount,
            totalCount: source.fileOperationTotalCount,
            mode: source.fileOperationMode,
            state: source.fileOperationState,
            items: source.fileOperationItems
        }
    }

    function currentArchiveOperationSnapshot() {
        var source = bridge || ops
        var state = source.archiveWorkflowState || "idle"
        return {
            running: source.archiveOperationKind !== "capabilities"
                && (source.archiveExtractionRunning || state === "waiting-password" || state === "waiting-conflict"),
            progress: source.archiveExtractionProgress,
            percent: source.archiveExtractionPercent,
            fileName: source.archiveExtractionFileName,
            status: source.archiveExtractionStatus,
            error: source.archiveExtractionError,
            destination: source.archiveExtractionDestination,
            doneCount: source.archiveExtractionDoneCount,
            totalCount: source.archiveExtractionTotalCount,
            remainingText: source.archiveExtractionRemainingText,
            operation: source.archiveOperationKind,
            state: state,
            phase: source.archivePhase,
            currentPath: source.archiveCurrentPath,
            currentName: source.archiveCurrentName,
            bytesDone: source.archiveBytesDone,
            bytesTotal: source.archiveBytesTotal
        }
    }

    property Connections bridgeConnections: Connections {
        target: ops.bridge
        function onFileOperationStateChanged() {
            ops.fileOperationChanged(ops.currentFileOperationSnapshot())
        }
        function onArchiveStateChanged() {
            ops.archiveOperationChanged(ops.currentArchiveOperationSnapshot())
        }
    }
    property var pendingDeleteTargets: []
    property var pendingRestoreTargets: []
    property var pendingPasteFiles: []
    property string pendingPasteMode: ""
    property string pendingPasteDestination: ""
    property string pendingPasteRename: bridge ? bridge.pendingPasteRename : ""
    property var clipboardFiles: bridge ? bridge.clipboardFiles : []
    property string clipboardMode: bridge ? bridge.clipboardMode : "copy"
    property bool pasteConflictVisible: bridge ? bridge.pasteConflictVisible : false
    property var pasteConflictItems: bridge ? bridge.pasteConflictItems : []
    property bool archiveExtractionRunning: bridge ? bridge.archiveExtractionRunning : false
    property real archiveExtractionProgress: bridge ? bridge.archiveExtractionProgress : 0
    property int archiveExtractionPercent: bridge ? bridge.archiveExtractionPercent : 0
    property string archiveExtractionFileName: bridge ? bridge.archiveExtractionFileName : ""
    property string archiveExtractionStatus: bridge ? bridge.archiveExtractionStatus : ""
    property string archiveExtractionError: bridge ? bridge.archiveExtractionError : ""
    property string archiveExtractionDestination: bridge ? bridge.archiveExtractionDestination : ""
    property int archiveExtractionDoneCount: bridge ? bridge.archiveExtractionDoneCount : 0
    property int archiveExtractionTotalCount: bridge ? bridge.archiveExtractionTotalCount : 0
    property string archiveExtractionRemainingText: bridge ? bridge.archiveExtractionRemainingText : ""
    property string archiveOperationKind: bridge ? bridge.archiveOperationKind : ""
    property string archiveWorkflowState: bridge ? bridge.archiveWorkflowState : "idle"
    property string archivePhase: bridge ? bridge.archivePhase : ""
    property string archiveCurrentPath: bridge ? bridge.archiveCurrentPath : ""
    property string archiveCurrentName: bridge ? bridge.archiveCurrentName : ""
    property real archiveBytesDone: bridge ? bridge.archiveBytesDone : -1
    property real archiveBytesTotal: bridge ? bridge.archiveBytesTotal : -1
    property var archiveCapabilities: bridge ? bridge.archiveCapabilities : []
    property bool archivePasswordPromptVisible: bridge ? bridge.archivePasswordPromptVisible : false
    property string archivePassword: ""
    property string archivePasswordError: bridge ? bridge.archivePasswordError : ""
    property bool archiveConflictVisible: bridge ? bridge.archiveConflictVisible : false
    property string archiveConflictDestination: bridge ? bridge.archiveConflictDestination : ""
    property string archiveConflictName: bridge ? bridge.archiveConflictName : ""
    property bool archiveWorkflowOccupied: bridge ? bridge.archiveWorkflowOccupied : false
    property bool fileOperationRunning: bridge ? bridge.fileOperationRunning : false
    property real fileOperationProgress: bridge ? bridge.fileOperationProgress : 0
    property int fileOperationPercent: bridge ? bridge.fileOperationPercent : 0
    property string fileOperationFileName: bridge ? bridge.fileOperationFileName : ""
    property string fileOperationStatus: bridge ? bridge.fileOperationStatus : ""
    property string fileOperationError: bridge ? bridge.fileOperationError : ""
    property string fileOperationDestination: bridge ? bridge.fileOperationDestination : ""
    property int fileOperationDoneCount: bridge ? bridge.fileOperationDoneCount : 0
    property int fileOperationTotalCount: bridge ? bridge.fileOperationTotalCount : 0
    property string fileOperationMode: bridge ? bridge.fileOperationMode : ""
    property string fileOperationState: bridge ? bridge.fileOperationState : ""
    property var fileOperationItems: bridge ? bridge.fileOperationItems : []
    property bool appImageInstallRunning: bridge ? bridge.appImageInstallRunning : false
    property bool wallpaperApplyRunning: bridge ? bridge.wallpaperApplyRunning : false

    function joinPath(directory, name) { return bridge ? bridge.joinPath(directory, name) : directory + "/" + name }
    function fileUrlForPath(path) { return bridge ? bridge.fileUrlForPath(path) : path }
    function selectedPathsInCurrentFolder() { return bridge ? bridge.selectedPathsInCurrentFolder() : [] }
    function selectedUriListInCurrentFolder() { return bridge ? bridge.selectedUriListInCurrentFolder() : "" }
    function isCutPending(name) { return bridge ? bridge.isCutPending(name) : false }
    function isCutPathPending(path) { return bridge ? bridge.isCutPathPending(path) : false }
    function copySelected() { if (bridge) bridge.copySelected() }
    function cutSelected() { if (bridge) bridge.cutSelected() }
    function dropFiles(urls, destination, mode) { if (bridge) bridge.dropFiles(urls, destination, mode || "copy") }
    function dropFilePaths(paths, destination, mode) { if (bridge) bridge.dropFilePaths(paths, destination, mode || "copy") }
    function pasteFiles() { return bridge ? bridge.pasteFiles() : 0 }
    function resolvePasteConflict(policy) { if (bridge) bridge.resolvePasteConflict(policy) }
    function renamePasteConflict(name) { if (bridge) bridge.renamePasteConflict(name) }
    function cancelPasteConflict() { if (bridge) bridge.cancelPasteConflict() }
    function deleteSelected() { if (bridge) bridge.deleteSelected() }
    function restoreSelected() { if (bridge) bridge.restoreSelected() }
    function emptyTrash() { if (bridge) bridge.emptyTrash() }
    function startArchiveExtraction(path, folder) { if (bridge) bridge.startArchiveExtraction(path, folder) }
    function startArchiveExtractionHere(path, destination) {
        if (bridge) bridge.startArchiveExtractionHere(path, destination)
    }
    function startArchiveExtractionTo(path, destination) {
        if (bridge) bridge.startArchiveExtractionTo(path, destination)
    }
    function submitArchivePassword(password) { if (bridge) bridge.submitArchivePassword(password) }
    function cancelArchivePassword() { if (bridge) bridge.cancelArchivePassword() }
    function submitArchiveConflict(policy) { if (bridge) bridge.submitArchiveConflict(policy) }
    function cancelArchiveConflict() { if (bridge) bridge.cancelArchiveConflict() }
    function cancelArchiveOperation() { if (bridge) bridge.cancelArchiveOperation() }
    function startArchiveCreation(sources, archiveName, format, profile) {
        if (bridge) bridge.startArchiveCreation(sources, archiveName, format, profile)
    }
    function canExtractArchive(path) { return bridge ? bridge.canExtractArchive(path) : false }
    function startFolderCompression(path, format) { if (bridge) bridge.startFolderCompression(path, format) }
    function installAppImage(path) { if (bridge) bridge.installAppImage(path) }
    function setAsWallpaper(path) { if (bridge) bridge.setAsWallpaper(path) }
}
