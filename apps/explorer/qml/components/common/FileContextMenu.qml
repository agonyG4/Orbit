import QtQuick 2.15
import QtQuick.Controls 2.15
import "../.."
import "." as Common
import Astrea.Files 1.0 as AstreaFiles
import Astrea.I18n 1.0 as AstreaI18n

Item {
    id: menuRoot
    anchors.fill: parent
    visible: menuFrame.menuOpen || creatingFolder || renamingItem || compressionDialogOpen
    z: 999

    property string itemPath: ""
    property string itemUrl: ""
    property bool itemIsDir: false
    property var clipboardProxy
    property string menuOwner: "file-context"
    property bool creatingFolder: false
    property bool renamingItem: false
    property bool compressionDialogOpen: false
    property var compressionSources: []
    property string compressionArchiveName: ""
    property string compressionFormat: ""
    property string compressionProfile: "balanced"
    property bool compressionNameEdited: false
    property string pendingExtractionPath: ""
    property string pendingFolderName: ""
    property string pendingRenameName: ""
    property int createFolderRequestId: 0
    property int renameRequestId: 0
    readonly property bool isBackgroundTarget: itemPath === AppState.currentPath && itemIsDir
    readonly property bool isArchiveTarget: !itemIsDir && AppState.canExtractArchive(itemPath)
    readonly property bool archiveOperationAvailable: !AppState.archiveWorkflowOccupied
    readonly property bool isAppImageTarget: !itemIsDir && AppState.isAppImageFileName(itemPath)
    readonly property bool isWallpaperImageTarget: !itemIsDir && !isBackgroundTarget && !AppState.inTrashView && AppState.isWallpaperImageFileName(itemPath)
    readonly property bool canCompressTarget: !isBackgroundTarget && !AppState.inTrashView
    readonly property bool canToggleSidebarFavorite: itemIsDir && !isBackgroundTarget && !AppState.inTrashView && AppState.canPinSidebarFavorite(itemPath)
    readonly property var createCapabilities: AppState.archiveCapabilities.filter(function(capability) {
        return capability.createSupported === true
    })
    readonly property bool createArchiveAvailable: archiveOperationAvailable && createCapabilities.length > 0

    function dismissTransientUi() {
        menuFrame.closeMenu()
        compressionDialogOpen = false
        creatingFolder = false
        renamingItem = false
    }

    function openAt(x, y, path, isDir, url) {
        AppState.announceContextMenuOpening(menuOwner)
        itemPath = path
        itemIsDir = isDir
        itemUrl = url
        compressionDialogOpen = false
        menuFrame.openAt(x, y)
    }

    function closeMenu() {
        compressionSubmenuOpen = false
        menuFrame.closeMenu()
    }

    Shortcut {
        sequence: "Esc"
        enabled: menuRoot.visible
        onActivated: menuRoot.dismissTransientUi()
    }

    Connections {
        target: AppState
        function onContextMenuOpening(owner) {
            if (owner !== menuRoot.menuOwner)
                menuRoot.closeMenu()
        }
    }

    function runOpen() {
        closeMenu()
        AppState.openItem(itemPath, itemIsDir, itemUrl)
    }

    function runOpenWith() {
        if (isBackgroundTarget)
            return
        var point = menuRoot.mapToItem(openWithMenu, menuFrame.menuX + menuFrame.menuWidth - 8, menuFrame.menuY + 4)
        closeMenu()
        openWithMenu.openAt(point.x, point.y, itemPath)
    }

    function runCopyPath() {
        if (clipboardProxy && itemPath !== "")
            clipboardProxy.copyPath(itemPath)
        closeMenu()
    }

    function runToggleSidebarFavorite() {
        if (!canToggleSidebarFavorite)
            return
        var name = itemPath.split("/").filter(Boolean).pop() || itemPath
        if (AppState.isSidebarFavorite(itemPath))
            AppState.removeSidebarFavorite(itemPath)
        else
            AppState.pinSidebarFavorite(itemPath, name, AppState.fileIconName(itemPath, true, false))
        closeMenu()
    }

    function runCreateFolder() {
        closeMenu()
        pendingFolderName = "Nova pasta"
        creatingFolder = true
        Qt.callLater(function() { nameField.forceActiveFocus(); nameField.selectAll() })
    }

    function runRename() {
        if (isBackgroundTarget) return
        closeMenu()
        pendingRenameName = itemPath.split("/").pop()
        renamingItem = true
        Qt.callLater(function() { renameField.forceActiveFocus(); renameField.selectAll() })
    }

    function extractionFolderName() {
        return AppState.canonicalArchiveStem(itemPath)
    }

    function runExtractHere() {
        if (!isArchiveTarget || !archiveOperationAvailable)
            return
        closeMenu()
        AppState.startArchiveExtractionHere(itemPath, AppState.currentPath)
    }

    function runExtractToNamedFolder() {
        if (!isArchiveTarget || !archiveOperationAvailable)
            return
        closeMenu()
        AppState.startArchiveExtraction(itemPath, extractionFolderName())
    }

    function runExtractTo() {
        if (!isArchiveTarget || !archiveOperationAvailable)
            return
        pendingExtractionPath = itemPath
        closeMenu()
        extractionFolderDialog.startFolder = AppState.currentPath || AppState.homePath
        extractionFolderDialog.openDialog()
    }

    function stripArchiveExtension(name) {
        return AppState.canonicalArchiveStem(String(name || ""))
    }

    function validArchiveName(name) {
        var value = String(name || "").trim()
        return value !== ""
            && value !== "."
            && value !== ".."
            && value.indexOf("/") < 0
            && value.indexOf("\\") < 0
            && value !== "/"
    }

    function openCompressionDialog() {
        if (!canCompressTarget || !archiveOperationAvailable || createCapabilities.length === 0)
            return
        compressionSources = AppState.isPathSelected(itemPath) && AppState.selectedPaths.length > 0
            ? AppState.selectedPaths.slice()
            : [itemPath]
        compressionNameEdited = false
        compressionArchiveName = compressionSources.length === 1
            ? stripArchiveExtension(compressionSources[0].split("/").pop())
            : "Archive"
        compressionFormat = createCapabilities[0].id
        compressionProfile = "balanced"
        closeMenu()
        compressionDialogOpen = true
        Qt.callLater(function() { compressionNameField.forceActiveFocus(); compressionNameField.selectAll() })
    }

    function runCompress() {
        if (!canCompressTarget || !archiveOperationAvailable || compressionSources.length === 0)
            return
        if (!validArchiveName(compressionArchiveName))
            return
        compressionDialogOpen = false
        AppState.startArchiveCreation(compressionSources, compressionArchiveName, compressionFormat, compressionProfile)
    }

    function runInstallAppImage() {
        if (!isAppImageTarget || AppState.appImageInstallRunning)
            return
        closeMenu()
        AppState.installAppImage(itemPath)
    }

    function runSetAsWallpaper() {
        if (!isWallpaperImageTarget || AppState.wallpaperApplyRunning)
            return
        closeMenu()
        AppState.setAsWallpaper(itemPath)
    }

    function runShowProperties() {
        closeMenu()
        if (propertiesWin.visible)
            propertiesWin.close()
        var selected = AppState.selectedFiles
        var inSelection = AppState.isPathSelected(itemPath)

        if (inSelection && selected.length > 1) {
            propertiesWin.isMulti = true
            propertiesWin.targetPaths = AppState.selectedPaths
            propertiesWin.targetPath = ""
            propertiesWin.targetIsDir = false
        } else {
            propertiesWin.isMulti = false
            propertiesWin.targetPath = itemPath
            propertiesWin.targetIsDir = itemIsDir
            propertiesWin.targetPaths = []
        }

        propertiesWin.show()
        propertiesWin.raise()
        propertiesWin.requestActivate()
    }

    function confirmCreateFolder() {
        var trimmed = pendingFolderName.trim()
        if (trimmed === "") return
        pendingFolderName = trimmed
        createFolderRequestId = AppState.createFolder(AppState.currentPath, pendingFolderName)
        creatingFolder = false
    }

    function confirmRename() {
        var trimmed = pendingRenameName.trim()
        var currentName = itemPath.split("/").pop()
        if (trimmed === "" || trimmed === currentName) return
        renameRequestId = AppState.renamePath(itemPath, pendingRenameName)
        renamingItem = false
    }

    function runDelete() {
        if (!AppState.isPathSelected(itemPath))
            AppState.selectByPath(itemPath)
        AppState.deleteSelected()
        closeMenu()
    }

    function runRestore() {
        if (!AppState.inTrashView || menuRoot.isBackgroundTarget)
            return
        if (!AppState.isPathSelected(itemPath))
            AppState.selectByPath(itemPath)
        AppState.restoreSelected()
        closeMenu()
    }

    AstreaFiles.FileContextMenu {
        id: menuFrame
        anchors.fill: parent

        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.abrir"]) || "Open")
            actionEnabled: true
            visible: !menuRoot.isBackgroundTarget
            onTriggered: menuRoot.runOpen()
        }
        Common.ContextMenuAction {
            label: "Abrir com"
            actionEnabled: true
            visible: !menuRoot.isBackgroundTarget
            onTriggered: menuRoot.runOpenWith()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.nova_pasta"]) || "New Folder")
            actionEnabled: true
            onTriggered: menuRoot.runCreateFolder()
        }
        Common.ContextMenuDivider {}
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.copiar_caminho"]) || "Copy Path")
            actionEnabled: true
            onTriggered: menuRoot.runCopyPath()
        }
        Common.ContextMenuAction {
            label: AppState.isSidebarFavorite(menuRoot.itemPath) ? "Remover dos Favoritos" : "Fixar na sidebar"
            actionEnabled: true
            visible: menuRoot.canToggleSidebarFavorite
            destructive: AppState.isSidebarFavorite(menuRoot.itemPath)
            onTriggered: menuRoot.runToggleSidebarFavorite()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.renomear"]) || "Rename")
            actionEnabled: true
            visible: !menuRoot.isBackgroundTarget
            onTriggered: menuRoot.runRename()
        }
        Common.ContextMenuAction {
            id: compressAction
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.compactar"]) || "Compress")
            actionEnabled: menuRoot.createArchiveAvailable
            hasSubmenu: false
            visible: menuRoot.canCompressTarget
            onTriggered: menuRoot.openCompressionDialog()
        }
        Common.ContextMenuAction {
            label: "Extract Here"
            actionEnabled: menuRoot.archiveOperationAvailable
            visible: menuRoot.isArchiveTarget
            onTriggered: menuRoot.runExtractHere()
        }
        Common.ContextMenuAction {
            label: "Extract to \"" + menuRoot.extractionFolderName() + "/\""
            actionEnabled: menuRoot.archiveOperationAvailable
            visible: menuRoot.isArchiveTarget
            onTriggered: menuRoot.runExtractToNamedFolder()
        }
        Common.ContextMenuAction {
            label: "Extract…"
            actionEnabled: menuRoot.archiveOperationAvailable
            visible: menuRoot.isArchiveTarget
            onTriggered: menuRoot.runExtractTo()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.install"]) || "Install")
            actionEnabled: !AppState.appImageInstallRunning
            visible: menuRoot.isAppImageTarget
            onTriggered: menuRoot.runInstallAppImage()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.definir_como_wallpaper"]) || "Definir como wallpaper")
            actionEnabled: !AppState.wallpaperApplyRunning
            visible: menuRoot.isWallpaperImageTarget
            onTriggered: menuRoot.runSetAsWallpaper()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.restaurar"]) || "Restore")
            actionEnabled: true
            visible: AppState.inTrashView && !menuRoot.isBackgroundTarget
            onTriggered: menuRoot.runRestore()
        }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.propriedades"]) || "Properties")
            actionEnabled: true
            onTriggered: menuRoot.runShowProperties()
        }
        Common.ContextMenuDivider { visible: !menuRoot.isBackgroundTarget }
        Common.ContextMenuAction {
            label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.mover_para_lixeira"]) || "Move to Trash")
            actionEnabled: true
            visible: !menuRoot.isBackgroundTarget && !AppState.inTrashView
            destructive: true
            onTriggered: menuRoot.runDelete()
        }
    }

    Popup {
        id: compressionDialog
        anchors.centerIn: parent
        width: 430
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        visible: menuRoot.compressionDialogOpen

        background: Rectangle {
            radius: 14
            color: Theme.panel
            border.color: Theme.border
            border.width: 1
        }

        contentItem: Column {
            spacing: 12
            padding: 16

            Text {
                text: "Compress…"
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: menuRoot.compressionSources.length + " source(s): " + menuRoot.compressionSources.map(function(path) { return path.split("/").pop() }).join(", ")
                color: Theme.textSec
                elide: Text.ElideMiddle
            }

            TextField {
                id: compressionNameField
                width: parent.width
                text: menuRoot.compressionArchiveName
                color: Theme.text
                placeholderText: "Archive name"
                selectByMouse: true
                onTextChanged: {
                    if (menuRoot.compressionDialogOpen)
                        menuRoot.compressionArchiveName = text
                }
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.color: compressionNameField.activeFocus ? Theme.accent : Theme.border
                    border.width: 1
                }
            }

            Row {
                width: parent.width
                spacing: 8
                Text { text: "Format"; color: Theme.textSec; width: 100; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: compressionFormatBox
                    width: parent.width - 108
                    model: menuRoot.createCapabilities
                    textRole: "label"
                    onActivated: {
                        if (index >= 0 && index < menuRoot.createCapabilities.length)
                            menuRoot.compressionFormat = menuRoot.createCapabilities[index].id
                    }
                }
            }

            Row {
                width: parent.width
                visible: menuRoot.createCapabilities.length > 0
                    && menuRoot.createCapabilities.some(function(capability) {
                        return capability.id === menuRoot.compressionFormat && capability.profiles.length > 0
                    })
                spacing: 8
                Text { text: "Compression"; color: Theme.textSec; width: 100; anchors.verticalCenter: parent.verticalCenter }
                ComboBox {
                    id: compressionProfileBox
                    width: parent.width - 108
                    model: {
                        var capability = menuRoot.createCapabilities.filter(function(item) {
                            return item.id === menuRoot.compressionFormat
                        })[0]
                        return capability ? capability.profiles : []
                    }
                    currentIndex: model.indexOf(menuRoot.compressionProfile)
                    onActivated: menuRoot.compressionProfile = currentText
                }
            }

            Text {
                width: parent.width
                text: {
                    var capability = menuRoot.createCapabilities.filter(function(item) { return item.id === menuRoot.compressionFormat })[0]
                    return capability ? "Output: " + menuRoot.compressionArchiveName + "." + capability.extension : ""
                }
                color: Theme.textTer
                elide: Text.ElideMiddle
            }

            Row {
                spacing: 8
                DialogButton {
                    label: "Cancel"
                    onClicked: menuRoot.compressionDialogOpen = false
                }
                DialogButton {
                    label: "Compress"
                    emphasized: true
                    onClicked: menuRoot.runCompress()
                }
            }
        }

        onVisibleChanged: {
            if (visible) {
                compressionFormatBox.currentIndex = Math.max(0, menuRoot.createCapabilities.findIndex(function(item) { return item.id === menuRoot.compressionFormat }))
                compressionProfileBox.currentIndex = Math.max(0, compressionProfileBox.model.indexOf(menuRoot.compressionProfile))
            }
        }
    }

    FileDialog {
        id: extractionFolderDialog
        mode: "select_folder"
        dialogTitle: "Extract archive to…"
        onFileChosen: function(path) {
            if (path !== "" && menuRoot.pendingExtractionPath !== "")
                AppState.startArchiveExtractionTo(menuRoot.pendingExtractionPath, path)
            menuRoot.pendingExtractionPath = ""
        }
    }

    component DialogButton: Button {
        property string label: ""
        property bool emphasized: false
        property bool danger: false
        text: label
        implicitHeight: 34
        leftPadding: 14
        rightPadding: 14
        contentItem: Text {
            text: parent.label
            color: parent.danger ? "#ffb3b3" : (parent.emphasized ? Theme.accent : Theme.text)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 8
            color: parent.emphasized ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18) : Qt.rgba(1, 1, 1, 0.06)
            border.color: parent.emphasized ? Theme.accent : Theme.border
            border.width: 1
        }
    }

    Connections {
        target: AppState
        function onFilesystemActionFinished(requestId, operation, ok, data, error) {
            if (operation === "create-folder" && requestId === menuRoot.createFolderRequestId) {
                menuRoot.creatingFolder = false
                if (ok)
                    AppState.refreshCurrentFolder()
                return
            }
            if (operation === "rename" && requestId === menuRoot.renameRequestId) {
                menuRoot.renamingItem = false
                if (ok) {
                    AppState.refreshCurrentFolder()
                    if (AppState.selectedFile === itemPath.split('/').pop())
                        AppState.selectedFile = pendingRenameName
                }
            }
        }
    }

    Common.OpenWithMenu {
        id: openWithMenu
        anchors.fill: parent
    }

    Window {
        id: propertiesWin
        title: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.propriedades"]) || "Properties")
        width: 440
        minimumWidth: 380
        minimumHeight: 300
        color: "#1c1c1e"
        flags: Qt.Window | Qt.Dialog

        property string targetPath: ""
        property bool targetIsDir: false
        property bool isMulti: false
        property var targetPaths: []
        property bool isLoading: false
        property string errorText: ""
        property string propType: ""
        property string propSize: ""
        property string propModified: ""
        property string propAccessed: ""
        property string propPerms: ""
        property string propContains: ""
        property int propertiesRequestId: 0
        property int metricsRequestId: 0

        readonly property bool isImageFile: {
            if (isMulti) return false
            var ext = targetPath.split(".").pop().toLowerCase()
            return ["jpg","jpeg","png","gif","bmp","webp","svg"].indexOf(ext) !== -1
        }

        height: isImageFile ? 520 : 340

        function fmtDate(epochSeconds) {
            var v = Number(epochSeconds)
            if (!isFinite(v) || v <= 0) return "--"
            return Qt.formatDateTime(new Date(v * 1000), "dd/MM/yyyy  HH:mm")
        }

        function metricsText(key, fallback, values) {
            var text = (AstreaI18n.I18n.messages && AstreaI18n.I18n.messages[key]) || fallback
            for (var i = 0; i < values.length; ++i)
                text = text.replace("%" + (i + 1), values[i])
            return text
        }

        function applyMetricsState() {
            if (metricsRequestId === 0 || metricsRequestId !== AppState.directoryMetricsRequestId)
                return
            var files = Number(AppState.directoryMetricsFileCount)
            var folders = Number(AppState.directoryMetricsDirectoryCount)
            var counts = metricsText(
                "apps.explorer.properties.text.files_and_folders",
                "%1 files, %2 folders",
                [files, folders])
            var state = AppState.directoryMetricsState
            if (state === "running") {
                var liveSize = AppState.formatSize(Number(AppState.directoryMetricsBytes))
                propertiesWin.propSize = Number(AppState.directoryMetricsBytes) > 0
                    ? liveSize + " (" + metricsText("apps.explorer.properties.text.calculating", "Calculating…", []) + ")"
                    : metricsText("apps.explorer.properties.text.calculating", "Calculating…", [])
                propertiesWin.propContains = counts
                propertiesWin.errorText = ""
                return
            }
            if (state === "success") {
                propertiesWin.propSize = AppState.formatSize(Number(AppState.directoryMetricsBytes))
                propertiesWin.propContains = counts
                propertiesWin.errorText = ""
                return
            }
            if (state === "partial") {
                propertiesWin.propSize = metricsText(
                    "apps.explorer.properties.text.at_least",
                    "At least %1",
                    [AppState.formatSize(Number(AppState.directoryMetricsBytes))])
                propertiesWin.propContains = counts
                propertiesWin.errorText = AppState.directoryMetricsError
                    || metricsText("apps.explorer.properties.text.some_items_unreadable", "Some items could not be read.", [])
                return
            }
            if (state === "failed") {
                propertiesWin.errorText = AppState.directoryMetricsError
                    || metricsText("apps.explorer.properties.text.metrics_failed", "Could not calculate folder size.", [])
            }
        }

        function markMetricsStartFailure() {
            propertiesWin.metricsRequestId = 0
            propertiesWin.propSize = metricsText(
                "apps.explorer.properties.text.metrics_failed",
                "Could not calculate folder size.",
                [])
            propertiesWin.propContains = ""
            propertiesWin.errorText = AppState.directoryMetricsError
                || metricsText("apps.explorer.properties.text.metrics_failed", "Could not calculate folder size.", [])
            propertiesWin.isLoading = false
        }

        function handleMetricsSuperseded(requestId) {
            if (requestId !== propertiesWin.metricsRequestId)
                return
            propertiesWin.metricsRequestId = 0
            propertiesWin.propSize = metricsText(
                "apps.explorer.properties.text.metrics_replaced",
                "Calculation replaced.",
                [])
            propertiesWin.propContains = ""
            propertiesWin.errorText = propertiesWin.propSize
            propertiesWin.isLoading = false
        }

        onVisibilityChanged: {
            if (!visible) {
                if (propertiesWin.metricsRequestId !== 0)
                    AppState.cancelDirectoryMetrics(propertiesWin.metricsRequestId)
                propertiesWin.metricsRequestId = 0
                propertiesWin.propertiesRequestId = 0
                return
            }
            isLoading = true
            errorText = ""
            propType = propertiesWin.isMulti
                ? metricsText("apps.explorer.properties.text.multiple_items", "Multiple items", [])
                : ""
            propSize = (propertiesWin.isMulti || propertiesWin.targetIsDir)
                ? metricsText("apps.explorer.properties.text.calculating", "Calculating…", [])
                : ""
            propModified = "Carregando..."
            propAccessed = "Carregando..."
            propPerms = "Carregando..."
            propContains = (propertiesWin.isMulti || propertiesWin.targetIsDir)
                ? metricsText("apps.explorer.properties.text.calculating", "Calculating…", [])
                : ""
            propertiesWin.propertiesRequestId = propertiesWin.isMulti
                ? 0
                : AppState.requestProperties(propertiesWin.targetPath)
            propertiesWin.metricsRequestId = (propertiesWin.isMulti || propertiesWin.targetIsDir)
                ? AppState.requestDirectoryMetrics(propertiesWin.isMulti
                    ? propertiesWin.targetPaths
                    : [propertiesWin.targetPath])
                : 0
            if ((propertiesWin.isMulti || propertiesWin.targetIsDir)
                && propertiesWin.metricsRequestId === 0)
                propertiesWin.markMetricsStartFailure()
        }

        Rectangle {
            id: propTitleBar
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 44
            color: "#252527"

            Text {
                anchors {
                    left: parent.left; leftMargin: 16
                    right: parent.right; rightMargin: 16
                    verticalCenter: parent.verticalCenter
                }
                text: propertiesWin.isMulti
                    ? (propertiesWin.targetPaths.length + " itens selecionados")
                    : (propertiesWin.targetPath.split("/").pop() || propertiesWin.targetPath)
                color: "#f2f2f7"
                font { pixelSize: 13; weight: Font.DemiBold }
                elide: Text.ElideMiddle
            }

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: "#2c2c2c"
            }
        }

        Rectangle {
            id: propPreview
            anchors { top: propTitleBar.bottom; left: parent.left; right: parent.right }
            height: propertiesWin.isImageFile ? 160 : 0
            visible: propertiesWin.isImageFile
            color: "#141416"

            Image {
                anchors { fill: parent; margins: 8 }
                source: propertiesWin.visible && propertiesWin.isImageFile
                    ? AppState.fileUrlForPath(propertiesWin.targetPath) : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                asynchronous: true
                cache: false
            }

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: "#2c2c2c"
            }
        }

        Item {
            anchors {
                top: propPreview.bottom
                left: parent.left; right: parent.right
                bottom: propFooter.top
            }

            Column {
                id: infoCol
                anchors {
                    top: parent.top; topMargin: 16
                    left: parent.left; leftMargin: 16
                    right: parent.right; rightMargin: 16
                }
                spacing: 10

                Repeater {
                    id: infoRepeater
                    model: {
                        var rows = [
                            { lbl: propertiesWin.isMulti ? "Local" : "Caminho", val: propertiesWin.isMulti ? AppState.currentPath : propertiesWin.targetPath },
                            { lbl: "Tipo", val: propertiesWin.propType },
                            { lbl: "Tamanho", val: propertiesWin.propSize }
                        ]
                        if (propertiesWin.targetIsDir || (propertiesWin.isMulti && propertiesWin.targetPaths.length > 0))
                            rows.push({ lbl: propertiesWin.isMulti ? "Itens" : "Conteudo", val: propertiesWin.propContains })
                        if (!propertiesWin.isMulti) {
                            rows.push({ lbl: "Modificado", val: propertiesWin.propModified })
                            rows.push({ lbl: "Permissoes", val: propertiesWin.propPerms })
                        }
                        return rows
                    }

                    Row {
                        width: infoCol.width
                        spacing: 12

                        Text {
                            text: modelData.lbl
                            color: "#8e8e93"
                            font.pixelSize: 12
                            width: 90
                        }

                        Text {
                            text: modelData.val
                            color: "#f2f2f7"
                            font.pixelSize: 12
                            width: infoCol.width - 90 - 12
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Text {
                    visible: propertiesWin.errorText !== ""
                    text: propertiesWin.errorText
                    color: "#ff6b6b"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: infoCol.width
                }
            }
        }

        Rectangle {
            id: propFooter
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 48
            color: "#252527"

            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1; color: "#2c2c2c"
            }

            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 14 }
                width: 80; height: 30; radius: 7
                color: propCloseMouse.containsMouse ? "#3a3a3c" : "#2c2c2e"
                border.color: "#48484a"; border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.text.fechar"]) || "Close")
                    color: "#f2f2f7"
                    font.pixelSize: 13
                }

                MouseArea {
                    id: propCloseMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: propertiesWin.close()
                }
            }
        }

        Connections {
            target: AppState
            function onFilesystemActionFinished(requestId, operation, ok, data, error) {
                if (operation !== "properties" || requestId !== propertiesWin.propertiesRequestId)
                    return
                if (!ok) {
                    propertiesWin.isLoading = false
                    propertiesWin.errorText = error || "Falha ao consultar propriedades."
                    return
                }
                if (!propertiesWin.isMulti
                    && !propertiesWin.targetIsDir
                    && propertiesWin.metricsRequestId === 0)
                    propertiesWin.errorText = ""
                propertiesWin.propType = data.type || (propertiesWin.targetIsDir ? "Pasta" : "Arquivo")
                if (!propertiesWin.targetIsDir && data.sizeKnown !== false)
                    propertiesWin.propSize = AppState.formatSize(Number(data.size || 0))
                propertiesWin.propModified = propertiesWin.fmtDate(Number(data.modifiedMs || 0) / 1000)
                propertiesWin.propAccessed = propertiesWin.fmtDate(Number(data.accessedMs || 0) / 1000)
                propertiesWin.propPerms = data.permissions || "--"
                if (propertiesWin.targetIsDir && propertiesWin.metricsRequestId === 0
                    && data.containsKnown === true) {
                    var count = Number(data.contains || 0)
                    propertiesWin.propContains = count + (count === 1 ? " item" : " itens")
                }
                propertiesWin.isLoading = false
            }
            function onDirectoryMetricsUpdated(requestId) {
                if (requestId === propertiesWin.metricsRequestId)
                    propertiesWin.applyMetricsState()
            }
            function onDirectoryMetricsSuperseded(requestId) {
                propertiesWin.handleMetricsSuperseded(requestId)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: creatingFolder
        color: Qt.rgba(0, 0, 0, 0.5)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: function(mouse) {
                mouse.accepted = true
                menuRoot.creatingFolder = false
            }
        }

        Rectangle {
            id: createDialog
            width: 320
            anchors.centerIn: parent
            height: createCol.implicitHeight + 24
            radius: 10
            color: "#1e1e20"
            border.color: "#3a3a3c"; border.width: 1

            Column {
                id: createCol
                anchors { fill: parent; margins: 16 }
                spacing: 12

                Text {
                    text: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.nova_pasta"]) || "New Folder")
                    color: "#f2f2f7"
                    font { pixelSize: 14; weight: Font.DemiBold }
                }

                TextField {
                    id: nameField
                    width: parent.width
                    text: menuRoot.pendingFolderName
                    color: "#f2f2f7"
                    placeholderText: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.placeholderText.nome_da_pasta"]) || "Folder name")
                    placeholderTextColor: "#636366"
                    selectByMouse: true
                    font.pixelSize: 13
                    background: Rectangle {
                        radius: 7; color: "#2c2c2e"
                        border.color: nameField.activeFocus ? "#636366" : "#3a3a3c"; border.width: 1
                    }
                    onTextChanged: menuRoot.pendingFolderName = text
                    onAccepted: menuRoot.confirmCreateFolder()
                }

                Row {
                    spacing: 8
                    FlatButton { id: cancelCreate; label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.cancelar"]) || "Cancel"); onClicked: menuRoot.creatingFolder = false }
                    FlatButton { label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.criar"]) || "Create"); primary: true; onClicked: menuRoot.confirmCreateFolder() }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: renamingItem
        color: Qt.rgba(0, 0, 0, 0.5)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: function(mouse) {
                mouse.accepted = true
                menuRoot.renamingItem = false
            }
        }

        Rectangle {
            width: 320
            anchors.centerIn: parent
            height: renameCol.implicitHeight + 24
            radius: 10
            color: "#1e1e20"
            border.color: "#3a3a3c"; border.width: 1

            Column {
                id: renameCol
                anchors { fill: parent; margins: 16 }
                spacing: 12

                Text {
                    text: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.renomear"]) || "Rename")
                    color: "#f2f2f7"
                    font { pixelSize: 14; weight: Font.DemiBold }
                }

                TextField {
                    id: renameField
                    width: parent.width
                    text: menuRoot.pendingRenameName
                    color: "#f2f2f7"
                    placeholderText: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.placeholderText.novo_nome"]) || "New name")
                    placeholderTextColor: "#636366"
                    selectByMouse: true
                    font.pixelSize: 13
                    background: Rectangle {
                        radius: 7; color: "#2c2c2e"
                        border.color: renameField.activeFocus ? "#636366" : "#3a3a3c"; border.width: 1
                    }
                    onTextChanged: menuRoot.pendingRenameName = text
                    onAccepted: menuRoot.confirmRename()
                }

                Row {
                    spacing: 8
                    FlatButton { label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.cancelar"]) || "Cancel"); onClicked: menuRoot.renamingItem = false }
                    FlatButton { label: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.components.common.file_context_menu.label.renomear"]) || "Rename"); primary: true; onClicked: menuRoot.confirmRename() }
                }
            }
        }
    }

    component FlatButton: Rectangle {
        id: fbRoot
        property string label: ""
        property bool primary: false
        signal clicked()

        width: 88; height: 30; radius: 7
        color: fbMouse.containsMouse
            ? "#3a3a3c"
            : (primary ? "#2c2c2e" : "#232325")
        border.color: "#3a3a3c"; border.width: 1
        Behavior on color { ColorAnimation { duration: 60 } }

        Text {
            anchors.centerIn: parent
            text: fbRoot.label
            color: "#f2f2f7"
            font { pixelSize: 13; weight: fbRoot.primary ? Font.DemiBold : Font.Normal }
        }

        MouseArea {
            id: fbMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: fbRoot.clicked()
        }
    }

}
