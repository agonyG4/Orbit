import QtQuick 2.15

// File operation workflow state is projected by FileOperationsController.
// QML retains only the compatibility shape used by existing presentation.
QtObject {
    id: ops

    property QtObject app
    readonly property var bridge: app && app.nativeAppState ? app.nativeAppState : null
    signal fileOperationChanged(var snapshot)
    signal archiveOperationChanged(var snapshot)

    function currentFileOperationSnapshot() {
        return bridge && bridge.currentFileOperationSnapshot
            ? bridge.currentFileOperationSnapshot()
            : {
                  running: fileOperationRunning,
                  progress: fileOperationProgress,
                  percent: fileOperationPercent,
                  fileName: fileOperationFileName,
                  status: fileOperationStatus,
                  error: fileOperationError,
                  destination: fileOperationDestination,
                  doneCount: fileOperationDoneCount,
                  totalCount: fileOperationTotalCount,
                  mode: fileOperationMode,
                  state: fileOperationState,
                  items: fileOperationItems
              }
    }

    function currentArchiveOperationSnapshot() {
        return bridge && bridge.currentArchiveOperationSnapshot
            ? bridge.currentArchiveOperationSnapshot()
            : {
                  running: archiveExtractionRunning,
                  progress: archiveExtractionProgress,
                  percent: archiveExtractionPercent,
                  fileName: archiveExtractionFileName,
                  status: archiveExtractionStatus,
                  error: archiveExtractionError,
                  destination: archiveExtractionDestination,
                  doneCount: archiveExtractionDoneCount,
                  totalCount: archiveExtractionTotalCount,
                  remainingText: archiveExtractionRemainingText
              }
    }

    property Connections bridgeConnections: Connections {
        target: ops.bridge
        function onFileOperationChanged(snapshot) {
            ops.fileOperationChanged(snapshot)
        }
        function onArchiveOperationChanged(snapshot) {
            ops.archiveOperationChanged(snapshot)
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
    property bool archivePasswordPromptVisible: bridge ? bridge.archivePasswordPromptVisible : false
    property string archivePassword: ""
    property string archivePasswordError: ""
    property bool archiveConflictVisible: false
    property string archiveConflictDestination: ""
    property string archiveConflictName: ""
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
    property bool appImageInstallRunning: false
    property bool wallpaperApplyRunning: false

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
    function submitArchivePassword(password) { if (bridge) bridge.submitArchivePassword(password) }
    function cancelArchivePassword() { if (bridge) bridge.cancelArchivePassword() }
    function submitArchiveConflict(policy) { if (bridge) bridge.submitArchiveConflict(policy) }
    function cancelArchiveConflict() { if (bridge) bridge.cancelArchiveConflict() }
    function startFolderCompression(path, format) { if (bridge) bridge.startFolderCompression(path, format) }
    function installAppImage(path) { if (bridge) bridge.installAppImage(path) }
    function setAsWallpaper(path) { if (bridge) bridge.setAsWallpaper(path) }
}
