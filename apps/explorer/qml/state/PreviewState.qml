import QtQuick 2.15

// Preview metadata and thumbnail scheduling are native. This compatibility
// object keeps formatting and icon lookup helpers close to presentation code.
QtObject {
    id: preview

    property QtObject app
    readonly property var bridge: app && app.nativeAppState ? app.nativeAppState : null
    property bool showPreview: bridge ? bridge.showPreview : false
    property string viewMode: bridge ? bridge.viewMode : "list"
    property bool previewsEnabled: bridge ? bridge.previewsEnabled : false
    property var pendingThumbnailWarmRequest: null
    property var activeThumbnailWarmRequest: null
    property string activePreviewRefreshPath: ""
    property var startupWarmQueue: []
    property bool startupWorkEnabled: false
    property real zoomLevel: bridge ? bridge.zoomLevel : 1.0
    readonly property var iconThemeRevision: app ? app.iconThemeRevision : 0

    function refreshPreviewMetadata() { if (bridge) bridge.refreshPreviewMetadata() }
    function fileIconName(path, isFolder, isExecutable) {
        var revision = iconThemeRevision
        if (bridge && typeof bridge.fileIconName === "function")
            return bridge.fileIconName(path || "", isFolder, isExecutable)
        return isFolder ? "inode-directory" : "application-x-generic"
    }
    function themedIconSource(iconName, size, themeName) {
        return bridge ? bridge.themedIconSource(iconName, size, themeName) : ""
    }
    function fileIconSource(path, isFolder, isExecutable, size, semanticIconName) {
        var revision = iconThemeRevision
        return bridge && typeof bridge.fileIconSource === "function"
            ? bridge.fileIconSource(path || "", isFolder, isExecutable, size, semanticIconName || "")
            : ""
    }
    function portalIconSource(iconName, size) { return themedIconSource(iconName, size, "") }
    function sidebarIconSource(iconName, size) {
        return bridge && typeof bridge.sidebarIconSource === "function"
            ? bridge.sidebarIconSource(iconName, size)
            : ""
    }
    function isPreviewableFile(fileName, isDir) {
        return !isDir && /\.(jpg|jpeg|png|gif|bmp|webp|svg)$/i.test(String(fileName || ""))
    }
    function requestThumbnailWarm(path, offset, limit) {
        if (!bridge || !path || app.remoteDirectoryActive) return
        pendingThumbnailWarmRequest = { path: path, offset: offset || 0, limit: limit || 12 }
        bridge.requestThumbnailWarm(path, offset || 0, limit || 12)
    }
    function startThumbnailWarm(request) { if (request) requestThumbnailWarm(request.path, request.offset, request.limit) }
    function warmCurrentDirectoryThumbnails() { requestThumbnailWarm(app.currentPath, 0, viewMode === "icon" ? 18 : 24) }
    function scheduleVisibleThumbnailWarm(firstIndex, lastIndex) {
        requestThumbnailWarm(app.currentPath, firstIndex, Math.max(8, lastIndex - firstIndex + 1))
    }
    function enqueueStartupWarm(path, limit) { startupWarmQueue.push({ path: path, limit: limit }) }
    function scheduleHomeThumbnailWarmup() {}
    function enableStartupWork() { startupWorkEnabled = true; warmCurrentDirectoryThumbnails() }
    function formatSize(bytes) {
        if (bytes < 0) return "—"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB"
        return (bytes / 1073741824).toFixed(2) + " GB"
    }
    function padDatePart(value) { return value < 10 ? "0" + value : String(value) }
    function formatAbsoluteDate(date) {
        if (!(date instanceof Date) || isNaN(date.getTime())) return "—"
        return padDatePart(date.getDate()) + "/" + padDatePart(date.getMonth() + 1) + "/" + date.getFullYear()
    }
    function formatDate(date) {
        if (!date) return "—"
        var value = typeof date === "number" ? new Date(date) : new Date(date)
        if (isNaN(value.getTime())) return "—"
        var diff = (new Date() - value) / 1000
        if (diff < 60) return "Agora"
        if (diff < 3600) return Math.floor(diff / 60) + " min atrás"
        if (value.toDateString() === new Date().toDateString()) return "Hoje, " + Qt.formatTime(value, "hh:mm")
        return formatAbsoluteDate(value)
    }
    function itemColor(name, hovered) { return app.isSelected(name) ? app.themeSelected : (hovered ? app.themeHover : "transparent") }
    function setZoom(level) { if (bridge) bridge.setZoom(level) }
    function increaseZoom() { if (bridge) bridge.increaseZoom() }
    function decreaseZoom() { if (bridge) bridge.decreaseZoom() }
    function resetZoom() { if (bridge) bridge.resetZoom() }
    function syncViewModeWithZoom() { if (bridge) bridge.setViewModeForZoom(zoomLevel >= app.thumbnailZoomThreshold ? "icon" : "list") }
    function thumbnailLevel() { if (zoomLevel < 1.25) return 0; if (zoomLevel < 1.35) return 1; if (zoomLevel < 1.45) return 2; if (zoomLevel < 1.55) return 3; if (zoomLevel < 1.7) return 4; if (zoomLevel < 1.9) return 5; return 6 }
    function thumbnailColumnCount() { return app.thumbnailColumnStops[thumbnailLevel()] }
    function thumbnailScale() { return app.thumbnailScaleStops[thumbnailLevel()] }
    function openShellScript(path) { if (bridge) bridge.openFile(path) }
    function openItem(path, isDir, fileUrl) { if (bridge) bridge.openItem(path, isDir, fileUrl || "") }
}
