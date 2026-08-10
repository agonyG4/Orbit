import Quickshell
import Quickshell.Io
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import "components/common" as Common
import "AstreaComponents" as Astrea

ApplicationWindow {
    id: root
    visible: true
    width: 1100
    height: 720
    minimumWidth: 700
    minimumHeight: 420
    title: (dirty ? "*" : "") + displayName + " - Notepad"
    color: Astrea.Theme.windowBackground

    property string currentPath: ""
    property string displayName: "Untitled.md"
    property string lastSavedSnapshot: ""
    property bool dirty: false
    property bool loadingFile: false
    property string pendingAction: ""
    property string pendingDialogMode: ""
    property string pendingDialogResultPath: ""
    property string pendingDialogStdout: ""
    property string pendingOpenPath: ""
    property string pendingSavePath: ""
    property string pendingSaveSnapshot: ""
    property string statusText: "Pronto"
    property string visibleText: ""
    property int wordCount: 0
    property bool formatBoldActive: false
    property bool formatItalicActive: false
    property bool formatUnderlineActive: false
    property bool formatH1Active: false
    property bool formatH2Active: false
    property var textColors: [
        { "name": "Azul", "value": "#0a84ff" },
        { "name": "Verde", "value": "#30d158" },
        { "name": "Amarelo", "value": "#ffd60a" },
        { "name": "Vermelho", "value": "#ff453a" },
        { "name": "Rosa", "value": "#ff2d55" },
        { "name": "Branco", "value": "#f2f2f7" }
    ]
    property string searchText: ""

    function markSavedSnapshot(snapshot) {
        lastSavedSnapshot = snapshot !== undefined ? snapshot : serializedDocument()
        dirty = serializedDocument() !== lastSavedSnapshot
    }

    function requestDestructiveAction(action) {
        if (!dirty) {
            executeAction(action)
            return
        }
        pendingAction = action
        showDirtyConfirmMenu()
    }

    function executeAction(action) {
        if (action === "new")
            newDocument()
        else if (action === "open")
            openSystemFileDialog("open_file")
        else if (action === "close")
            Qt.quit()
    }

    function resumePendingAction() {
        var action = pendingAction
        pendingAction = ""
        if (action)
            executeAction(action)
    }

    function showDirtyConfirmMenu() {
        closeConfirmMenu.menuX = Math.round((root.width - closeConfirmMenu.menuWidth) / 2)
        closeConfirmMenu.menuY = Math.round((root.height - closeConfirmMenu.height) / 2)
        closeConfirmMenu.menuVisible = true
    }

    function closeEditorMenu() {
        editorContextMenu.menuVisible = false
        colorMenu.menuVisible = false
    }

    function showEditorMenu(localX, localY) {
        var mapped = editor.mapToItem(root.contentItem, localX, localY)
        editorContextMenu.menuX = Math.min(mapped.x, root.width - editorContextMenu.menuWidth - 10)
        editorContextMenu.menuY = Math.min(mapped.y, root.height - editorContextMenu.height - 10)
        editorContextMenu.menuVisible = true
    }

    function showCloseMenu() {
        requestDestructiveAction("close")
    }

    function documentStartFolder() {
        if (currentPath) {
            var slashIndex = currentPath.lastIndexOf("/")
            if (slashIndex > 0)
                return currentPath.slice(0, slashIndex)
        }
        return StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
    }

    function dialogStartFolder(mode) {
        if (mode === "insert_image")
            return StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        return documentStartFolder()
    }

    function openSystemFileDialog(mode) {
        pendingDialogMode = mode
        pendingDialogStdout = ""
        pendingDialogResultPath = "/tmp/notepad_file_dialog_" + Date.now() + "_" + Math.floor(Math.random() * 1000000) + ".json"

        var options = {
            mode: mode,
            title: mode === "save_file" ? "Salvar Nota"
                 : mode === "insert_image" ? "Inserir imagem"
                                            : "Abrir Nota",
            startFolder: dialogStartFolder(mode),
            acceptLabel: mode === "save_file" ? "Salvar"
                       : mode === "insert_image" ? "Inserir"
                                                  : "Abrir",
            currentName: currentPath ? markdownNameFromPath(currentPath) : "Untitled.md",
            filters: mode === "save_file"
                ? ["Markdown (*.md)"]
                : mode === "insert_image"
                    ? ["Imagens (*.png *.jpg *.jpeg *.gif *.webp *.svg)", "Todos os arquivos (*)"]
                    : ["Markdown (*.md)"]
        }

        portalDialogProcess.environment = {
            "BENCH_FILE_DIALOG_OPTIONS": JSON.stringify(options),
            "BENCH_FILE_DIALOG_RESULT_FILE": pendingDialogResultPath
        }
        portalDialogProcess.running = false
        portalDialogProcess.running = true
    }

    function parsePortalPayload(raw) {
        if (!raw)
            return null

        var prefix = "__BENCH_FILE_DIALOG__"
        var prefixIndex = raw.lastIndexOf(prefix)
        if (prefixIndex >= 0)
            raw = raw.slice(prefixIndex + prefix.length)

        raw = raw.trim()
        if (!raw)
            return null

        try {
            return JSON.parse(raw)
        } catch (error) {
            console.error("Failed to parse file dialog result:", error, raw)
            return null
        }
    }

    function handlePortalPayload(payload) {
        if (!payload || !payload.accepted) {
            statusText = "Dialogo cancelado"
            if (pendingDialogMode === "save_file")
                pendingAction = ""
            return
        }

        if (pendingDialogMode === "open_file")
            openDocument(payload.filePath)
        else if (pendingDialogMode === "save_file")
            saveDocument(payload.filePath)
        else if (pendingDialogMode === "insert_image")
            insertImage(payload.filePath, payload.fileUrl)
    }

    function pathFromUrl(url) {
        var raw = url.toString()
        if (raw.indexOf("file://") === 0)
            raw = raw.slice(7)
        return decodeURIComponent(raw)
    }

    function nameFromPath(path) {
        if (!path)
            return "Untitled.md"
        var parts = path.split("/")
        return parts[parts.length - 1] || "Untitled.md"
    }

    function markdownNameFromPath(path) {
        var name = nameFromPath(path)
        return name.replace(/\.[^\/.]+$/, "") + ".md"
    }

    function ensureNotePath(path) {
        if (!path)
            return path
        return /\.[^\/.]+$/.test(path) ? path.replace(/\.[^\/.]+$/, ".md") : path + ".md"
    }

    function escapeHtml(value) {
        return (value || "")
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
    }

    function decodeHtml(value) {
        return (value || "")
            .replace(/&nbsp;/g, " ")
            .replace(/&quot;/g, '"')
            .replace(/&#39;/g, "'")
            .replace(/&lt;/g, "<")
            .replace(/&gt;/g, ">")
            .replace(/&amp;/g, "&")
    }

    function markdownFromHtml(html) {
        return decodeHtml((html || "")
            .replace(/<h1[^>]*>/gi, "# ")
            .replace(/<\/h1>/gi, "\n\n")
            .replace(/<h2[^>]*>/gi, "## ")
            .replace(/<\/h2>/gi, "\n\n")
            .replace(/<(b|strong)[^>]*>/gi, "**")
            .replace(/<\/(b|strong)>/gi, "**")
            .replace(/<(i|em)[^>]*>/gi, "*")
            .replace(/<\/(i|em)>/gi, "*")
            .replace(/<u[^>]*>/gi, "")
            .replace(/<\/u>/gi, "")
            .replace(/<img[^>]*src=[\"']([^\"']+)[\"'][^>]*alt=[\"']([^\"']*)[\"'][^>]*>/gi, "![$2]($1)")
            .replace(/<img[^>]*src=[\"']([^\"']+)[\"'][^>]*>/gi, "![]($1)")
            .replace(/<br\s*\/?\s*>/gi, "\n")
            .replace(/<\/p>/gi, "\n\n")
            .replace(/<[^>]+>/g, "")
            .replace(/\n{3,}/g, "\n\n")
            .trim()) + "\n"
    }

    function serializedDocument() {
        if (!editor || editor.length === 0)
            return ""
        return markdownFromHtml(editor.getFormattedText(0, editor.length))
    }

    function formattingProbeHtml() {
        if (!editor || editor.length === 0)
            return ""

        var start = editor.selectionStart
        var end = editor.selectionEnd
        if (start === end) {
            start = Math.max(0, editor.cursorPosition - 1)
            end = Math.min(editor.length, editor.cursorPosition + 1)
        }
        if (start >= end)
            return ""
        return editor.getFormattedText(start, end)
    }

    function refreshFormattingState() {
        var html = formattingProbeHtml().toLowerCase()
        formatBoldActive = /<(b|strong)(\s|>)/.test(html)
        formatItalicActive = /<(i|em)(\s|>)/.test(html)
        formatUnderlineActive = /<u(\s|>)/.test(html)
        formatH1Active = /<h1(\s|>)/.test(html)
        formatH2Active = /<h2(\s|>)/.test(html)
    }

    function replaceSelection(fragment) {
        var start = editor.selectionStart
        var end = editor.selectionEnd
        if (start === end) {
            editor.insert(editor.cursorPosition, fragment)
            return
        }

        editor.remove(start, end)
        editor.insert(start, fragment)
    }

    function wrapSelection(prefix, suffix, placeholder) {
        if (editor.selectionStart === editor.selectionEnd) {
            statusText = "Selecione texto para formatar"
            editor.forceActiveFocus()
            return false
        }

        var selected = escapeHtml(editor.selectedText || placeholder)
        var nextText = prefix + selected + suffix
        replaceSelection(nextText)
        editor.forceActiveFocus()
        formattingStateTimer.restart()
        return true
    }

    function applyInlineFormat(tag, placeholder, message) {
        if (wrapSelection("<" + tag + ">", "</" + tag + ">", placeholder))
            statusText = message
    }

    function insertHeading(level) {
        if (editor.selectionStart === editor.selectionEnd) {
            statusText = "Selecione texto para transformar em titulo"
            editor.forceActiveFocus()
            return
        }

        var selected = escapeHtml(editor.selectedText || "Titulo")
        var size = level === 1 ? 30 : 22
        var html = "<h" + level + " style=\"font-size:" + size + "px; font-weight:700; margin:0 0 10px 0; color:" + Astrea.Theme.textPrimary + ";\">" + selected + "</h" + level + "><p><br/></p>"
        replaceSelection(html)
        statusText = level === 1 ? "Titulo inserido" : "Subtitulo inserido"
        editor.forceActiveFocus()
        formattingStateTimer.restart()
    }

    function applyColor(colorValue) {
        if (wrapSelection("<span style=\"color:" + colorValue + ";\">", "</span>", "texto colorido"))
            statusText = "Cor aplicada"
    }

    function insertImage(path, fileUrl) {
        var name = nameFromPath(path)
        var url = fileUrl || ("file://" + path)
        replaceSelection("<p><img src=\"" + escapeHtml(url) + "\" alt=\"" + escapeHtml(name) + "\" width=\"520\" /></p><p><br/></p>")
        statusText = "Imagem inserida"
        editor.forceActiveFocus()
    }

    function newDocument() {
        loadingFile = true
        currentPath = ""
        displayName = "Untitled.md"
        editor.text = "<h1 style=\"font-size:30px; font-weight:700; margin:0 0 10px 0; color:" + Astrea.Theme.textPrimary + ";\">Sem Titulo</h1><p><br/></p>"
        markSavedSnapshot()
        loadingFile = false
        refreshTextStats()
        statusText = "Novo documento"
        editor.forceActiveFocus()
        refreshNoteList()
    }

    function openDocument(path) {
        loadingFile = true
        pendingOpenPath = path
        fileView.path = path
        fileView.reload()
    }

    function saveDocument(path) {
        var targetPath = ensureNotePath(path)
        if (!targetPath) {
            openSystemFileDialog("save_file")
            return
        }

        var snapshot = serializedDocument()
        pendingSavePath = targetPath
        pendingSaveSnapshot = snapshot
        fileView.path = targetPath
        fileView.setText(snapshot)
    }

    function refreshNoteList() {
        noteListModel.clear()
        noteListModel.append({
            title: root.displayName,
            path: root.currentPath,
            date: "Agora",
            current: true
        })
    }

    function refreshTextStats() {
        visibleText = editor.getText(0, editor.length)
        var trimmed = visibleText.trim()
        wordCount = trimmed.length === 0 ? 0 : trimmed.split(/\s+/).length
    }

    Component.onCompleted: {
        Qt.application.name = "notepad"
        Qt.application.organization = "agony"
        Qt.application.domain = "local"
        newDocument()
    }

    Timer {
        id: editorStatsTimer
        interval: 250
        repeat: false
        onTriggered: root.refreshTextStats()
    }

    Timer {
        id: dirtyCheckTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (!root.loadingFile)
                root.dirty = root.serializedDocument() !== root.lastSavedSnapshot
        }
    }

    Timer {
        id: formattingStateTimer
        interval: 80
        repeat: false
        onTriggered: root.refreshFormattingState()
    }

    onClosing: function(close) {
        if (dirty) {
            close.accepted = false
            showCloseMenu()
        } else {
            Qt.quit()
        }
    }

    FileView {
        id: fileView
        path: root.currentPath
        blockLoading: true
        blockWrites: true
        atomicWrites: true
        printErrors: true

        onLoaded: {
            if (!root.loadingFile)
                return
            var loadedText = text()
            editor.text = loadedText
            root.currentPath = root.pendingOpenPath
            root.displayName = root.nameFromPath(root.pendingOpenPath)
            root.pendingOpenPath = ""
            root.markSavedSnapshot()
            root.loadingFile = false
            root.refreshTextStats()
            root.statusText = "Aberto: " + root.displayName
            editor.forceActiveFocus()
            root.refreshNoteList()
        }

        onLoadFailed: function(error) {
            root.loadingFile = false
            root.pendingOpenPath = ""
            root.statusText = "Nao foi possivel abrir o arquivo"
        }

        onSaved: {
            root.currentPath = root.pendingSavePath
            root.displayName = root.nameFromPath(root.pendingSavePath)
            root.markSavedSnapshot(root.pendingSaveSnapshot)
            root.pendingSavePath = ""
            root.pendingSaveSnapshot = ""
            root.statusText = "Salvo: " + root.displayName
            root.refreshNoteList()
            root.resumePendingAction()
        }

        onSaveFailed: function(error) {
            root.pendingAction = ""
            root.pendingSavePath = ""
            root.pendingSaveSnapshot = ""
            root.statusText = "Nao foi possivel salvar o arquivo"
        }
    }

    FileView {
        id: portalDialogResult
        path: root.pendingDialogResultPath
        blockLoading: true
        printErrors: false
    }

    Process {
        id: portalDialogProcess
        command: ["/usr/bin/qs", "-p", "/home/agony/.local/share/Astrea/Apps/Explorer/PortalDialog.qml"]
        running: false

        stdout: SplitParser {
            onRead: function(data) {
                root.pendingDialogStdout += data
            }
        }

        onExited: function(exitCode, exitStatus) {
            var payload = root.parsePortalPayload(root.pendingDialogStdout)
            root.handlePortalPayload(payload)
        }
    }

    Shortcut { sequences: ["Ctrl+N"]; onActivated: root.requestDestructiveAction("new") }
    Shortcut { sequences: ["Ctrl+O"]; onActivated: root.requestDestructiveAction("open") }
    Shortcut { sequences: ["Ctrl+S"]; onActivated: root.saveDocument(root.currentPath) }
    Shortcut { sequences: ["Ctrl+Shift+S"]; onActivated: root.openSystemFileDialog("save_file") }
    Shortcut { sequences: ["Ctrl+Q"]; onActivated: root.close() }
    Shortcut { sequences: ["Ctrl+B"]; onActivated: root.applyInlineFormat("b", "negrito", "Negrito aplicado") }
    Shortcut { sequences: ["Ctrl+I"]; onActivated: root.applyInlineFormat("i", "italico", "Italico aplicado") }
    Shortcut { sequences: ["Ctrl+U"]; onActivated: root.applyInlineFormat("u", "sublinhado", "Sublinhado aplicado") }

    ListModel {
        id: noteListModel
    }

    background: Rectangle {
        color: Astrea.Theme.windowBackground
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Astrea.SidebarFrame {
            id: notesSidebar

            Layout.preferredWidth: 260
            Layout.fillHeight: true
            topMargin: 0
            bottomMargin: 0
            leftMargin: 0
            rightMargin: 0
            cornerRadius: 18
            contentTopPadding: Astrea.Theme.spacingMedium
            contentBottomPadding: Astrea.Theme.spacingLarge
            contentSpacing: Astrea.Theme.spacingTiny

            Item {
                width: parent.width - 28
                x: 14
                height: 44

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    Astrea.DisplayLabel {
                        Layout.fillWidth: true
                        text: "Notas"
                        font.pixelSize: 20
                        font.weight: Astrea.Theme.fontWeightBold
                    }

                    Astrea.Button {
                        text: ""
                        iconText: "+"
                        flat: true
                        controlWidth: 32
                        controlHeight: 32
                        onClicked: root.requestDestructiveAction("new")
                    }
                }
            }

            Astrea.SearchField {
                width: parent.width - 28
                height: 36
                x: 14
                placeholderText: "Buscar notas"
                onTextEdited: (value) => { root.searchText = value }
            }

            Item { width: 1; height: 6 }

            Rectangle {
                width: parent.width - 28
                x: 14
                height: 1
                color: Astrea.Theme.cardBorder
                opacity: 0.8
            }

            Item { width: 1; height: 6 }

            Repeater {
                model: noteListModel

                Astrea.NavItem {
                    required property string title
                    required property bool current

                    width: notesSidebar.width
                    label: title
                    sym: "\uf15c"
                    selected: current
                    onClicked: editor.forceActiveFocus()
                }
            }
        }

        // Editor
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Toolbar
            Rectangle {
                Layout.fillWidth: true
                height: 46
                color: Astrea.Theme.windowBackground
                border.color: Astrea.Theme.cardBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 6

                    ToolbarButton {
                        text: "+"
                        tooltip: "Novo"
                        onClicked: root.requestDestructiveAction("new")
                    }

                    ToolbarButton {
                        text: "Open"
                        width: 54
                        tooltip: "Abrir nota"
                        onClicked: root.requestDestructiveAction("open")
                    }

                    ToolbarButton {
                        text: "Save"
                        width: 54
                        enabled: root.dirty || root.currentPath === ""
                        tooltip: "Salvar"
                        active: root.dirty
                        onClicked: root.saveDocument(root.currentPath)
                    }

                    ToolbarButton {
                        text: "Save As"
                        width: 72
                        tooltip: "Salvar como"
                        onClicked: root.openSystemFileDialog("save_file")
                    }

                    Rectangle {
                        width: 1
                        height: 26
                        color: Astrea.Theme.cardBorder
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                    }

                    ToolbarButton {
                        text: "H1"
                        width: 38
                        tooltip: "Inserir titulo"
                        active: root.formatH1Active
                        onClicked: root.insertHeading(1)
                    }

                    ToolbarButton {
                        text: "H2"
                        width: 38
                        tooltip: "Inserir subtitulo"
                        active: root.formatH2Active
                        onClicked: root.insertHeading(2)
                    }

                    ToolbarButton {
                        text: "B"
                        tooltip: "Negrito"
                        font.weight: Font.Bold
                        active: root.formatBoldActive
                        onClicked: root.applyInlineFormat("b", "negrito", "Negrito aplicado")
                    }

                    ToolbarButton {
                        text: "I"
                        tooltip: "Italico"
                        font.italic: true
                        active: root.formatItalicActive
                        onClicked: root.applyInlineFormat("i", "italico", "Italico aplicado")
                    }

                    ToolbarButton {
                        text: "U"
                        tooltip: "Sublinhado"
                        font.underline: true
                        active: root.formatUnderlineActive
                        onClicked: root.applyInlineFormat("u", "sublinhado", "Sublinhado aplicado")
                    }

                    ToolbarButton {
                        text: "Cor"
                        width: 42
                        tooltip: "Cor do texto"
                        active: colorMenu.menuVisible
                        onClicked: {
                            var mapped = mapToItem(root.contentItem, 0, height + 6)
                            colorMenu.menuX = mapped.x
                            colorMenu.menuY = mapped.y
                            colorMenu.menuVisible = !colorMenu.menuVisible
                        }
                    }

                    ToolbarButton {
                        text: "Imagem"
                        width: 68
                        tooltip: "Inserir imagem"
                        onClicked: root.openSystemFileDialog("insert_image")
                    }

                    Rectangle {
                        width: 1
                        height: 26
                        color: Astrea.Theme.cardBorder
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 32
                        radius: Astrea.Theme.controlRadius
                        color: Astrea.Theme.cardBg
                        border.color: Astrea.Theme.cardBorder
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Label {
                                text: root.displayName
                                color: Astrea.Theme.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.currentPath || "Nova nota"
                                elide: Text.ElideMiddle
                                color: Astrea.Theme.textTertiary
                                font.pixelSize: 12
                            }
                        }
                    }

                    Rectangle {
                        width: dirtyBadgeText.implicitWidth + 18
                        height: 26
                        radius: Astrea.Theme.controlRadius
                        color: root.dirty ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.16) : Astrea.Theme.cardBg
                        border.color: root.dirty ? Astrea.Theme.accent : Astrea.Theme.cardBorder
                        border.width: 1

                        Label {
                            id: dirtyBadgeText
                            anchors.centerIn: parent
                            text: root.dirty ? "Editado" : "Salvo"
                            color: root.dirty ? Astrea.Theme.textPrimary : Astrea.Theme.textSecondary
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            // Editor area
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Astrea.Theme.windowBackground

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true

                    TextArea {
                        id: editor
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        persistentSelection: true
                        tabStopDistance: fontMetrics.advanceWidth("    ")
                        padding: 18
                        color: Astrea.Theme.textPrimary
                        selectedTextColor: Astrea.Theme.textPrimary
                        selectionColor: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.16)
                        placeholderText: "Comece a escrever..."
                        placeholderTextColor: Astrea.Theme.textTertiary
                        font.pixelSize: 15
                        background: Rectangle {
                            radius: Astrea.Theme.cardRadius
                            color: Astrea.Theme.cardBg
                            border.color: editor.activeFocus ? Astrea.Theme.accent : Astrea.Theme.cardBorder
                            border.width: 1
                        }

                        onTextChanged: {
                            editorStatsTimer.restart()
                            formattingStateTimer.restart()
                            if (!root.loadingFile) {
                                root.dirty = true
                                dirtyCheckTimer.restart()
                            }
                        }

                        onCursorPositionChanged: formattingStateTimer.restart()
                        onSelectionStartChanged: formattingStateTimer.restart()
                        onSelectionEndChanged: formattingStateTimer.restart()

                        Keys.onPressed: function(event) {
                            root.closeEditorMenu()
                            if (event.key === Qt.Key_Tab) {
                                insert(cursorPosition, "    ")
                                event.accepted = true
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: function(eventPoint, button) {
                                root.showEditorMenu(eventPoint.position.x, eventPoint.position.y)
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.closeEditorMenu()
                        }
                    }
                }
            }

            // Status bar
            Rectangle {
                Layout.fillWidth: true
                height: 28
                color: Astrea.Theme.cardBg
                border.color: Astrea.Theme.cardBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 14

                    Label {
                        text: root.statusText
                        color: Astrea.Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: root.wordCount + " palavras"
                        color: Astrea.Theme.textSecondary
                        font.pixelSize: 12
                    }

                    Label {
                        text: editor.length + " caracteres"
                        color: Astrea.Theme.textSecondary
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    FontMetrics {
        id: fontMetrics
        font: editor.font
    }

    Common.ContextMenuPopup {
        id: colorMenu
        z: 1001
        menuWidth: 170

        Repeater {
            model: root.textColors

            Common.ContextMenuAction {
                label: modelData.name
                onTriggered: {
                    root.applyColor(modelData.value)
                    colorMenu.menuVisible = false
                }
            }
        }
    }

    Common.ContextMenuPopup {
        id: closeConfirmMenu
        z: 1002
        menuWidth: 250

        Item {
            width: parent ? parent.width : 242
            height: 64

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: 12
                    rightMargin: 12
                }
                spacing: 5

                Text {
                    width: parent.width
                    text: "Documento com alteracoes"
                    color: Astrea.Theme.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.pendingAction === "new" ? "Salvar antes de criar uma nova nota?"
                         : root.pendingAction === "open" ? "Salvar antes de abrir outra nota?"
                                                        : "Salvar antes de fechar?"
                    color: Astrea.Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        Common.ContextMenuDivider {}

        Common.ContextMenuAction {
            label: "Salvar"
            actionEnabled: true
            onTriggered: {
                closeConfirmMenu.menuVisible = false
                root.saveDocument(root.currentPath)
            }
        }

        Common.ContextMenuAction {
            label: "Descartar"
            destructive: true
            actionEnabled: true
            onTriggered: {
                closeConfirmMenu.menuVisible = false
                if (root.pendingAction !== "open")
                    root.dirty = false
                root.resumePendingAction()
            }
        }

        Common.ContextMenuAction {
            label: "Cancelar"
            actionEnabled: true
            onTriggered: {
                root.pendingAction = ""
                closeConfirmMenu.menuVisible = false
            }
        }
    }

    Common.ContextMenuPopup {
        id: editorContextMenu
        z: 1000
        menuWidth: 190

        Common.ContextMenuAction {
            label: "Desfazer"
            actionEnabled: editor.canUndo
            onTriggered: {
                editor.undo()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Refazer"
            actionEnabled: editor.canRedo
            onTriggered: {
                editor.redo()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuDivider {}

        Common.ContextMenuAction {
            label: "Recortar"
            actionEnabled: editor.selectedText.length > 0
            onTriggered: {
                editor.cut()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Copiar"
            actionEnabled: editor.selectedText.length > 0
            onTriggered: {
                editor.copy()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Colar"
            actionEnabled: true
            onTriggered: {
                editor.paste()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuDivider {}

        Common.ContextMenuAction {
            label: "Inserir Titulo"
            actionEnabled: true
            onTriggered: {
                root.insertHeading(1)
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Negrito"
            actionEnabled: true
            onTriggered: {
                root.applyInlineFormat("b", "negrito", "Negrito aplicado")
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Italico"
            actionEnabled: true
            onTriggered: {
                root.applyInlineFormat("i", "italico", "Italico aplicado")
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Sublinhado"
            actionEnabled: true
            onTriggered: {
                root.applyInlineFormat("u", "sublinhado", "Sublinhado aplicado")
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Texto Azul"
            actionEnabled: true
            onTriggered: {
                root.applyColor("#0a84ff")
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Inserir Imagem"
            actionEnabled: true
            onTriggered: {
                root.openSystemFileDialog("insert_image")
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuDivider {}

        Common.ContextMenuAction {
            label: "Selecionar Tudo"
            actionEnabled: editor.length > 0
            onTriggered: {
                editor.selectAll()
                root.closeEditorMenu()
            }
        }

        Common.ContextMenuAction {
            label: "Limpar Selecao"
            actionEnabled: editor.selectedText.length > 0
            onTriggered: {
                editor.deselect()
                root.closeEditorMenu()
            }
        }
    }

    component ToolbarButton: ToolButton {
        property string tooltip: ""
        property bool active: false

        width: 32
        height: 32
        font.pixelSize: 13

        contentItem: Text {
            text: parent.text
            font: parent.font
            color: !parent.enabled ? Astrea.Theme.textTertiary
                 : parent.active ? Astrea.Theme.textPrimary
                 : parent.hovered ? Astrea.Theme.textPrimary
                                   : Astrea.Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Astrea.Theme.controlRadius
            color: !parent.enabled ? Qt.rgba(1, 1, 1, 0.025)
                 : parent.active ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.16)
                 : parent.down ? Qt.rgba(1, 1, 1, 0.12)
                 : parent.hovered ? Qt.rgba(1, 1, 1, 0.075)
                                  : "transparent"
            border.color: parent.active ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.36) : "transparent"
            border.width: parent.active ? 1 : 0

            Behavior on color {
                ColorAnimation { duration: Astrea.Theme.animationQuick }
            }

            Behavior on border.color {
                ColorAnimation { duration: Astrea.Theme.animationQuick }
            }
        }

        ToolTip.text: tooltip
        ToolTip.visible: tooltip !== "" && hovered
        ToolTip.delay: 500
    }
}
