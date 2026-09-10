import QtQuick 2.15

// Preview generation and visual metadata hydration are native. This object
// keeps formatting and icon source helpers close to presentation code.
QtObject {
    id: preview

    property QtObject app
    readonly property var bridge: app ? app.nativeAppState : null
    property bool showPreview: bridge ? bridge.showPreview : false
    property string viewMode: bridge ? bridge.viewMode : "list"
    property bool previewsEnabled: bridge ? bridge.previewsEnabled : false
    property real zoomLevel: bridge ? bridge.zoomLevel : 1.0
    readonly property var iconThemeRevision: app ? app.iconThemeRevision : 0

    function refreshPreviewMetadata() { if (bridge) bridge.refreshPreviewMetadata() }
    function fileIconName(path, isFolder, isExecutable) {
        var revision = iconThemeRevision
        if (bridge && typeof bridge.fileIconName === "function")
            return bridge.fileIconName(path || "", isFolder, isExecutable)
        return isFolder ? "inode-directory" : "application-x-generic"
    }
    function themedIconSource(iconName, size, themeName, devicePixelRatio) {
        return bridge
            ? bridge.themedIconSource(iconName, size, themeName, devicePixelRatio || 1.0)
            : ""
    }
    function fileIconSource(path, isFolder, isExecutable, size, semanticIconName, devicePixelRatio) {
        var revision = iconThemeRevision
        return bridge && typeof bridge.fileIconSource === "function"
            ? bridge.fileIconSource(
                path || "", isFolder, isExecutable, size, semanticIconName || "",
                devicePixelRatio || 1.0)
            : ""
    }
    function stringListFromModel(value) {
        if (value === undefined || value === null)
            return []

        if (typeof value.get === "function" && typeof value.count === "number") {
            var modelValues = []
            for (var i = 0; i < value.count; i++) {
                var modelValue = value.get(i)
                if (modelValue && typeof modelValue === "object") {
                    if (modelValue.value !== undefined)
                        modelValue = modelValue.value
                    else if (modelValue.modelData !== undefined)
                        modelValue = modelValue.modelData
                }
                if (modelValue !== undefined && modelValue !== null
                        && String(modelValue) !== "")
                    modelValues.push(String(modelValue))
            }
            return modelValues
        }

        if (Array.isArray(value)) {
            var arrayValues = []
            for (var j = 0; j < value.length; j++) {
                if (value[j] !== undefined && value[j] !== null
                        && String(value[j]) !== "")
                    arrayValues.push(String(value[j]))
            }
            return arrayValues
        }

        return [String(value)]
    }
    function richFileIconSource(path, isFolder, isExecutable, size, semanticIconName, iconNames, iconFileUrl, iconFileVersion, devicePixelRatio) {
        var revision = iconThemeRevision
        return bridge && typeof bridge.richFileIconSource === "function"
            ? bridge.richFileIconSource(
                path || "", isFolder, isExecutable, size, semanticIconName || "",
                stringListFromModel(iconNames), iconFileUrl || "", iconFileVersion || "",
                devicePixelRatio || 1.0)
            : fileIconSource(path, isFolder, isExecutable, size, semanticIconName, devicePixelRatio)
    }
    function emblemIconSource(name, size, devicePixelRatio) {
        return bridge && typeof bridge.emblemIconSource === "function"
            ? bridge.emblemIconSource(name || "", size, devicePixelRatio || 1.0)
            : ""
    }
    function portalIconSource(iconName, size, devicePixelRatio) {
        return themedIconSource(iconName, size, "", devicePixelRatio)
    }
    function sidebarIconSource(iconName, size, devicePixelRatio) {
        return bridge && typeof bridge.sidebarIconSource === "function"
            ? bridge.sidebarIconSource(iconName, size, devicePixelRatio || 1.0)
            : ""
    }
    function requestVisibleThumbnailRange(firstIndex, lastIndex, physicalTarget) {
        if (bridge && typeof bridge.requestVisibleThumbnailRange === "function")
            bridge.requestVisibleThumbnailRange(
                firstIndex || 0, lastIndex || firstIndex || 0, physicalTarget || 128)
    }
    function requestSelectedThumbnail(filePath, physicalTarget) {
        if (bridge && typeof bridge.requestSelectedThumbnail === "function")
            bridge.requestSelectedThumbnail(filePath || "", physicalTarget || 320)
    }
    function requestFileVisualMetadata(firstIndex, lastIndex) {
        if (bridge && typeof bridge.requestFileVisualMetadata === "function")
            bridge.requestFileVisualMetadata(
                firstIndex || 0, lastIndex || firstIndex || 0)
    }
    function scheduleVisibleFileVisualMetadata(firstIndex, lastIndex) {
        requestFileVisualMetadata(firstIndex, lastIndex)
    }
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
        return padDatePart(date.getDate()) + "/"
            + padDatePart(date.getMonth() + 1) + "/" + date.getFullYear()
    }
    function formatDate(date) {
        if (!date) return "—"
        var value = typeof date === "number" ? new Date(date) : new Date(date)
        if (isNaN(value.getTime())) return "—"
        var diff = (new Date() - value) / 1000
        if (diff < 60) return "Agora"
        if (diff < 3600) return Math.floor(diff / 60) + " min atrás"
        if (value.toDateString() === new Date().toDateString())
            return "Hoje, " + Qt.formatTime(value, "hh:mm")
        return formatAbsoluteDate(value)
    }
    function itemColor(name, hovered) {
        return app.isSelected(name)
            ? app.themeSelected
            : (hovered ? app.themeHover : "transparent")
    }
    function setZoom(level) { if (bridge) bridge.setZoom(level) }
    function increaseZoom() { if (bridge) bridge.increaseZoom() }
    function decreaseZoom() { if (bridge) bridge.decreaseZoom() }
    function resetZoom() { if (bridge) bridge.resetZoom() }
    function syncViewModeWithZoom() {
        if (bridge) bridge.setViewModeForZoom(zoomLevel >= app.thumbnailZoomThreshold ? "icon" : "list")
    }
    function thumbnailLevel() {
        if (zoomLevel < 1.25) return 0
        if (zoomLevel < 1.35) return 1
        if (zoomLevel < 1.45) return 2
        if (zoomLevel < 1.55) return 3
        if (zoomLevel < 1.7) return 4
        if (zoomLevel < 1.9) return 5
        return 6
    }
    function thumbnailColumnCount() { return app.thumbnailColumnStops[thumbnailLevel()] }
    function thumbnailScale() { return app.thumbnailScaleStops[thumbnailLevel()] }
    function openShellScript(path) { if (bridge) bridge.openFile(path) }
    function openItem(path, isDir, fileUrl) {
        if (bridge) bridge.openItem(path, isDir, fileUrl || "")
    }
}
