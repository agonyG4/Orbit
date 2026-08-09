import Quickshell
import QtQuick 2.15
import QtCore

QtObject {
    id: root

    property QtObject app
    readonly property bool isPortalDialog: (Quickshell.env("ASTREA_FILE_DIALOG_OPTIONS") || Quickshell.env("BENCH_FILE_DIALOG_OPTIONS") || "") !== ""
    readonly property string homePath: Quickshell.env("HOME") || ""
    readonly property string runtimeRoot: Quickshell.env("ASTREA_ROOT") || (homePath + "/.local/share/Astrea")
    readonly property string backendPath: runtimeRoot + "/Core/bridge/apps/explorer_backend"
    readonly property string helperPath: runtimeRoot + "/Apps/Explorer/explorer_helper.py"
    readonly property string wallpaperManagerPath: runtimeRoot + "/Core/bridge/wallpaper/wallpaper_manager.py"
    readonly property string astreaLaunch: runtimeRoot + "/bin/astrea-launch"
    readonly property string windowsRun: runtimeRoot + "/System/scripts/astrea-windows-run"
    readonly property string networkRootPath: (Quickshell.env("XDG_RUNTIME_DIR") || ("/run/user/" + Quickshell.env("UID"))) + "/gvfs"
    readonly property string trashFilesPath: homePath + "/.local/share/Trash/files"
    readonly property string trashInfoPath: homePath + "/.local/share/Trash/info"
    readonly property string recentVirtualPath: "recent://"

    readonly property var navigationState: app && app.navigation ? app.navigation : null
    readonly property var selectionState: app && app.selection ? app.selection : null
    readonly property var fileOperationsState: app && app.fileOps ? app.fileOps : null
    readonly property var previewState: app && app.preview ? app.preview : null

    property string currentPath: navigationState ? navigationState.currentPath : ""
    property var history: navigationState ? navigationState.history : []
    property int historyIdx: navigationState ? navigationState.historyIdx : -1
    property var tabs: navigationState ? navigationState.tabs : []
    property int activeTabIndex: navigationState ? navigationState.activeTabIndex : 0
    property var breadcrumbParts: navigationState ? navigationState.breadcrumbParts : []
    property bool loadingDir: navigationState ? navigationState.loadingDir : false
    property string loadError: navigationState ? navigationState.loadError : ""
    property bool remoteDirectoryActive: navigationState ? navigationState.remoteDirectoryActive : false
    property bool searchActive: navigationState ? navigationState.searchActive : false
    property bool searchVisible: navigationState ? navigationState.searchVisible : false
    property string searchQuery: navigationState ? navigationState.searchQuery : ""
    property var fileModel: navigationState ? navigationState.fileModel : null
    property int fileModelRevision: navigationState ? navigationState.fileModelRevision : 0
    property bool fileModelFilling: navigationState ? navigationState.fileModelFilling : false
    property string selectedFile: selectionState ? selectionState.selectedFile : ""
    property var selectedFiles: selectionState ? selectionState.selectedFiles : []
    property int lastSelectedIndex: selectionState ? selectionState.lastSelectedIndex : -1
    property var clipboardFiles: fileOperationsState ? fileOperationsState.clipboardFiles : []
    property string clipboardMode: fileOperationsState ? fileOperationsState.clipboardMode : "copy"
    property bool showPreview: previewState ? previewState.showPreview : false
    property bool previewsEnabled: previewState ? previewState.previewsEnabled : false
    property string viewMode: previewState ? previewState.viewMode : "list"
    property real zoomLevel: previewState ? previewState.zoomLevel : 1.0
    property string sortField: "name"
    property bool sortAsc: true
    property bool showHidden: false
    property bool foldersFirst: true
    property bool groupingEnabled: true
    property string sidebarFavoritesJson: "[]"
    property string sidebarHiddenDefaultFavoritesJson: "[]"
    property var sidebarFavorites: parseSidebarArray(sidebarFavoritesJson)
    property var sidebarHiddenDefaultFavorites: parseSidebarArray(sidebarHiddenDefaultFavoritesJson)
    property int sidebarFavoritesRevision: 0
    readonly property var defaultSidebarFavoritePaths: [
        homePath + "/Área de trabalho",
        homePath + "/Documentos",
        homePath + "/Downloads",
        homePath + "/Imagens",
        homePath + "/Músicas",
        homePath + "/Vídeos",
        homePath + "/Público",
        homePath + "/Modelos"
    ]
    readonly property bool inTrashView: isTrashPath(currentPath)
    property bool dialogActive: navigationState ? navigationState.dialogActive : false
    property string dialogMode: navigationState ? navigationState.dialogMode : "browse"
    property var dialogFilePatterns: navigationState ? navigationState.dialogFilePatterns : []
    property string autoMountDeviceIdsJson: app && app.deviceNet ? app.deviceNet.autoMountDeviceIdsJson : "[]"
    readonly property bool nativeFacade: false

    signal searchStateChanged()
    signal dialogStateChanged()
    signal clipboardStateChanged()

    property Settings persistedState: Settings {
        location: "file://" + root.homePath + "/.config/explorer.conf"
        category: "Explorer"
        property alias currentPath: root.currentPath
        property alias showPreview: root.showPreview
        property alias viewMode: root.viewMode
        property alias sortField: root.sortField
        property alias sortAsc: root.sortAsc
        property alias showHidden: root.showHidden
        property alias foldersFirst: root.foldersFirst
        property alias groupingEnabled: root.groupingEnabled
        property alias zoomLevel: root.zoomLevel
        property alias autoMountDeviceIdsJson: root.autoMountDeviceIdsJson
        property alias sidebarFavoritesJson: root.sidebarFavoritesJson
        property alias sidebarHiddenDefaultFavoritesJson: root.sidebarHiddenDefaultFavoritesJson
    }

    onCurrentPathChanged: {
        if (navigationState && navigationState.currentPath !== currentPath)
            navigationState.navigateTo(currentPath)
    }
    onDialogActiveChanged: {
        if (navigationState && navigationState.dialogActive !== dialogActive)
            navigationState.dialogActive = dialogActive
        dialogStateChanged()
    }
    onDialogModeChanged: {
        if (navigationState && navigationState.dialogMode !== dialogMode)
            navigationState.dialogMode = dialogMode
        dialogStateChanged()
    }
    onDialogFilePatternsChanged: {
        if (navigationState && navigationState.dialogFilePatterns.toString() !== dialogFilePatterns.toString())
            navigationState.dialogFilePatterns = dialogFilePatterns
        dialogStateChanged()
    }
    onSearchQueryChanged: {
        if (navigationState && navigationState.searchQuery !== searchQuery)
            navigationState.searchQuery = searchQuery
        searchStateChanged()
    }
    onSelectedFileChanged: {
        if (!selectionState || selectionState.selectedFile === selectedFile)
            return
        if (selectedFile)
            selectionState.selectByName(selectedFile)
        else
            selectionState.clearSelection()
    }
    onShowPreviewChanged: if (previewState && previewState.showPreview !== showPreview) previewState.showPreview = showPreview
    onViewModeChanged: if (previewState && previewState.viewMode !== viewMode) previewState.viewMode = viewMode

    function parseSidebarArray(text) {
        try {
            var value = JSON.parse(text || "[]")
            return Array.isArray(value) ? value : []
        } catch (error) {
            return []
        }
    }
    function isSelected(name) { return selectionState && selectionState.isSelected(name) }
    function clearSelection() { if (selectionState) selectionState.clearSelection() }
    function handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection) {
        if (selectionState) selectionState.handleSelection(name, index, ctrlMode, shiftMode, preserveCurrentSelection)
    }
    function selectAll() { if (selectionState) selectionState.selectAll() }
    function selectByName(name) { if (selectionState) selectionState.selectByName(name) }
    function createTab(path) { if (navigationState) navigationState.createTab(path || "") }
    function closeTab(index) { if (navigationState) navigationState.closeTab(index) }
    function closeTabById(tabId) { if (navigationState) navigationState.closeTabById(tabId) }
    function switchTabById(tabId) { if (navigationState) navigationState.switchTabById(tabId) }
    function tabIndexById(tabId) { return navigationState ? navigationState.tabIndexById(tabId) : -1 }
    function moveTab(fromIndex, toIndex) { if (navigationState) navigationState.moveTab(fromIndex, toIndex) }
    function switchTab(index) { if (navigationState) navigationState.switchTab(index) }
    function navigateTo(path) { return navigationState ? navigationState.navigateTo(path) : 0 }
    function goBack() { if (navigationState) navigationState.goBack() }
    function goForward() { if (navigationState) navigationState.goForward() }
    function refreshCurrentFolder() { return navigationState ? navigationState.refreshCurrentFolder() : 0 }
    function replaceFileModel(items) { return navigationState ? navigationState.replaceFileModel(items) : false }
    function updateFileModelMetadata(items) { return navigationState ? navigationState.updateFileModelMetadata(items) : 0 }
    function removePathsFromFileModel(paths) { return navigationState ? navigationState.removePathsFromFileModel(paths) : 0 }
    function selectedItem() { return navigationState ? navigationState.selectedItem() : null }
    function fileMatchesDialogFilter(fileName, isDirectory) { return navigationState ? navigationState.fileMatchesDialogFilter(fileName, isDirectory) : true }
    function hideSearch() { if (navigationState) navigationState.hideSearch() }
    function submitSearch(rootPath, query) { return navigationState ? navigationState.submitSearch(query || "") : 0 }
    function clearSearch() { if (navigationState) navigationState.clearSearch() }
    function isCutPending(name) { return fileOperationsState && fileOperationsState.isCutPending(name) }
    function joinPath(directory, fileName) { return fileOperationsState ? fileOperationsState.joinPath(directory, fileName) : directory + "/" + fileName }
    function fileUrlForPath(path) { return fileOperationsState ? fileOperationsState.fileUrlForPath(path) : path }
    function selectedPathsInCurrentFolder() { return fileOperationsState ? fileOperationsState.selectedPathsInCurrentFolder() : [] }
    function selectedUriListInCurrentFolder() { return fileOperationsState ? fileOperationsState.selectedUriListInCurrentFolder() : "" }
    function copySelected() { if (fileOperationsState) fileOperationsState.copySelected() }
    function cutSelected() { if (fileOperationsState) fileOperationsState.cutSelected() }
    function dropFiles(urls, destination, mode) { if (fileOperationsState) fileOperationsState.dropFiles(urls, destination, mode || "copy") }
    function dropFilePaths(paths, destination, mode) { if (fileOperationsState) fileOperationsState.dropFilePaths(paths, destination, mode || "copy") }
    function setZoom(level) { if (previewState) previewState.setZoom(level) }
    function increaseZoom() { if (previewState) previewState.increaseZoom() }
    function decreaseZoom() { if (previewState) previewState.decreaseZoom() }
    function resetZoom() { if (previewState) previewState.resetZoom() }
    function isRecentPath(path) { return path === recentVirtualPath }
    function isTrashPath(path) {
        var value = String(path || "")
        while (value.length > 1 && value.endsWith("/"))
            value = value.slice(0, -1)
        return value === trashFilesPath
    }
    function canPinSidebarFavorite(path) { return String(path || "").indexOf("/") === 0 && !isTrashPath(path) }
    function isSidebarFavorite(path) {
        for (var i = 0; i < sidebarFavorites.length; i++)
            if (sidebarFavorites[i].path === path)
                return true
        return defaultSidebarFavoritePaths.indexOf(path) !== -1 && sidebarHiddenDefaultFavorites.indexOf(path) === -1
    }
    function visibleDefaultSidebarFavorites(items) {
        var visible = []
        for (var i = 0; i < items.length; i++) {
            var path = items[i].path
            if (sidebarHiddenDefaultFavorites.indexOf(path) === -1)
                visible.push(items[i])
        }
        return visible
    }
    function pinSidebarFavorite(path, label, icon) {
        if (!canPinSidebarFavorite(path) || isSidebarFavorite(path))
            return
        var items = sidebarFavorites.slice()
        items.push({ label: label || path.split("/").pop(), icon: icon || "inode-directory", path: path })
        sidebarFavoritesJson = JSON.stringify(items)
        sidebarFavoritesRevision++
    }
    function removeSidebarFavorite(path) {
        var items = []
        for (var i = 0; i < sidebarFavorites.length; i++)
            if (sidebarFavorites[i].path !== path)
                items.push(sidebarFavorites[i])
        sidebarFavoritesJson = JSON.stringify(items)
        sidebarFavoritesRevision++
    }
}
