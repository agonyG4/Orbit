import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../AstreaComponents" as Astrea

Rectangle {
    id: pane

    property var message: ({})
    property bool hasMessage: false
    property color softSurface: Astrea.Theme.cardBg
    signal replyRequested()
    signal starRequested(string messageId)
    signal archiveRequested()
    signal markReadRequested(bool read)
    signal moveToInboxRequested()
    signal trashRequested()
    signal loadImagesRequested()

    readonly property bool originalHtml: pane.hasHtmlBody() && pane.message.htmlRenderMode === "html"

    radius: Astrea.Theme.cardRadius
    color: Astrea.Theme.cardBg
    border.width: 1
    border.color: Astrea.Theme.cardBorder
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Astrea.Theme.spacingXLarge
        spacing: Astrea.Theme.spacingLarge
        visible: pane.hasMessage

        RowLayout {
            Layout.fillWidth: true
            spacing: Astrea.Theme.spacingLarge

            AvatarBubble {
                initials: pane.initials(pane.message.fromName || "Mail")
                tag: pane.message.tag || ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Astrea.DisplayLabel {
                    Layout.fillWidth: true
                    text: pane.message.subject || ""
                    textColor: Astrea.Theme.textPrimary
                    font.pixelSize: Astrea.Theme.fontSizeHeader
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Astrea.TextLabel {
                        text: pane.message.fromName || ""
                        textColor: Astrea.Theme.textPrimary
                        font.pixelSize: Astrea.Theme.fontSizeNormal
                        font.weight: Astrea.Theme.fontWeightDemiBold
                    }

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: pane.message.fromAddress ? "<" + pane.message.fromAddress + ">" : ""
                        textColor: Astrea.Theme.textSecondary
                        font.pixelSize: Astrea.Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }

                    Astrea.TextLabel {
                        text: pane.message.timestamp || ""
                        textColor: Astrea.Theme.textTertiary
                        font.pixelSize: Astrea.Theme.fontSizeSmall
                    }
                }
            }

            Astrea.Button {
                text: ""
                iconText: pane.message.starred ? "\uf005" : "\uf006"
                iconFontFamily: "JetBrainsMono Nerd Font"
                controlWidth: 38
                controlHeight: 36
                flat: true
                onClicked: pane.starRequested(pane.message.messageId || "")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Astrea.Theme.spacing

            Astrea.Button {
                text: "Reply"
                iconText: "\uf112"
                iconFontFamily: "JetBrainsMono Nerd Font"
                onClicked: pane.replyRequested()
            }

            Astrea.Button {
                text: "Archive"
                iconText: "\uf187"
                iconFontFamily: "JetBrainsMono Nerd Font"
                enabled: pane.message.folder !== "Archive"
                onClicked: pane.archiveRequested()
            }

            Astrea.Button {
                text: pane.message.isRead ? "Mark unread" : "Mark read"
                iconText: pane.message.isRead ? "\uf0e0" : "\uf2b6"
                iconFontFamily: "JetBrainsMono Nerd Font"
                onClicked: pane.markReadRequested(!pane.message.isRead)
            }

            Astrea.Button {
                visible: pane.message.folder === "Trash" || pane.message.folder === "Archive"
                text: "Move to Inbox"
                iconText: "\uf01c"
                iconFontFamily: "JetBrainsMono Nerd Font"
                onClicked: pane.moveToInboxRequested()
            }

            Astrea.Button {
                text: "Trash"
                iconText: "\uf1f8"
                iconFontFamily: "JetBrainsMono Nerd Font"
                danger: true
                enabled: pane.message.folder !== "Trash"
                onClicked: pane.trashRequested()
            }

            Astrea.Button {
                visible: pane.hasRemoteImages()
                text: "Load images"
                iconText: "\uf03e"
                iconFontFamily: "JetBrainsMono Nerd Font"
                flat: true
                onClicked: pane.loadImagesRequested()
            }

            Astrea.Button {
                visible: pane.hasReadableText()
                text: "Copy text"
                iconText: "\uf0c5"
                iconFontFamily: "JetBrainsMono Nerd Font"
                flat: true
                onClicked: pane.copySelectedText()
            }

            Item { Layout.fillWidth: true }

            TagPill {
                label: pane.message.importance === "high" ? "Important" : pane.message.tag || "Mail"
                tag: pane.message.importance === "high" ? "Important" : pane.message.tag || ""
            }
        }

        Astrea.Divider {
            lineColor: Astrea.Theme.cardBorder
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Astrea.Theme.controlRadius + 4
            color: pane.softSurface
            border.width: 1
            border.color: Astrea.Theme.cardBorder
            clip: true

            ScrollView {
                id: bodyScroll
                anchors.fill: parent
                anchors.margins: Astrea.Theme.spacingLarge
                clip: true
                ScrollBar.horizontal.policy: pane.originalHtml && !pane.waitingForWebPreview() && !pane.hasWebPreview() ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.availableWidth
                    spacing: Astrea.Theme.spacingLarge

                    Item {
                        id: webPreviewFrame

                        visible: pane.hasWebPreview()
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: pane.previewDisplayWidth(bodyScroll.availableWidth)
                        Layout.preferredHeight: pane.previewDisplayHeight(bodyScroll.availableWidth)

                        Image {
                            id: webPreviewImage

                            anchors.fill: parent
                            source: pane.message.webPreviewUrl || ""
                            sourceSize.width: Math.ceil(pane.previewDisplayWidth(bodyScroll.availableWidth))
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            smooth: true
                            cache: true
                        }

                        Repeater {
                            model: pane.webPreviewLinkRects()

                            delegate: Item {
                                x: Math.round((modelData.x || 0) * webPreviewFrame.width / Math.max(1, Number(pane.message.webPreviewWidth || 1)))
                                y: Math.round((modelData.y || 0) * webPreviewFrame.height / Math.max(1, Number(pane.message.webPreviewHeight || 1)))
                                width: Math.max(4, Math.round((modelData.width || 0) * webPreviewFrame.width / Math.max(1, Number(pane.message.webPreviewWidth || 1))))
                                height: Math.max(4, Math.round((modelData.height || 0) * webPreviewFrame.height / Math.max(1, Number(pane.message.webPreviewHeight || 1))))
                                visible: pane.safeExternalLink(modelData.url || "") !== ""

                                Rectangle {
                                    anchors.fill: parent
                                    visible: previewLinkMouse.containsMouse
                                    radius: 4
                                    color: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.08)
                                    border.width: 1
                                    border.color: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.42)
                                }

                                MouseArea {
                                    id: previewLinkMouse

                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mouse => {
                                        if (mouse.button === Qt.RightButton) {
                                            previewLinkMenu.linkUrl = modelData.url || ""
                                            previewLinkMenu.popup()
                                            return
                                        }
                                        pane.openExternalLink(modelData.url || "")
                                    }
                                }
                            }
                        }

                        Menu {
                            id: previewLinkMenu

                            property string linkUrl: ""

                            MenuItem {
                                text: "Open"
                                enabled: pane.safeExternalLink(previewLinkMenu.linkUrl) !== ""
                                onTriggered: pane.openExternalLink(previewLinkMenu.linkUrl)
                            }

                            MenuItem {
                                text: "Copy link"
                                enabled: pane.safeExternalLink(previewLinkMenu.linkUrl) !== ""
                                onTriggered: pane.copyLink(previewLinkMenu.linkUrl)
                            }
                        }
                    }

                    Rectangle {
                        visible: !pane.hasWebPreview()
                        Layout.alignment: pane.originalHtml ? Qt.AlignHCenter : Qt.AlignLeft
                        Layout.fillWidth: !pane.originalHtml
                        Layout.preferredWidth: pane.originalHtml
                            ? Math.min(bodyScroll.availableWidth, 820)
                            : bodyScroll.availableWidth
                        radius: pane.originalHtml ? Astrea.Theme.controlRadius : 0
                        color: pane.originalHtml ? "#ffffff" : "transparent"
                        border.width: pane.originalHtml ? 1 : 0
                        border.color: "#d7dee8"
                        readonly property int contentMargin: pane.originalHtml ? Astrea.Theme.spacingXLarge : 0
                        implicitHeight: Math.max(1, bodyText.contentHeight) + contentMargin * 2
                        clip: pane.originalHtml

                        TextEdit {
                            id: bodyText
                            x: parent.contentMargin
                            y: parent.contentMargin
                            width: Math.max(1, parent.width - parent.contentMargin * 2)
                            height: Math.max(1, contentHeight)
                            readOnly: true
                            selectByMouse: true
                            persistentSelection: true
                            text: pane.fallbackBodyText()
                            textFormat: pane.hasHtmlBody() && !pane.waitingForWebPreview() && !pane.hasWebPreview() ? TextEdit.RichText : TextEdit.PlainText
                            color: pane.originalHtml ? "#25313d" : Astrea.Theme.textPrimary
                            font.family: Astrea.Theme.fontFamily
                            font.pixelSize: pane.originalHtml ? Astrea.Theme.fontSizeNormal : Astrea.Theme.fontSizeLarge
                            wrapMode: TextEdit.WordWrap
                            horizontalAlignment: TextEdit.AlignLeft
                            renderType: Text.NativeRendering
                            antialiasing: true
                            onLinkActivated: link => pane.openExternalLink(link)

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: bodyContextMenu.popup()
                            }
                        }

                        Menu {
                            id: bodyContextMenu

                            MenuItem {
                                text: "Copy"
                                enabled: bodyText.selectedText !== ""
                                onTriggered: pane.copySelectedText()
                            }

                            MenuItem {
                                text: "Copy all"
                                enabled: pane.hasReadableText()
                                onTriggered: pane.copyAllText()
                            }
                        }
                    }

                    Repeater {
                        model: pane.attachmentList()

                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Astrea.Theme.spacingSmall

                            Rectangle {
                                Layout.fillWidth: true
                                visible: modelData.dataUrl !== ""
                                radius: Astrea.Theme.controlRadius + 2
                                color: Qt.rgba(Astrea.Theme.textPrimary.r, Astrea.Theme.textPrimary.g, Astrea.Theme.textPrimary.b, 0.04)
                                border.width: 1
                                border.color: Astrea.Theme.cardBorder
                                implicitHeight: attachmentImage.visible ? Math.min(Math.max(attachmentImage.implicitHeight, 180), 360) : 0
                                clip: true

                                Image {
                                    id: attachmentImage
                                    anchors.fill: parent
                                    anchors.margins: Astrea.Theme.spacingSmall
                                    visible: modelData.dataUrl !== ""
                                    source: modelData.dataUrl || ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    smooth: true
                                    cache: true
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                radius: Astrea.Theme.controlRadius
                                color: pane.softSurface
                                border.width: 1
                                border.color: Astrea.Theme.cardBorder
                                implicitHeight: 44

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Astrea.Theme.spacing
                                    anchors.rightMargin: Astrea.Theme.spacing
                                    spacing: Astrea.Theme.spacing

                                    Text {
                                        text: (modelData.mimeType || "").indexOf("image/") === 0 ? "\uf03e" : "\uf0c6"
                                        color: Astrea.Theme.textSecondary
                                        font.family: "JetBrainsMono Nerd Font"
                                        font.pixelSize: 16
                                    }

                                    Astrea.TextLabel {
                                        Layout.fillWidth: true
                                        text: modelData.name || "Attachment"
                                        textColor: Astrea.Theme.textPrimary
                                        font.pixelSize: Astrea.Theme.fontSizeNormal
                                        elide: Text.ElideRight
                                    }

                                    Astrea.TextLabel {
                                        text: pane.formatBytes(modelData.size || 0)
                                        textColor: Astrea.Theme.textTertiary
                                        font.pixelSize: Astrea.Theme.fontSizeSmall
                                        visible: (modelData.size || 0) > 0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: Astrea.Theme.spacing
        visible: !pane.hasMessage

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "\uf0e0"
            color: Astrea.Theme.textTertiary
            font.family: "JetBrainsMono Nerd Font"
            font.pixelSize: 34
        }

        Astrea.TextLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Select a message"
            textColor: Astrea.Theme.textSecondary
            font.pixelSize: Astrea.Theme.fontSizeLarge
        }
    }

    TextEdit {
        id: clipboardProxy

        visible: false

        function copyText(value) {
            text = String(value || "")
            forceActiveFocus()
            select(0, text.length)
            copy()
            text = ""
        }
    }

    function initials(name) {
        const parts = (name || "Mail").replace(/^\s+|\s+$/g, "").split(/\s+/)
        const first = parts.length > 0 ? parts[0].charAt(0) : "M"
        const second = parts.length > 1 ? parts[parts.length - 1].charAt(0) : ""
        return (first + second).toUpperCase()
    }

    function attachmentList() {
        if (!pane.message || pane.message.attachments === undefined)
            return []
        const attachments = pane.message.attachments || []
        if (!pane.hasHtmlBody())
            return attachments

        const visibleAttachments = []
        for (let i = 0; i < attachments.length; i++) {
            const attachment = attachments[i]
            if (!attachment.inline || attachment.dataUrl === "")
                visibleAttachments.push(attachment)
        }
        return visibleAttachments
    }

    function linkList() {
        if (!pane.message || pane.message.links === undefined)
            return []
        return pane.message.links || []
    }

    function webPreviewLinkRects() {
        if (!pane.message || pane.message.webPreviewLinks === undefined)
            return []
        const links = pane.message.webPreviewLinks || []
        const safeLinks = []
        for (let i = 0; i < links.length; i++) {
            const link = links[i]
            if (pane.safeExternalLink(link.url || "") !== "")
                safeLinks.push(link)
        }
        return safeLinks
    }

    function hasReadableText() {
        return pane.hasMessage
            && String(pane.plainBodyText() || "").replace(/^\s+|\s+$/g, "") !== ""
    }

    function plainBodyText() {
        if (!pane.message)
            return ""
        return pane.message.body || pane.message.preview || ""
    }

    function copySelectedText() {
        if (bodyText.selectedText !== "") {
            bodyText.copy()
            return
        }
        pane.copyAllText()
    }

    function copyAllText() {
        clipboardProxy.copyText(pane.plainBodyText())
    }

    function copyLink(link) {
        const normalized = pane.safeExternalLink(link)
        if (normalized !== "")
            clipboardProxy.copyText(normalized)
    }

    function openExternalLink(link) {
        const normalized = pane.safeExternalLink(link)
        if (normalized !== "")
            Qt.openUrlExternally(normalized)
    }

    function safeExternalLink(link) {
        let value = String(link || "").replace(/^\s+|\s+$/g, "")
        if (value.indexOf("//") === 0)
            value = "https:" + value
        const lower = value.toLowerCase()
        if (lower.indexOf("http://") === 0 || lower.indexOf("https://") === 0) {
            const authority = value.slice(value.indexOf("://") + 3).split(/[/?#]/)[0]
            return authority !== "" ? value : ""
        }
        if (lower.indexOf("mailto:") === 0)
            return value.length > 7 ? value : ""
        return ""
    }

    function hasHtmlBody() {
        return pane.message
            && pane.message.htmlBody !== undefined
            && String(pane.message.htmlBody || "").replace(/^\s+|\s+$/g, "") !== ""
    }

    function hasRemoteImages() {
        return pane.message
            && Number(pane.message.remoteImageCount || 0) > 0
            && !pane.message.remoteImagesLoaded
    }

    function hasWebPreview() {
        return pane.message
            && pane.message.webPreviewUrl !== undefined
            && String(pane.message.webPreviewUrl || "").replace(/^\s+|\s+$/g, "") !== ""
    }

    function waitingForWebPreview() {
        return pane.originalHtml && !pane.hasWebPreview()
    }

    function fallbackBodyText() {
        if (pane.hasWebPreview())
            return ""
        if (pane.waitingForWebPreview())
            return pane.message.body || pane.message.preview || ""
        if (!pane.hasHtmlBody())
            return pane.message.body || ""
        return String(pane.message.htmlBody || "").replace(/font-size\s*:\s*0+(?:\.0+)?(?:px|pt|em|rem|%)?/gi, "font-size:1px")
    }

    function previewDisplayWidth(containerWidth) {
        const available = Math.max(1, Number(containerWidth || 1))
        const sourceWidth = Math.max(1, Number(pane.message.webPreviewWidth || 820))
        return Math.min(available, sourceWidth)
    }

    function previewDisplayHeight(containerWidth) {
        const sourceWidth = Math.max(1, Number(pane.message.webPreviewWidth || 820))
        const sourceHeight = Math.max(1, Number(pane.message.webPreviewHeight || 640))
        return Math.ceil(sourceHeight * pane.previewDisplayWidth(containerWidth) / sourceWidth)
    }

    function formatBytes(value) {
        const size = Number(value || 0)
        if (size <= 0)
            return ""
        if (size < 1024)
            return size + " B"
        if (size < 1024 * 1024)
            return Math.round(size / 1024) + " KB"
        return (size / (1024 * 1024)).toFixed(size < 10 * 1024 * 1024 ? 1 : 0) + " MB"
    }
}
