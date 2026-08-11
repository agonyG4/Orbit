import QtQuick 2.15
import QtQuick.Controls 2.15
import "." as Finder
import "AstreaI18n" as AstreaI18n

ApplicationWindow {
    id: root
    visible: true
    width: 1080
    height: 720
    color: "transparent"
    title: ((AstreaI18n.I18n.messages && AstreaI18n.I18n.messages["apps.explorer.portal_dialog.title.astrea_file_dialog"]) || "Astrea File Dialog")

    property var options: ({})
    property string resultFile: ""
    property string pendingResultJson: ""
    readonly property string portalOptionsJson: astreaPortalOptionsJson || "{}"

    function parseOptions() {
        var raw = root.portalOptionsJson
        if (!raw)
            return {}

        try {
            return JSON.parse(raw)
        } catch (error) {
            console.error("Failed to parse Astrea file dialog options:", error)
            return {}
        }
    }

    function emitResult(payload) {
        console.log("__ASTREA_FILE_DIALOG__" + JSON.stringify(payload))
        console.log("__BENCH_FILE_DIALOG__" + JSON.stringify(payload))
    }

    property bool resultSent: false

    function emitResultOnce(payload) {
        if (resultSent)
            return
        resultSent = true
        pendingResultJson = JSON.stringify(payload)
        var selectedPaths = []
        if (payload.files) {
            for (var i = 0; i < payload.files.length; i++)
                if (payload.files[i].filePath) selectedPaths.push(payload.files[i].filePath)
        } else if (payload.filePath) {
            selectedPaths.push(payload.filePath)
        }
        if (typeof NativePortalController !== "undefined") {
            NativePortalController.setSelectedPaths(selectedPaths)
            if (payload.accepted) NativePortalController.accept()
            else NativePortalController.reject()
        }
        if (resultFile !== "") {
            if (AppState.writePortalResult(root.pendingResultJson))
                Qt.quit()
        } else {
            emitResult(payload)
            Qt.callLater(function() {
                Qt.quit()
            })
        }
    }

    Component.onCompleted: {
        Qt.application.name = "Explorer"
        Qt.application.organization = "agony"
        Qt.application.domain = "local"
        options = parseOptions()
        resultFile = astreaPortalResultFile || ""
        dialog.mode = options.mode || "open_file"
        dialog.dialogTitle = options.title || dialog.dialogTitle
        dialog.acceptLabel = options.acceptLabel || dialog.acceptLabel
        dialog.startFolder = options.startFolder || dialog.startFolder
        dialog.selectedName = options.currentName || ""
        dialog.nameFilters = options.filters || []
        dialog.initialViewMode = options.viewMode || "icon"
        dialog.allowMultiple = Boolean(options.multiple)
        // Delay opening the popup until the top-level window is established.
        Qt.callLater(function() {
            dialog.openDialog()
        })
    }

    Finder.FileDialog {
        id: dialog

        onFileChosen: function(filePath, fileUrl) {
            root.emitResultOnce({
                accepted: true,
                filePath: filePath,
                fileUrl: fileUrl
            })
        }

        onFilesChosen: function(files) {
            var selected = files || []
            var first = selected.length > 0 ? selected[0] : {}
            root.emitResultOnce({
                accepted: true,
                files: selected,
                filePath: first.filePath || "",
                fileUrl: first.fileUrl || ""
            })
        }

        onRejected: {
            root.emitResultOnce({ accepted: false })
        }
    }

    onClosing: function(close) {
        root.emitResultOnce({ accepted: false })
    }
}
