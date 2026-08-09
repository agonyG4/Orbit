import QtQml 2.15
import Astrea.Explorer.Native 1.0

QtObject {
    id: root

    readonly property QtObject facade: NativeAppState
    readonly property bool isPortalDialog: facade.isPortalDialog
    readonly property string homePath: facade.homePath
    readonly property string backendPath: facade.backendPath
    readonly property string helperPath: facade.helperPath
    readonly property string wallpaperManagerPath: facade.wallpaperManagerPath
    readonly property string astreaLaunch: facade.astreaLaunch
    readonly property string windowsRun: facade.windowsRun
    readonly property string networkRootPath: facade.networkRootPath
    readonly property string trashFilesPath: facade.trashFilesPath
    readonly property string trashInfoPath: facade.trashInfoPath
    readonly property string recentVirtualPath: facade.recentVirtualPath

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
    property int lastSelectedIndex: facade.lastSelectedIndex
    property var clipboardFiles: facade.clipboardFiles
    property string clipboardMode: facade.clipboardMode
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
    property int sidebarFavoritesRevision: facade.sidebarFavoritesRevision
    property var defaultSidebarFavoritePaths: facade.defaultSidebarFavoritePaths
    property bool inTrashView: facade.inTrashView
    property bool dialogActive: facade.dialogActive
    property string dialogMode: facade.dialogMode
    property var dialogFilePatterns: facade.dialogFilePatterns

    readonly property bool nativeFacade: true

    signal searchStateChanged()
    signal dialogStateChanged()
    signal clipboardStateChanged()

    property Connections facadeConnections: Connections {
        target: root.facade
        function onSearchStateChanged() { root.searchStateChanged() }
        function onDialogStateChanged() { root.dialogStateChanged() }
        function onClipboardStateChanged() { root.clipboardStateChanged() }
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
    function clearSelection() { facade.clearSelection() }
    function handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection) {
        facade.handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection)
    }
    function selectAll() { facade.selectAll() }
    function selectByName(name) { facade.selectByName(name) }
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
    function joinPath(directory, fileName) { return facade.joinPath(directory, fileName) }
    function fileUrlForPath(path) { return facade.fileUrlForPath(path) }
    function selectedPathsInCurrentFolder() { return facade.selectedPathsInCurrentFolder() }
    function selectedUriListInCurrentFolder() { return facade.selectedUriListInCurrentFolder() }
    function copySelected() { facade.copySelected() }
    function cutSelected() { facade.cutSelected() }
    function dropFiles(urls, destination, mode) { facade.dropFiles(urls, destination, mode || "copy") }
    function dropFilePaths(paths, destination, mode) { facade.dropFilePaths(paths, destination, mode || "copy") }
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
}
