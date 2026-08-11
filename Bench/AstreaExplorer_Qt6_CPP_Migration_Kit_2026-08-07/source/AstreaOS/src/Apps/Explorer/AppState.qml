pragma Singleton
import QtQuick 2.15
import QtQml 2.15
import "state" as StateModules

QtObject {
    id: state

    // This is process-local and is set only after the native executable has
    // registered the NativeAppState singleton. Inherited environment state is
    // deliberately not part of the capability boundary.
    readonly property bool nativeCapabilityAvailable: Boolean(
        typeof astreaNativeAppStateAvailable !== "undefined"
        && astreaNativeAppStateAvailable === true)
    readonly property bool nativeAdapterReady: state.nativeCapabilityAvailable
        && state.nativeAppStateLoader.status === Loader.Ready
        && state.nativeAppStateLoader.item
        && state.nativeAppStateLoader.item.nativeFacade === true
    readonly property QtObject nativeAppState: state.nativeAdapterReady
        ? state.nativeAppStateLoader.item
        : state.legacyAppStateAdapter
    readonly property bool nativeNavigationActive: state.nativeAdapterReady
    readonly property bool isPortalDialog: nativeAppState.isPortalDialog
    readonly property string homePath: nativeAppState.homePath
    readonly property string backendPath: nativeAppState.backendPath
    readonly property string helperPath: nativeAppState.helperPath
    readonly property string wallpaperManagerPath: nativeAppState.wallpaperManagerPath
    readonly property string astreaLaunch: nativeAppState.astreaLaunch
    readonly property string windowsRun: nativeAppState.windowsRun
    readonly property string networkRootPath: nativeAppState.networkRootPath
    readonly property string trashFilesPath: nativeAppState.trashFilesPath
    readonly property string trashInfoPath: nativeAppState.trashInfoPath
    readonly property string recentVirtualPath: nativeAppState.recentVirtualPath
    readonly property real minZoom: 0.75
    readonly property real maxZoom: 2.0
    readonly property real thumbnailZoomThreshold: 1.15
    readonly property var thumbnailColumnStops: [18, 14, 10, 7, 5, 4, 3]
    readonly property var thumbnailScaleStops: [1.0, 1.08, 1.16, 1.26, 1.38, 1.65, 2.0]
    readonly property color themeSelected: Theme.selected
    readonly property color themeHover: Theme.hover
    property var scrollPositions: ({})

    property string sortField: nativeAppState.sortField
    property bool sortAsc: nativeAppState.sortAsc
    property bool showHidden: nativeAppState.showHidden
    property bool foldersFirst: nativeAppState.foldersFirst
    property bool groupingEnabled: nativeAppState.groupingEnabled
    property string sidebarFavoritesJson: nativeAppState.sidebarFavoritesJson
    property string sidebarHiddenDefaultFavoritesJson: nativeAppState.sidebarHiddenDefaultFavoritesJson
    property var sidebarFavorites: nativeAppState.sidebarFavorites
    property var sidebarHiddenDefaultFavorites: nativeAppState.sidebarHiddenDefaultFavorites
    readonly property int sidebarFavoritesRevision: nativeAppState.sidebarFavoritesRevision
    readonly property bool inTrashView: nativeAppState.inTrashView
    readonly property var defaultSidebarFavoritePaths: nativeAppState.defaultSidebarFavoritePaths

    property string currentPath: nativeAppState.currentPath
    property var history: nativeAppState.history
    property int historyIdx: nativeAppState.historyIdx
    property var tabs: nativeAppState.tabs
    property int activeTabIndex: nativeAppState.activeTabIndex
    property alias nextTabId: navigationObj.nextTabId
    property var breadcrumbParts: nativeAppState.breadcrumbParts
    property bool loadingDir: nativeAppState.loadingDir
    property string loadError: nativeAppState.loadError
    property bool dialogActive: nativeAppState.dialogActive
    property string dialogMode: nativeAppState.dialogMode
    property var dialogFilePatterns: nativeAppState.dialogFilePatterns
    property alias activeDirectoryRequestPath: navigationObj.activeDirectoryRequestPath
    // Keep the legacy alias as the presentation-facing mirror while native navigation owns the value.
    property alias remoteDirectoryActive: navigationObj.remoteDirectoryActive
    property alias remoteDirectoryReason: navigationObj.remoteDirectoryReason
    property bool searchActive: nativeAppState.searchActive
    property bool searchVisible: nativeAppState.searchVisible
    property string searchQuery: nativeAppState.searchQuery
    property alias searchRootPath: navigationObj.searchRootPath
    property var fileModel: nativeAppState.fileModel
    property int fileModelRevision: nativeAppState.fileModelRevision
    property bool fileModelFilling: nativeAppState.fileModelFilling

    property string selectedFile: nativeAppState.selectedFile
    property var selectedFiles: nativeAppState.selectedFiles
    property int lastSelectedIndex: nativeAppState.lastSelectedIndex

    property var clipboardFiles: nativeAppState.clipboardFiles
    property string clipboardMode: nativeAppState.clipboardMode
    property alias pasteConflictVisible: fileOpsObj.pasteConflictVisible
    property alias pasteConflictItems: fileOpsObj.pasteConflictItems
    property alias pendingPasteFiles: fileOpsObj.pendingPasteFiles
    property alias pendingPasteMode: fileOpsObj.pendingPasteMode
    property alias pendingPasteDestination: fileOpsObj.pendingPasteDestination
    property alias pendingPasteRename: fileOpsObj.pendingPasteRename
    property alias archiveExtractionRunning: fileOpsObj.archiveExtractionRunning
    property alias archiveExtractionProgress: fileOpsObj.archiveExtractionProgress
    property alias archiveExtractionPercent: fileOpsObj.archiveExtractionPercent
    property alias archiveExtractionFileName: fileOpsObj.archiveExtractionFileName
    property alias archiveExtractionStatus: fileOpsObj.archiveExtractionStatus
    property alias archiveExtractionError: fileOpsObj.archiveExtractionError
    property alias archiveExtractionDestination: fileOpsObj.archiveExtractionDestination
    property alias archiveExtractionDoneCount: fileOpsObj.archiveExtractionDoneCount
    property alias archiveExtractionTotalCount: fileOpsObj.archiveExtractionTotalCount
    property alias archiveExtractionRemainingText: fileOpsObj.archiveExtractionRemainingText
    property alias archivePasswordPromptVisible: fileOpsObj.archivePasswordPromptVisible
    property alias archivePassword: fileOpsObj.archivePassword
    property alias archivePasswordError: fileOpsObj.archivePasswordError
    property alias archiveConflictVisible: fileOpsObj.archiveConflictVisible
    property alias archiveConflictDestination: fileOpsObj.archiveConflictDestination
    property alias archiveConflictName: fileOpsObj.archiveConflictName
    property alias fileOperationRunning: fileOpsObj.fileOperationRunning
    property alias fileOperationProgress: fileOpsObj.fileOperationProgress
    property alias fileOperationPercent: fileOpsObj.fileOperationPercent
    property alias fileOperationFileName: fileOpsObj.fileOperationFileName
    property alias fileOperationStatus: fileOpsObj.fileOperationStatus
    property alias fileOperationError: fileOpsObj.fileOperationError
    property alias fileOperationDestination: fileOpsObj.fileOperationDestination
    property alias fileOperationDoneCount: fileOpsObj.fileOperationDoneCount
    property alias fileOperationTotalCount: fileOpsObj.fileOperationTotalCount
    property alias fileOperationMode: fileOpsObj.fileOperationMode
    property alias appImageInstallRunning: fileOpsObj.appImageInstallRunning
    property alias wallpaperApplyRunning: fileOpsObj.wallpaperApplyRunning

    property bool showPreview: nativeAppState.showPreview
    property string viewMode: nativeAppState.viewMode
    property alias previewsEnabled: previewObj.previewsEnabled
    property alias pendingThumbnailWarmRequest: previewObj.pendingThumbnailWarmRequest
    property alias activeThumbnailWarmRequest: previewObj.activeThumbnailWarmRequest
    property alias activePreviewRefreshPath: previewObj.activePreviewRefreshPath
    property alias startupWarmQueue: previewObj.startupWarmQueue
    property real zoomLevel: nativeAppState.zoomLevel

    property alias deviceModel: deviceNetObj.deviceModel
    property alias autoMountDeviceIds: deviceNetObj.autoMountDeviceIds
    property alias autoMountDeviceIdsJson: deviceNetObj.autoMountDeviceIdsJson
    property alias deviceOperationPath: deviceNetObj.deviceOperationPath
    property alias deviceOperationType: deviceNetObj.deviceOperationType
    property alias deviceOperationTargetMountPath: deviceNetObj.deviceOperationTargetMountPath
    property alias deviceOperationOpenAfterMount: deviceNetObj.deviceOperationOpenAfterMount
    property alias lastUnmountedMountPath: deviceNetObj.lastUnmountedMountPath
    property alias deviceError: deviceNetObj.deviceError
    property alias networkConnectVisible: deviceNetObj.networkConnectVisible
    property alias networkAddress: deviceNetObj.networkAddress
    property alias networkError: deviceNetObj.networkError
    property alias networkConnecting: deviceNetObj.networkConnecting

    property QtObject selection: StateModules.SelectionState {
        id: selectionObj
        app: state
    }

    property QtObject navigation: StateModules.NavigationState {
        id: navigationObj
        app: state
    }

    property QtObject fileOps: StateModules.FileOperationsState {
        id: fileOpsObj
        app: state
    }

    property QtObject preview: StateModules.PreviewState {
        id: previewObj
        app: state
    }

    property QtObject deviceNet: StateModules.DeviceNetworkState {
        id: deviceNetObj
        app: state
    }

    property QtObject recent: StateModules.RecentState {
        id: recentObj
        app: state
    }

    property QtObject legacyAppStateAdapter: LegacyAppStateAdapter {
        app: state
    }

    property Loader nativeAppStateLoader: Loader {
        id: nativeAppStateLoader
        active: state.nativeCapabilityAvailable
        source: "compatibility/NativeAppStateAdapter.qml"
    }

    property Connections nativeStateConnections: Connections {
        target: state.nativeAppState

        function onCurrentPathChanged() { state.currentPath = state.nativeAppState.currentPath }
        function onHistoryChanged() { state.history = state.nativeAppState.history }
        function onTabsChanged() { state.tabs = state.nativeAppState.tabs }
        function onActiveTabIndexChanged() { state.activeTabIndex = state.nativeAppState.activeTabIndex }
        function onLoadingDirChanged() { state.loadingDir = state.nativeAppState.loadingDir }
        function onLoadErrorChanged() { state.loadError = state.nativeAppState.loadError }
        function onRemoteDirectoryActiveChanged() {
            navigationObj.remoteDirectoryActive = state.nativeAppState.remoteDirectoryActive
        }
        function onSearchStateChanged() {
            state.searchActive = state.nativeAppState.searchActive
            state.searchVisible = state.nativeAppState.searchVisible
            state.searchQuery = state.nativeAppState.searchQuery
        }
        function onSelectedFileChanged() { state.selectedFile = state.nativeAppState.selectedFile }
        function onSelectedFilesChanged() { state.selectedFiles = state.nativeAppState.selectedFiles }
        function onLastSelectedIndexChanged() { state.lastSelectedIndex = state.nativeAppState.lastSelectedIndex }
        function onFileModelRevisionChanged() { state.fileModelRevision = state.nativeAppState.fileModelRevision }
        function onShowPreviewChanged() {
            state.showPreview = state.nativeAppState.showPreview
            previewObj.showPreview = state.nativeAppState.showPreview
        }
        function onViewModeChanged() {
            state.viewMode = state.nativeAppState.viewMode
            previewObj.viewMode = state.nativeAppState.viewMode
        }
        function onZoomLevelChanged() {
            state.zoomLevel = state.nativeAppState.zoomLevel
            previewObj.zoomLevel = state.nativeAppState.zoomLevel
        }
        function onSortFieldChanged() { state.sortField = state.nativeAppState.sortField }
        function onSortAscChanged() { state.sortAsc = state.nativeAppState.sortAsc }
        function onShowHiddenChanged() { state.showHidden = state.nativeAppState.showHidden }
        function onFoldersFirstChanged() { state.foldersFirst = state.nativeAppState.foldersFirst }
        function onGroupingEnabledChanged() { state.groupingEnabled = state.nativeAppState.groupingEnabled }
        function onSidebarFavoritesJsonChanged() {
            state.sidebarFavoritesJson = state.nativeAppState.sidebarFavoritesJson
            state.sidebarFavorites = state.nativeAppState.sidebarFavorites
        }
        function onSidebarHiddenDefaultFavoritesJsonChanged() {
            state.sidebarHiddenDefaultFavoritesJson = state.nativeAppState.sidebarHiddenDefaultFavoritesJson
            state.sidebarHiddenDefaultFavorites = state.nativeAppState.sidebarHiddenDefaultFavorites
        }
        function onSidebarFavoritesChanged() {
            state.sidebarFavorites = state.nativeAppState.sidebarFavorites
            state.sidebarHiddenDefaultFavorites = state.nativeAppState.sidebarHiddenDefaultFavorites
        }
        function onDialogStateChanged() {
            state.dialogActive = state.nativeAppState.dialogActive
            state.dialogMode = state.nativeAppState.dialogMode
            state.dialogFilePatterns = state.nativeAppState.dialogFilePatterns
        }
        function onClipboardStateChanged() {
            state.clipboardFiles = state.nativeAppState.clipboardFiles
            state.clipboardMode = state.nativeAppState.clipboardMode
            fileOpsObj.clipboardFiles = state.nativeAppState.clipboardFiles
            fileOpsObj.clipboardMode = state.nativeAppState.clipboardMode
        }
    }

    onCurrentPathChanged: {
        if (state.currentPath !== state.nativeAppState.currentPath)
            state.nativeAppState.navigateTo(state.currentPath)
    }
    onSearchQueryChanged: {
        if (state.searchQuery !== state.nativeAppState.searchQuery)
            state.nativeAppState.searchQuery = state.searchQuery
    }
    onSelectedFileChanged: {
        if (state.selectedFile !== state.nativeAppState.selectedFile)
            state.nativeAppState.selectedFile = state.selectedFile
    }
    onShowPreviewChanged: {
        if (state.showPreview !== state.nativeAppState.showPreview)
            state.nativeAppState.showPreview = state.showPreview
    }
    onViewModeChanged: {
        if (state.viewMode !== state.nativeAppState.viewMode)
            state.nativeAppState.viewMode = state.viewMode
    }
    onSortFieldChanged: {
        if (state.sortField !== state.nativeAppState.sortField)
            state.nativeAppState.sortField = state.sortField
    }
    onSortAscChanged: {
        if (state.sortAsc !== state.nativeAppState.sortAsc)
            state.nativeAppState.sortAsc = state.sortAsc
    }
    onShowHiddenChanged: {
        if (state.showHidden !== state.nativeAppState.showHidden)
            state.nativeAppState.showHidden = state.showHidden
    }
    onFoldersFirstChanged: {
        if (state.foldersFirst !== state.nativeAppState.foldersFirst)
            state.nativeAppState.foldersFirst = state.foldersFirst
    }
    onGroupingEnabledChanged: {
        if (state.groupingEnabled !== state.nativeAppState.groupingEnabled)
            state.nativeAppState.groupingEnabled = state.groupingEnabled
    }
    onZoomLevelChanged: {
        if (state.zoomLevel !== state.nativeAppState.zoomLevel)
            state.nativeAppState.zoomLevel = state.zoomLevel
    }
    onSidebarFavoritesJsonChanged: {
        if (state.sidebarFavoritesJson !== state.nativeAppState.sidebarFavoritesJson)
            state.nativeAppState.sidebarFavoritesJson = state.sidebarFavoritesJson
    }
    onSidebarHiddenDefaultFavoritesJsonChanged: {
        if (state.sidebarHiddenDefaultFavoritesJson !== state.nativeAppState.sidebarHiddenDefaultFavoritesJson)
            state.nativeAppState.sidebarHiddenDefaultFavoritesJson = state.sidebarHiddenDefaultFavoritesJson
    }
    onDialogActiveChanged: {
        if (state.dialogActive !== state.nativeAppState.dialogActive)
            state.nativeAppState.dialogActive = state.dialogActive
    }
    onDialogModeChanged: {
        if (state.dialogMode !== state.nativeAppState.dialogMode)
            state.nativeAppState.dialogMode = state.dialogMode
    }
    onDialogFilePatternsChanged: {
        if (state.dialogFilePatterns.toString() !== state.nativeAppState.dialogFilePatterns.toString())
            state.nativeAppState.dialogFilePatterns = state.dialogFilePatterns
    }

    Component.onCompleted: {
        if (!state.nativeNavigationActive)
            navigation.initialize()
        deferredStartupTimer.restart()
    }

    property Timer deferredStartupTimer: Timer {
        interval: 650
        repeat: false
        onTriggered: {
            if (state.nativeNavigationActive)
                nativeAppState.loadRecent()
            else
                recent.load()
            deviceNet.loadSavedAutoMounts()
            deviceNet.scheduleStartupDeviceRefresh()
            preview.enableStartupWork()
        }
    }

    function isSelected(name) { return nativeAppState.isSelected(name) }
    function clearSelection() { nativeAppState.clearSelection() }
    function handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection) {
        nativeAppState.handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection)
    }
    function selectAll() { nativeAppState.selectAll() }
    function selectByName(name) { nativeAppState.selectByName(name) }

    function createTab(initialPath) { nativeAppState.createTab(initialPath || "") }
    function closeTab(index) { nativeAppState.closeTab(index) }
    function closeTabById(tabId) { nativeAppState.closeTabById(tabId) }
    function switchTabById(tabId) { nativeAppState.switchTabById(tabId) }
    function tabIndexById(tabId) { return nativeAppState.tabIndexById(tabId) }
    function activeTabId() {
        if (activeTabIndex < 0 || activeTabIndex >= tabs.length)
            return -1
        return tabs[activeTabIndex].id
    }
    function moveTab(fromIndex, toIndex) { nativeAppState.moveTab(fromIndex, toIndex) }
    function switchTab(index) { nativeAppState.switchTab(index) }
    function navigateTo(path) { return nativeAppState.navigateTo(path) }
    function goBack() { nativeAppState.goBack() }
    function goForward() { nativeAppState.goForward() }
    function pathComponents() { return nativeAppState.breadcrumbParts }
    function rebuildBreadcrumbs() { return nativeAppState.breadcrumbParts }
    function refreshCurrentFolder() { return nativeAppState.refreshCurrentFolder() }
    function loadDirectory() { return nativeAppState.refreshCurrentFolder() }
    function replaceFileModel(items) { return nativeAppState.replaceFileModel(items) }
    function updateFileModelMetadata(items) { return nativeAppState.updateFileModelMetadata(items) }
    function removePathsFromFileModel(paths) { return nativeAppState.removePathsFromFileModel(paths) }
    function selectedItem() { return nativeAppState.selectedItem() }
    function fileMatchesDialogFilter(fileName, isDir) {
        return nativeAppState.fileMatchesDialogFilter(fileName, isDir)
    }
    function hideSearch() { nativeAppState.hideSearch() }
    function submitSearch(query) { return nativeAppState.submitSearch(currentPath, query || "") }
    function clearSearch() { nativeAppState.clearSearch() }

    function isCutPending(name) { return nativeAppState.isCutPending(name) }
    function joinPath(dirPath, fileName) { return nativeAppState.joinPath(dirPath, fileName) }
    function fileUrlForPath(path) { return nativeAppState.fileUrlForPath(path) }
    function selectedPathsInCurrentFolder() { return nativeAppState.selectedPathsInCurrentFolder() }
    function selectedUriListInCurrentFolder() { return nativeAppState.selectedUriListInCurrentFolder() }
    function copySelected() {
        nativeAppState.copySelected()
        fileOpsObj.clipboardFiles = nativeAppState.clipboardFiles
        fileOpsObj.clipboardMode = nativeAppState.clipboardMode
    }
    function cutSelected() {
        nativeAppState.cutSelected()
        fileOpsObj.clipboardFiles = nativeAppState.clipboardFiles
        fileOpsObj.clipboardMode = nativeAppState.clipboardMode
    }
    function pasteFiles() { fileOps.pasteFiles() }
    function dropFiles(urls, destinationPath, mode) {
        nativeAppState.dropFiles(urls, destinationPath, mode || "copy")
    }
    function dropFilePaths(paths, destinationPath, mode) {
        nativeAppState.dropFilePaths(paths, destinationPath, mode || "copy")
    }
    function resolvePasteConflict(policy) { fileOps.resolvePasteConflict(policy) }
    function renamePasteConflict(newName) { fileOps.renamePasteConflict(newName) }
    function cancelPasteConflict() { fileOps.cancelPasteConflict() }
    function deleteSelected() { fileOps.deleteSelected() }
    function restoreSelected() { fileOps.restoreSelected() }
    function emptyTrash() { fileOps.emptyTrash() }
    function startArchiveExtraction(archivePath, folderName) { fileOps.startArchiveExtraction(archivePath, folderName) }
    function submitArchivePassword(password) { fileOps.submitArchivePassword(password) }
    function cancelArchivePassword() { fileOps.cancelArchivePassword() }
    function submitArchiveConflict(policy) { fileOps.submitArchiveConflict(policy) }
    function cancelArchiveConflict() { fileOps.cancelArchiveConflict() }
    function startFolderCompression(folderPath, format) { fileOps.startFolderCompression(folderPath, format) }
    function isAppImageFileName(fileName) { return String(fileName || "").toLowerCase().endsWith(".appimage") }
    function isWallpaperImageFileName(fileName) { return /\.(avif|bmp|gif|heic|heif|jpe?g|png|tif|tiff|webp)$/i.test(String(fileName || "")) }
    function installAppImage(path) { fileOps.installAppImage(path) }
    function setAsWallpaper(path) { fileOps.setAsWallpaper(path) }

    function refreshPreviewMetadata() { preview.refreshPreviewMetadata() }
    function fileIconName(fileName, isFolder, isExecutable) { return preview.fileIconName(fileName, isFolder, isExecutable) }
    function portalIconSource(iconName, size) { return preview.portalIconSource(iconName, size) }
    function sidebarIconSource(iconName, size) { return preview.sidebarIconSource(iconName, size) }
    function isPreviewableFile(fileName, isDir) { return preview.isPreviewableFile(fileName, isDir) }
    function requestThumbnailWarm(path, offset, limit) { preview.requestThumbnailWarm(path, offset, limit) }
    function startThumbnailWarm(request) { preview.startThumbnailWarm(request) }
    function warmCurrentDirectoryThumbnails() { preview.warmCurrentDirectoryThumbnails() }
    function scheduleVisibleThumbnailWarm(firstIndex, lastIndex) { preview.scheduleVisibleThumbnailWarm(firstIndex, lastIndex) }
    function enqueueStartupWarm(path, limit) { preview.enqueueStartupWarm(path, limit) }
    function scheduleHomeThumbnailWarmup() { preview.scheduleHomeThumbnailWarmup() }
    function formatSize(bytes) { return preview.formatSize(bytes) }
    function formatDate(date) { return preview.formatDate(date) }
    function itemColor(name, hovered) { return preview.itemColor(name, hovered) }
    function setZoom(level) {
        nativeAppState.setZoom(level)
        previewObj.setZoom(nativeAppState.zoomLevel)
    }
    function increaseZoom() { setZoom(nativeAppState.zoomLevel + 0.1) }
    function decreaseZoom() { setZoom(nativeAppState.zoomLevel - 0.1) }
    function resetZoom() { setZoom(1.0) }
    function syncViewModeWithZoom() {
        nativeAppState.setViewMode(
            nativeAppState.zoomLevel >= thumbnailZoomThreshold ? "icon" : "list")
        previewObj.syncViewModeWithZoom()
    }
    function thumbnailLevel() { return preview.thumbnailLevel() }
    function thumbnailColumnCount() { return preview.thumbnailColumnCount() }
    function thumbnailScale() { return preview.thumbnailScale() }
    function openShellScript(path) { preview.openShellScript(path) }
    function openItem(path, isDir, fileUrl) { preview.openItem(path, isDir, fileUrl) }
    function recordRecentItem(path, isDir, fileUrl) {
        if (nativeNavigationActive)
            nativeAppState.recordRecentAccess(path, isDir, fileUrl || "")
        else
            recent.recordAccess(path, isDir, fileUrl)
    }
    function recentModelItems() {
        return nativeNavigationActive ? [] : recent.recentModelItems()
    }

    signal contextMenuOpening(string owner)

    function announceContextMenuOpening(owner) {
        contextMenuOpening(owner || "")
    }

    function normalizeSidebarPath(path) {
        var text = String(path || "")
        if (text.length > 1)
            text = text.replace(/\/+$/, "")
        return text
    }

    function sidebarLabelForPath(path) {
        var cleanPath = normalizeSidebarPath(path)
        if (cleanPath === homePath)
            return "Home Folder"
        if (cleanPath === "/")
            return "System"
        var parts = cleanPath.split("/").filter(Boolean)
        return parts.length > 0 ? parts[parts.length - 1] : cleanPath
    }

    function parseSidebarArray(text) {
        try {
            var parsed = JSON.parse(text || "[]")
            return Array.isArray(parsed) ? parsed : []
        } catch (error) {
            return []
        }
    }

    function loadSidebarFavorites() { }

    function saveSidebarFavorites() {
        nativeAppState.sidebarFavoritesJson = sidebarFavoritesJson
        nativeAppState.sidebarHiddenDefaultFavoritesJson = sidebarHiddenDefaultFavoritesJson
    }

    function isDefaultSidebarFavoritePath(path) {
        var cleanPath = normalizeSidebarPath(path)
        for (var i = 0; i < defaultSidebarFavoritePaths.length; i++) {
            if (normalizeSidebarPath(defaultSidebarFavoritePaths[i]) === cleanPath)
                return true
        }
        return false
    }

    function isDefaultSidebarFavoriteHidden(path) {
        var cleanPath = normalizeSidebarPath(path)
        for (var i = 0; i < sidebarHiddenDefaultFavorites.length; i++) {
            if (normalizeSidebarPath(sidebarHiddenDefaultFavorites[i]) === cleanPath)
                return true
        }
        return false
    }

    function isCustomSidebarFavorite(path) {
        return nativeAppState.isSidebarFavorite(path)
            && !isDefaultSidebarFavoritePath(path)
    }

    function isSidebarFavorite(path) {
        return nativeAppState.isSidebarFavorite(path)
    }

    function canPinSidebarFavorite(path) {
        return nativeAppState.canPinSidebarFavorite(path)
    }

    function visibleDefaultSidebarFavorites(items) {
        return nativeAppState.visibleDefaultSidebarFavorites(items)
    }

    function pinSidebarFavorite(path, label, icon) {
        nativeAppState.pinSidebarFavorite(path, label || "", icon || "")
    }

    function removeSidebarFavorite(path) {
        nativeAppState.removeSidebarFavorite(path)
    }

    function isTrashPath(path) {
        return nativeAppState.isTrashPath(path)
    }

    function isRecentPath(path) {
        return nativeAppState.isRecentPath(path)
    }

    function showNetworkConnectDialog() { deviceNet.showNetworkConnectDialog() }
    function hideNetworkConnectDialog() { deviceNet.hideNetworkConnectDialog() }
    function normalizedNetworkAddress() { return deviceNet.normalizedNetworkAddress() }
    function openNetworkBrowser() { deviceNet.openNetworkBrowser() }
    function connectToNetwork() { deviceNet.connectToNetwork() }
    function loadSavedAutoMounts() { deviceNet.loadSavedAutoMounts() }
    function saveAutoMounts() { deviceNet.saveAutoMounts() }
    function isDeviceAutoMount(deviceId) { return deviceNet.isDeviceAutoMount(deviceId) }
    function setDeviceAutoMount(deviceId, enabled) { deviceNet.setDeviceAutoMount(deviceId, enabled) }
    function toggleDeviceAutoMount(deviceId) { deviceNet.toggleDeviceAutoMount(deviceId) }
    function syncDeviceAutoMountFlags() { deviceNet.syncDeviceAutoMountFlags() }
    function replaceDeviceModel(items) { deviceNet.replaceDeviceModel(items) }
    function refreshDevices() { deviceNet.refreshDevices() }
    function ensureAutoMountDevices() { deviceNet.ensureAutoMountDevices() }
    function requestMountDevice(devicePath, fromAutoMount, openAfterMount) { deviceNet.requestMountDevice(devicePath, fromAutoMount, openAfterMount) }
    function requestUnmountDevice(devicePath, mountPath) { deviceNet.requestUnmountDevice(devicePath, mountPath) }
    function requestRemountDevice(devicePath, mountPath, openAfterMount) { deviceNet.requestRemountDevice(devicePath, mountPath, openAfterMount) }
    function syncDeviceBusyFlags() { deviceNet.syncDeviceBusyFlags() }
    function startSearch() { nativeAppState.startSearch() }

    function scrollPositionKey(path, viewMode) {
        return (viewMode || "list") + "::" + (path || "")
    }

    function rememberScrollPosition(path, viewMode, position) {
        if (!path || searchActive)
            return
        if (typeof position !== "number" || isNaN(position))
            return

        var key = scrollPositionKey(path, viewMode)
        var next = {}
        for (var existingKey in scrollPositions)
            next[existingKey] = scrollPositions[existingKey]
        next[key] = Math.max(0, position)
        scrollPositions = next
    }

    function savedScrollPosition(path, viewMode) {
        if (!path || searchActive)
            return 0

        var key = scrollPositionKey(path, viewMode)
        return scrollPositions[key] || 0
    }

    signal dialogFileActivated(string path, string fileUrl)

    onAutoMountDeviceIdsJsonChanged: loadSavedAutoMounts()
}
