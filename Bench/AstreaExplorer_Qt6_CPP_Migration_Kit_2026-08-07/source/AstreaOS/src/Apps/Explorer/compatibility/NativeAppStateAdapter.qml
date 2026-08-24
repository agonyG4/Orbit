import QtQml 2.15
import Astrea.Explorer.Native 1.0

QtObject {
    id: root

    readonly property QtObject facade: NativeAppState
    readonly property bool isPortalDialog: facade.isPortalDialog
    readonly property string homePath: facade.homePath
    readonly property string runtimeRoot: facade.runtimeRoot
    readonly property string backendPath: facade.backendPath
    readonly property string helperPath: facade.helperPath
    readonly property string wallpaperManagerPath: facade.wallpaperManagerPath
    readonly property string astreaLaunch: facade.astreaLaunch
    readonly property string windowsRun: facade.windowsRun
    readonly property string networkRootPath: facade.networkRootPath
    readonly property string trashFilesPath: facade.trashFilesPath
    readonly property string trashInfoPath: facade.trashInfoPath
    readonly property string trashVirtualPath: facade.trashVirtualPath
    readonly property string recentVirtualPath: facade.recentVirtualPath
    readonly property string effectiveIconTheme: facade.effectiveIconTheme
    readonly property var iconThemeRevision: facade.iconThemeRevision

    property string currentPath: facade.currentPath
    property var history: facade.history
    property int historyIdx: facade.historyIdx
    property var tabs: facade.tabs
    property int activeTabIndex: facade.activeTabIndex
    property var breadcrumbParts: facade.breadcrumbParts
    property bool loadingDir: facade.loadingDir
    property string loadError: facade.loadError
    property bool remoteDirectoryActive: facade.remoteDirectoryActive
    property bool searchActive: facade.searchActive
    property bool searchVisible: facade.searchVisible
    property string searchQuery: facade.searchQuery
    property var fileModel: facade.fileModel
    property int fileModelRevision: facade.fileModelRevision
    property bool fileModelFilling: facade.fileModelFilling
    property string selectedFile: facade.selectedFile
    property var selectedFiles: facade.selectedFiles
    property var selectedPaths: facade.selectedPaths
    property int lastSelectedIndex: facade.lastSelectedIndex
    property var clipboardFiles: facade.clipboardFiles
    property string clipboardMode: facade.clipboardMode
    property bool fileOperationRunning: facade.fileOperationRunning
    property real fileOperationProgress: facade.fileOperationProgress
    property int fileOperationPercent: facade.fileOperationPercent
    property string fileOperationFileName: facade.fileOperationFileName
    property string fileOperationStatus: facade.fileOperationStatus
    property string fileOperationError: facade.fileOperationError
    property string fileOperationDestination: facade.fileOperationDestination
    property int fileOperationDoneCount: facade.fileOperationDoneCount
    property int fileOperationTotalCount: facade.fileOperationTotalCount
    property string fileOperationMode: facade.fileOperationMode
    property string fileOperationState: facade.fileOperationState
    property var fileOperationItems: facade.fileOperationItems
    property bool pasteConflictVisible: facade.pasteConflictVisible
    property var pasteConflictItems: facade.pasteConflictItems
    property string pendingPasteRename: facade.pendingPasteRename
    property bool showPreview: facade.showPreview
    property bool previewsEnabled: facade.previewsEnabled
    property string viewMode: facade.viewMode
    property string sortField: facade.sortField
    property bool sortAsc: facade.sortAsc
    property bool showHidden: facade.showHidden
    property bool foldersFirst: facade.foldersFirst
    property bool groupingEnabled: facade.groupingEnabled
    property real zoomLevel: facade.zoomLevel
    property string autoMountDeviceIdsJson: facade.autoMountDeviceIdsJson
    property string sidebarFavoritesJson: facade.sidebarFavoritesJson
    property string sidebarHiddenDefaultFavoritesJson: facade.sidebarHiddenDefaultFavoritesJson
    property var sidebarFavorites: facade.sidebarFavorites
    property var sidebarHiddenDefaultFavorites: facade.sidebarHiddenDefaultFavorites
    property var sidebarFavoritesModel: facade.sidebarFavoritesModel
    property int sidebarFavoritesRevision: facade.sidebarFavoritesRevision
    property var defaultSidebarFavoritePaths: facade.defaultSidebarFavoritePaths
    property bool inTrashView: facade.inTrashView
    property bool dialogActive: facade.dialogActive
    property string dialogMode: facade.dialogMode
    property var dialogFilePatterns: facade.dialogFilePatterns
    property var deviceModel: facade.deviceModel
    property string deviceError: facade.deviceError
    property string deviceOperationPath: facade.deviceOperationPath
    property string deviceOperationType: facade.deviceOperationType
    property string deviceOperationTargetMountPath: facade.deviceOperationTargetMountPath
    property bool deviceOperationOpenAfterMount: facade.deviceOperationOpenAfterMount
    property string lastUnmountedMountPath: facade.lastUnmountedMountPath
    property bool archiveExtractionRunning: facade.archiveExtractionRunning
    property real archiveExtractionProgress: facade.archiveExtractionProgress
    property int archiveExtractionPercent: facade.archiveExtractionPercent
    property string archiveExtractionFileName: facade.archiveExtractionFileName
    property string archiveExtractionStatus: facade.archiveExtractionStatus
    property string archiveExtractionError: facade.archiveExtractionError
    property string archiveExtractionDestination: facade.archiveExtractionDestination
    property int archiveExtractionDoneCount: facade.archiveExtractionDoneCount
    property int archiveExtractionTotalCount: facade.archiveExtractionTotalCount
    property string archiveExtractionRemainingText: facade.archiveExtractionRemainingText
    property bool archivePasswordPromptVisible: facade.archivePasswordPromptVisible
    property string archivePassword: ""
    property string archivePasswordError: facade.archivePasswordError
    property bool archiveConflictVisible: facade.archiveConflictVisible
    property string archiveConflictDestination: facade.archiveConflictDestination
    property string archiveConflictName: facade.archiveConflictName
    property bool appImageInstallRunning: facade.appImageInstallRunning
    property bool wallpaperApplyRunning: facade.wallpaperApplyRunning

    readonly property bool nativeFacade: true

    signal searchStateChanged()
    signal dialogStateChanged()
    signal clipboardStateChanged()
    signal filesystemActionFinished(int requestId, string operation, bool ok, var data, string error)
    signal openWithReady(string path, var applications)
    signal fileOperationChanged(var snapshot)
    signal archiveOperationChanged(var snapshot)

    function currentFileOperationSnapshot() {
        return {
            running: facade.fileOperationRunning,
            progress: facade.fileOperationProgress,
            percent: facade.fileOperationPercent,
            fileName: facade.fileOperationFileName,
            status: facade.fileOperationStatus,
            error: facade.fileOperationError,
            destination: facade.fileOperationDestination,
            doneCount: facade.fileOperationDoneCount,
            totalCount: facade.fileOperationTotalCount,
            mode: facade.fileOperationMode,
            state: facade.fileOperationState,
            items: facade.fileOperationItems
        }
    }

    function currentArchiveOperationSnapshot() {
        return {
            running: facade.archiveExtractionRunning,
            progress: facade.archiveExtractionProgress,
            percent: facade.archiveExtractionPercent,
            fileName: facade.archiveExtractionFileName,
            status: facade.archiveExtractionStatus,
            error: facade.archiveExtractionError,
            destination: facade.archiveExtractionDestination,
            doneCount: facade.archiveExtractionDoneCount,
            totalCount: facade.archiveExtractionTotalCount,
            remainingText: facade.archiveExtractionRemainingText
        }
    }

    property Connections facadeConnections: Connections {
        target: root.facade
        function onSearchStateChanged() { root.searchStateChanged() }
        function onDialogStateChanged() { root.dialogStateChanged() }
        function onClipboardStateChanged() { root.clipboardStateChanged() }
        function onFilesystemActionFinished(requestId, operation, ok, data, error) {
            root.filesystemActionFinished(requestId, operation, ok, data, error)
        }
        function onOpenWithReady(path, applications) {
            root.openWithReady(path, applications)
        }
        function onFileOperationStateChanged() {
            root.fileOperationChanged(root.currentFileOperationSnapshot())
        }
        function onArchiveStateChanged() {
            root.archiveOperationChanged(root.currentArchiveOperationSnapshot())
        }
    }

    onCurrentPathChanged: {
        if (facade.currentPath !== currentPath)
            facade.navigateTo(currentPath)
    }
    onSearchQueryChanged: {
        if (facade.searchQuery !== searchQuery)
            facade.searchQuery = searchQuery
    }
    onSelectedFileChanged: {
        if (facade.selectedFile !== selectedFile)
            facade.selectedFile = selectedFile
    }
    onShowPreviewChanged: {
        if (facade.showPreview !== showPreview)
            facade.showPreview = showPreview
    }
    onViewModeChanged: {
        if (facade.viewMode !== viewMode)
            facade.viewMode = viewMode
    }
    onSortFieldChanged: {
        if (facade.sortField !== sortField)
            facade.sortField = sortField
    }
    onSortAscChanged: {
        if (facade.sortAsc !== sortAsc)
            facade.sortAsc = sortAsc
    }
    onShowHiddenChanged: {
        if (facade.showHidden !== showHidden)
            facade.showHidden = showHidden
    }
    onFoldersFirstChanged: {
        if (facade.foldersFirst !== foldersFirst)
            facade.foldersFirst = foldersFirst
    }
    onGroupingEnabledChanged: {
        if (facade.groupingEnabled !== groupingEnabled)
            facade.groupingEnabled = groupingEnabled
    }
    onZoomLevelChanged: {
        if (facade.zoomLevel !== zoomLevel)
            facade.zoomLevel = zoomLevel
    }
    onSidebarFavoritesJsonChanged: {
        if (facade.sidebarFavoritesJson !== sidebarFavoritesJson)
            facade.sidebarFavoritesJson = sidebarFavoritesJson
    }
    onSidebarHiddenDefaultFavoritesJsonChanged: {
        if (facade.sidebarHiddenDefaultFavoritesJson !== sidebarHiddenDefaultFavoritesJson)
            facade.sidebarHiddenDefaultFavoritesJson = sidebarHiddenDefaultFavoritesJson
    }
    onDialogActiveChanged: {
        if (facade.dialogActive !== dialogActive)
            facade.dialogActive = dialogActive
    }
    onDialogModeChanged: {
        if (facade.dialogMode !== dialogMode)
            facade.dialogMode = dialogMode
    }
    onDialogFilePatternsChanged: {
        if (facade.dialogFilePatterns.toString() !== dialogFilePatterns.toString())
            facade.dialogFilePatterns = dialogFilePatterns
    }

    function isSelected(name) { return facade.isSelected(name) }
    function isPathSelected(path) { return facade.isPathSelected(path) }
    function clearSelection() { facade.clearSelection() }
    function handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection) {
        facade.handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection)
    }
    function selectAll() { facade.selectAll() }
    function selectByName(name) { facade.selectByName(name) }
    function selectByPath(path) { facade.selectByPath(path) }
    function prepareSelectionForDrag(name, index) { facade.prepareSelectionForDrag(name, index) }
    function createTab(path) { return facade.createTab(path || "") }
    function closeTab(index) { facade.closeTab(index) }
    function closeTabById(tabId) { facade.closeTabById(tabId) }
    function switchTabById(tabId) { facade.switchTabById(tabId) }
    function tabIndexById(tabId) { return facade.tabIndexById(tabId) }
    function moveTab(fromIndex, toIndex) { facade.moveTab(fromIndex, toIndex) }
    function switchTab(index) { facade.switchTab(index) }
    function navigateTo(path) { return facade.navigateTo(path) }
    function goBack() { facade.goBack() }
    function goForward() { facade.goForward() }
    function refreshCurrentFolder() { return facade.refreshCurrentFolder() }
    function loadRecent() { facade.loadRecent() }
    function recordRecentAccess(path, isDirectory, fileUrl) {
        facade.recordRecentAccess(path, isDirectory, fileUrl || "")
    }
    function createFolder(basePath, name) { return facade.createFolder(basePath, name) }
    function renamePath(sourcePath, newName) { return facade.renamePath(sourcePath, newName) }
    function requestDirectorySuggestions(basePath, prefix) {
        return facade.requestDirectorySuggestions(basePath, prefix)
    }
    function checkExecutable(program) { return facade.checkExecutable(program) }
    function requestProperties(path) { return facade.requestProperties(path) }
    function createDesktopShortcut(path) { return facade.createDesktopShortcut(path) }
    function requestNetworkMountProbe(rootPath) { return facade.requestNetworkMountProbe(rootPath) }
    function connectToNetwork(address) { return facade.connectToNetwork(address) }
    function refreshDevices() { facade.refreshDevices() }
    function ensureAutoMountDevices() { facade.ensureAutoMountDevices() }
    function requestMountDevice(path, fromAutoMount, openAfterMount) {
        return facade.requestMountDevice(path, fromAutoMount || false, openAfterMount || false)
    }
    function requestUnmountDevice(path, mountPath) { return facade.requestUnmountDevice(path, mountPath || "") }
    function requestRemountDevice(path, mountPath, openAfterMount) {
        return facade.requestRemountDevice(path, mountPath || "", openAfterMount || false)
    }
    function toggleDeviceAutoMount(deviceId, enabled) { facade.toggleDeviceAutoMount(deviceId, enabled) }
    function replaceFileModel(items) { return facade.replaceFileModel(items) }
    function updateFileModelMetadata(items) { return facade.updateFileModelMetadata(items) }
    function removePathsFromFileModel(paths) { return facade.removePathsFromFileModel(paths) }
    function selectedItem() { return facade.selectedItem() }
    function fileMatchesDialogFilter(fileName, isDirectory) {
        return facade.fileMatchesDialogFilter(fileName, isDirectory)
    }
    function hideSearch() { facade.hideSearch() }
    function submitSearch(rootPath, query) { return facade.submitSearch(rootPath, query || "") }
    function clearSearch() { facade.clearSearch() }
    function isCutPending(name) { return facade.isCutPending(name) }
    function isCutPathPending(path) { return facade.isCutPathPending(path) }
    function joinPath(directory, fileName) { return facade.joinPath(directory, fileName) }
    function fileUrlForPath(path) { return facade.fileUrlForPath(path) }
    function selectedPathsInCurrentFolder() { return facade.selectedPathsInCurrentFolder() }
    function selectedUriListInCurrentFolder() { return facade.selectedUriListInCurrentFolder() }
    function copySelected() { facade.copySelected() }
    function cutSelected() { facade.cutSelected() }
    function dropFiles(urls, destination, mode) { facade.dropFiles(urls, destination, mode || "copy") }
    function dropFilePaths(paths, destination, mode) { facade.dropFilePaths(paths, destination, mode || "copy") }
    function pasteFiles() { return facade.pasteFiles() }
    function resolvePasteConflict(policy) { facade.resolvePasteConflict(policy) }
    function renamePasteConflict(name) { facade.renamePasteConflict(name) }
    function cancelPasteConflict() { facade.cancelPasteConflict() }
    function deleteSelected() { facade.deleteSelected() }
    function restoreSelected() { facade.restoreSelected() }
    function emptyTrash() { facade.emptyTrash() }
    function startArchiveExtraction(path, folderName) { facade.startArchiveExtraction(path, folderName) }
    function submitArchivePassword(password) { facade.submitArchivePassword(password) }
    function cancelArchivePassword() { facade.cancelArchivePassword() }
    function submitArchiveConflict(policy) { facade.submitArchiveConflict(policy) }
    function cancelArchiveConflict() { facade.cancelArchiveConflict() }
    function startFolderCompression(path, format) { facade.startFolderCompression(path, format) }
    function installAppImage(path) { facade.installAppImage(path) }
    function setAsWallpaper(path) { facade.setAsWallpaper(path) }
    function openWithApplications(path) { return facade.openWithApplications(path) }
    function launchOpenWith(path, desktopFile) { return facade.launchOpenWith(path, desktopFile) }
    function setDefaultOpenWith(path, desktopFile) { return facade.setDefaultOpenWith(path, desktopFile) }
    function openItem(path, isDirectory, fileUrl) { facade.openItem(path, isDirectory, fileUrl || "") }
    function openFile(path) { facade.openFile(path) }
    function refreshPreviewMetadata() { facade.refreshPreviewMetadata() }
    function requestThumbnailWarm(path, offset, limit) { facade.requestThumbnailWarm(path, offset, limit) }
    function themedIconSource(iconName, size, themeName) { return facade.themedIconSource(iconName, size, themeName) }
    function sidebarIconSource(iconName, size) { return facade.sidebarIconSource(iconName, size) }
    function fileIconName(path, isDirectory, isExecutable) {
        return facade.fileIconName(path, isDirectory, isExecutable)
    }
    function fileIconSource(path, isDirectory, isExecutable, size, semanticIconName) {
        return facade.fileIconSource(path, isDirectory, isExecutable, size, semanticIconName || "")
    }
    function writePortalResult(json) { return facade.writePortalResult(json) }
    function setPendingPasteRename(name) { facade.pendingPasteRename = name }
    function setZoom(level) { facade.setZoom(level) }
    function increaseZoom() { facade.increaseZoom() }
    function decreaseZoom() { facade.decreaseZoom() }
    function resetZoom() { facade.resetZoom() }
    function setViewModeForZoom(mode) { facade.viewMode = mode }
    function isRecentPath(path) { return facade.isRecentPath(path) }
    function isTrashPath(path) { return facade.isTrashPath(path) }
    function canPinSidebarFavorite(path) { return facade.canPinSidebarFavorite(path) }
    function isSidebarFavorite(path) { return facade.isSidebarFavorite(path) }
    function visibleDefaultSidebarFavorites(items) { return facade.visibleDefaultSidebarFavorites(items) }
    function pinSidebarFavorite(path, label, icon) { facade.pinSidebarFavorite(path, label || "", icon || "") }
    function removeSidebarFavorite(path) { facade.removeSidebarFavorite(path) }
    function moveSidebarFavorite(path, index) { facade.moveSidebarFavorite(path, index) }
    function beginSidebarFavoriteDrag(path) { return facade.beginSidebarFavoriteDrag(path) }
    function previewSidebarFavoriteMove(path, index) {
        return facade.previewSidebarFavoriteMove(path, index)
    }
    function commitSidebarFavoriteDrag() { return facade.commitSidebarFavoriteDrag() }
    function cancelSidebarFavoriteDrag() { facade.cancelSidebarFavoriteDrag() }
}
