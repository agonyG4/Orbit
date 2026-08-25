import Quickshell
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import "AstreaComponents" as Astrea
import "components" as Email
import "services" as Services
import "state" as State

ApplicationWindow {
    id: root

    visible: true
    width: 1440
    height: 900
    minimumWidth: 760
    minimumHeight: 480
    title: "Email"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    font.family: Astrea.Theme.fontFamily
    font.pixelSize: Astrea.Theme.fontSizeNormal
    font.weight: Astrea.Theme.fontWeightNormal
    background: Rectangle { color: "transparent" }
    onClosing: Qt.quit()

    readonly property int pagePad: Astrea.Theme.pageMargin
    readonly property int sidebarWidth: sidebarCollapsed ? 58 : 206
    readonly property int listWidth: width < 1060 ? 350 : 392
    readonly property color selectedBg: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.16)
    readonly property color selectedBorder: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.34)
    readonly property color softSurface: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035)
    readonly property color hoverSurface: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.045) : Qt.rgba(1, 1, 1, 0.060)
    readonly property bool composeReady: trim(composeTo) !== "" && trim(composeSubject) !== ""
    readonly property string accountLabel: gmail.authenticated
        ? (gmail.account !== "" ? gmail.account : "Gmail connected")
        : ""

    property bool sidebarCollapsed: false
    property bool composeOpen: false
    property bool settingsOpen: false
    property string composeTo: ""
    property string composeSubject: ""
    property string composeBody: ""
    property bool mailLoadingMore: false
    property bool mailForceRefreshPages: false
    property bool mailBackgroundRefreshing: false
    property string mailNextPageToken: ""
    property int mailResultSizeEstimate: 0
    property var pendingPreviewRequests: ({})
    property bool mailServiceEnabled: true
    property bool copyCodesEnabled: true
    property bool islandCodesEnabled: true
    property bool desktopNotificationsEnabled: true

    function trim(value) {
        return (value || "").replace(/^\s+|\s+$/g, "")
    }

    function shorten(value, limit) {
        const text = trim(value || "").replace(/\s+/g, " ")
        if (text.length <= limit)
            return text
        return text.slice(0, limit - 1) + "..."
    }

    function canSync(messageId) {
        return gmail.authenticated && !mail.isLocalMessage(messageId)
    }

    function refreshMailbox(forceRefresh) {
        const shouldRefresh = forceRefresh === true
        mailLoadingMore = false
        mailForceRefreshPages = shouldRefresh
        mailBackgroundRefreshing = false
        mailNextPageToken = ""
        mailResultSizeEstimate = 0
        if (gmail.authenticated) {
            mail.statusText = shouldRefresh ? "Refreshing Gmail" : "Loading saved mail"
            gmail.list(mail.selectedFolder, mail.messageFilter, mail.searchText, "", 100, shouldRefresh, !shouldRefresh)
        } else {
            mail.statusText = "0 messages"
            mail.rebuildMessages()
        }
    }

    function refreshMailboxFromNetwork(background) {
        if (!gmail.authenticated)
            return
        mailLoadingMore = false
        mailForceRefreshPages = true
        mailBackgroundRefreshing = background === true
        mailNextPageToken = ""
        mailResultSizeEstimate = 0
        if (!mailBackgroundRefreshing)
            mail.statusText = "Refreshing Gmail"
        gmail.list(mail.selectedFolder, mail.messageFilter, mail.searchText, "", 100, true, false)
    }

    function pollEmailNotifications() {
        if (!gmail.authenticated || !root.mailServiceEnabled)
            return
        gmail.notify(20, root.desktopNotificationAllowedForPoll(), root.copyCodesEnabled, root.islandCodesEnabled)
    }

    function desktopNotificationAllowedForPoll() {
        if (!root.desktopNotificationsEnabled)
            return false
        return !(root.active
            && !root.settingsOpen
            && !root.composeOpen
            && mail.selectedFolder === "Inbox"
            && mail.searchText === "")
    }

    function applyEmailSettings(settings) {
        if (!settings)
            return
        if (settings.mailServiceEnabled !== undefined)
            root.mailServiceEnabled = !!settings.mailServiceEnabled
        if (settings.copyCodesEnabled !== undefined)
            root.copyCodesEnabled = !!settings.copyCodesEnabled
        if (settings.islandCodesEnabled !== undefined)
            root.islandCodesEnabled = !!settings.islandCodesEnabled
        if (settings.desktopNotificationsEnabled !== undefined)
            root.desktopNotificationsEnabled = !!settings.desktopNotificationsEnabled
    }

    function setEmailSetting(key, value) {
        if (key === "mailServiceEnabled")
            root.mailServiceEnabled = value
        else if (key === "copyCodesEnabled")
            root.copyCodesEnabled = value
        else if (key === "islandCodesEnabled")
            root.islandCodesEnabled = value
        else if (key === "desktopNotificationsEnabled")
            root.desktopNotificationsEnabled = value
        gmail.setSetting(key, value)
    }

    function loadedStatus() {
        const estimate = mailResultSizeEstimate
        const loaded = mail.visibleMessages.count
        if (estimate > loaded)
            return "Loaded " + loaded + " of " + estimate
        return loaded + " messages loaded"
    }

    function loadMoreMessages() {
        if (!gmail.authenticated || gmail.busy || mailNextPageToken === "")
            return
        mailLoadingMore = true
        mail.statusText = "Loading more Gmail messages"
        gmail.list(mail.selectedFolder, mail.messageFilter, mail.searchText, mailNextPageToken, 100, mailForceRefreshPages, false)
    }

    function showGmailDetails() {
        settingsOpen = true
        mail.statusText = "Credentials: " + gmail.credentialsPath
    }

    function selectFolder(folder) {
        settingsOpen = false
        mail.selectFolder(folder)
        refreshMailbox(false)
    }

    function setMessageFilter(filter) {
        mail.setMessageFilter(filter)
        refreshMailbox(false)
    }

    function updateSearch(value) {
        mail.setSearchText(value)
        if (gmail.authenticated)
            searchDebounce.restart()
    }

    function clearFilters() {
        mail.clearFilters()
        refreshMailbox(false)
    }

    function selectMessage(messageId) {
        const previous = mail.messageById(messageId)
        mail.selectMessage(messageId)
        requestMessageDetail(messageId)
        if (previous.messageId && !previous.isRead && canSync(messageId))
            gmail.modify(messageId, "read")
    }

    function previewRequestKey(messageId, loadImages) {
        return String(messageId || "") + "|" + (loadImages ? "images" : "plain")
    }

    function setPreviewRequestPending(messageId, loadImages, pending) {
        const key = root.previewRequestKey(messageId, loadImages)
        const next = Object.assign({}, root.pendingPreviewRequests)
        if (pending)
            next[key] = true
        else
            delete next[key]
        root.pendingPreviewRequests = next
    }

    function clearPreviewRequestsFor(messageId) {
        const prefix = String(messageId || "") + "|"
        const next = ({})
        const keys = Object.keys(root.pendingPreviewRequests)
        for (let i = 0; i < keys.length; i++) {
            if (keys[i].indexOf(prefix) !== 0)
                next[keys[i]] = root.pendingPreviewRequests[keys[i]]
        }
        root.pendingPreviewRequests = next
    }

    function requestMessagePreview(messageId, loadImages, force) {
        if (!canSync(messageId))
            return

        const current = mail.messageById(messageId)
        if (!current.messageId || current.htmlRenderMode !== "html")
            return
        const previewLinksReady = !!current.webPreviewLinksReady
        if (!force && String(current.webPreviewUrl || "") !== "" && previewLinksReady)
            return

        const key = root.previewRequestKey(messageId, loadImages === true)
        if (root.pendingPreviewRequests[key])
            return

        root.setPreviewRequestPending(messageId, loadImages === true, true)
        gmail.preview(messageId, loadImages === true)
    }

    function requestMessageDetail(messageId) {
        if (!canSync(messageId))
            return

        const current = mail.messageById(messageId)
        if (current.messageId
                && (!current.detailLoaded
                    || current.htmlRenderMode === "reader")) {
            gmail.get(messageId, false, true)
            return
        }

        root.requestMessagePreview(messageId, false)
    }

    function loadActiveImages() {
        const messageId = mail.activeMessage.messageId || ""
        if (messageId === "" || !canSync(messageId))
            return
        mail.statusText = "Loading images"
        root.requestMessagePreview(messageId, true, true)
    }

    function selectRelative(delta) {
        if (mail.visibleMessages.count === 0)
            return

        let index = 0
        for (let i = 0; i < mail.visibleMessages.count; i++) {
            if (mail.visibleMessages.get(i).messageId === mail.selectedMessageId) {
                index = i
                break
            }
        }

        index = Math.max(0, Math.min(mail.visibleMessages.count - 1, index + delta))
        selectMessage(mail.visibleMessages.get(index).messageId)
    }

    function toggleStar(messageId) {
        const previous = mail.messageById(messageId)
        if (!previous.messageId)
            return
        mail.toggleStar(messageId)
        if (canSync(messageId))
            gmail.modify(messageId, previous.starred ? "unstar" : "star")
    }

    function archiveActive() {
        const messageId = mail.activeMessage.messageId || ""
        if (messageId === "")
            return
        mail.setFolder(messageId, "Archive")
        if (canSync(messageId))
            gmail.modify(messageId, "archive")
    }

    function trashActive() {
        const messageId = mail.activeMessage.messageId || ""
        if (messageId === "")
            return
        mail.setFolder(messageId, "Trash")
        if (canSync(messageId))
            gmail.modify(messageId, "trash")
    }

    function moveActiveToInbox() {
        const messageId = mail.activeMessage.messageId || ""
        if (messageId === "")
            return
        mail.moveToInbox(messageId)
        if (canSync(messageId))
            gmail.modify(messageId, "inbox")
    }

    function setActiveRead(read) {
        const messageId = mail.activeMessage.messageId || ""
        if (messageId === "")
            return
        mail.setRead(messageId, read)
        if (canSync(messageId))
            gmail.modify(messageId, read ? "read" : "unread")
    }

    function openCompose(mode) {
        if (mode === "reply" && mail.activeMessage && mail.activeMessage.messageId) {
            composeTo = mail.activeMessage.fromAddress
            composeSubject = mail.activeMessage.subject.indexOf("Re:") === 0 ? mail.activeMessage.subject : "Re: " + mail.activeMessage.subject
            composeBody = "\n\nOn " + mail.activeMessage.timestamp + ", " + mail.activeMessage.fromName + " wrote:\n> " + shorten(mail.activeMessage.body, 220)
        } else {
            composeTo = ""
            composeSubject = ""
            composeBody = ""
        }
        composeOpen = true
    }

    function closeCompose() {
        composeOpen = false
    }

    function sendDraft() {
        const to = trim(composeTo)
        const subject = trim(composeSubject)
        const body = trim(composeBody)
        if (to === "" || subject === "") {
            mail.statusText = "Add recipient and subject"
            return
        }

        if (gmail.authenticated) {
            mail.statusText = "Sending via Gmail"
            gmail.send(to, subject, body)
        } else {
            mail.statusText = "Connect Gmail to send"
        }
    }

    Timer {
        id: searchDebounce
        interval: 350
        repeat: false
        onTriggered: root.refreshMailbox(false)
    }

    Timer {
        id: networkRefreshDelay
        interval: 450
        repeat: false
        onTriggered: root.refreshMailboxFromNetwork(true)
    }

    Timer {
        id: inboxPoll
        interval: 60000
        repeat: true
        running: gmail.authenticated
            && !root.settingsOpen
            && !root.composeOpen
            && mail.selectedFolder === "Inbox"
            && mail.searchText === ""
        onTriggered: root.refreshMailboxFromNetwork(true)
    }

    Timer {
        id: emailNotifyPoll
        interval: 30000
        repeat: true
        running: gmail.authenticated && root.mailServiceEnabled
        onTriggered: root.pollEmailNotifications()
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: root.openCompose("new")
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: searchBox.focusField(true)
    }

    Shortcut {
        sequence: "Ctrl+Down"
        onActivated: root.selectRelative(1)
    }

    Shortcut {
        sequence: "Ctrl+Up"
        onActivated: root.selectRelative(-1)
    }

    State.MailStore {
        id: mail
    }

    Services.EmailCliClient {
        id: gmail

        onStatusReady: payload => {
            if (payload.authenticated) {
                mail.statusText = payload.account || "Gmail ready"
                root.refreshMailbox(false)
                root.pollEmailNotifications()
            } else {
                mail.statusText = "0 messages"
            }
        }

        onAuthReady: payload => {
            mail.statusText = payload.account ? "Connected: " + payload.account : "Gmail connected"
            root.refreshMailbox(true)
            root.pollEmailNotifications()
        }

        onSettingsReady: payload => {
            root.applyEmailSettings(payload.settings)
        }

        onMessagesReady: payload => {
            root.mailNextPageToken = payload.nextPageToken || ""
            root.mailResultSizeEstimate = payload.resultSizeEstimate || 0
            if (root.mailLoadingMore)
                mail.appendMessages(payload.messages || [], "Gmail synced")
            else
                mail.replaceMessages(payload.messages || [], "Gmail synced")
            root.mailLoadingMore = false
            if (payload.cacheMiss && !root.mailBackgroundRefreshing)
                mail.statusText = "Refreshing Gmail"
            else if (!root.mailBackgroundRefreshing || payload.cacheMiss)
                mail.statusText = root.loadedStatus()
            root.mailBackgroundRefreshing = false
            root.requestMessageDetail(mail.selectedMessageId)
            if (payload.cached || payload.cacheMiss)
                networkRefreshDelay.restart()
        }

        onSendReady: payload => {
            root.composeOpen = false
            root.composeTo = ""
            root.composeSubject = ""
            root.composeBody = ""
            mail.statusText = "Sent via Gmail"
            mail.selectFolder("Sent")
            root.refreshMailbox(true)
        }

        onModifyReady: payload => {
            if (payload.message && payload.message.messageId)
                mail.applyRemotePatch(payload.message)
        }

        onMessageReady: payload => {
            if (payload.message && payload.message.messageId) {
                mail.applyRemoteMessage(payload.message)
                if (payload.message.messageId === mail.selectedMessageId)
                    root.requestMessagePreview(payload.message.messageId, false)
            }
        }

        onPreviewReady: payload => {
            if (payload.message && payload.message.messageId) {
                root.clearPreviewRequestsFor(payload.message.messageId)
                mail.applyRemotePatch(payload.message)
            }
        }

        onNotifyReady: payload => {
            if (payload.newCount > 0) {
                const status = payload.newCount + " new email" + (payload.newCount === 1 ? "" : "s")
                mail.statusText = status
                if (mail.selectedFolder === "Inbox" && mail.searchText === "")
                    mail.prependMessages(payload.events || [], status)
                networkRefreshDelay.restart()
            }
        }

        onViewReady: payload => {
            mail.statusText = payload.message || "Opened original message"
        }

        onFailed: (action, message) => {
            root.mailLoadingMore = false
            root.mailBackgroundRefreshing = false
            if (action === "preview")
                root.pendingPreviewRequests = ({})
            mail.statusText = message
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        color: "transparent"
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.24) : Qt.rgba(0, 0, 0, 0.6)
            shadowBlur: 1.0
            shadowVerticalOffset: 8
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Astrea.Theme.windowBackground
        border.width: 1
        border.color: Astrea.Theme.windowBorder
        clip: true

        Item {
            id: sceneLayer
            anchors.fill: parent

            Rectangle {
                anchors.fill: parent
                color: Astrea.Theme.windowWash
            }

            MouseArea {
                property point pressPos
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                height: 54
                cursorShape: Qt.SizeAllCursor
                onPressed: mouse => pressPos = Qt.point(mouse.x, mouse.y)
                onPositionChanged: mouse => {
                    if (pressed) {
                        root.setX(root.x + mouse.x - pressPos.x)
                        root.setY(root.y + mouse.y - pressPos.y)
                    }
                }
            }

            RowLayout {
                anchors {
                    fill: parent
                    margins: root.pagePad
                }
                spacing: Astrea.Theme.spacingLarge

                Email.EmailSidebar {
                    Layout.preferredWidth: root.sidebarWidth
                    Layout.fillHeight: true
                    collapsed: root.sidebarCollapsed
                    statusText: gmail.busy ? "Gmail working" : mail.statusText
                    selectedFolder: mail.selectedFolder
                    settingsOpen: root.settingsOpen
                    inboxUnread: mail.unreadCount("Inbox")
                    draftsCount: mail.folderCount("Drafts")
                    onComposeRequested: root.openCompose("new")
                    onFolderRequested: folder => root.selectFolder(folder)
                    onCollapseRequested: collapsed => root.sidebarCollapsed = collapsed
                    onSettingsRequested: {
                        root.settingsOpen = true
                        gmail.refreshSettings()
                    }

                    Behavior on Layout.preferredWidth {
                        NumberAnimation {
                            duration: Astrea.Theme.animationNormal
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Astrea.Theme.spacingLarge

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Astrea.Theme.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Astrea.Theme.spacingTiny

                            Astrea.DisplayLabel {
                                text: root.settingsOpen ? "Settings" : "Email"
                                textColor: Astrea.Theme.textPrimary
                                font.pixelSize: Astrea.Theme.fontSizeHeader
                                font.weight: Astrea.Theme.fontWeightDemiBold
                            }

                            Astrea.TextLabel {
                                Layout.fillWidth: true
                                text: root.settingsOpen
                                    ? "Mail service, notifications, security codes and Gmail account"
                                    : (mail.selectedFolder === "All" ? "All Mail" : mail.selectedFolder)
                                    + " • " + mail.visibleMessages.count + " shown"
                                    + " • " + mail.unreadCount(mail.selectedFolder) + " unread"
                                    + (root.accountLabel !== "" ? " • " + root.accountLabel : "")
                                textColor: Astrea.Theme.textSecondary
                                font.pixelSize: Astrea.Theme.fontSizeNormal
                                elide: Text.ElideRight
                            }
                        }

                        Email.MetricPill {
                            visible: !root.settingsOpen
                            label: "Unread"
                            value: mail.unreadCount(mail.selectedFolder)
                            surfaceColor: root.softSurface
                        }

                        Email.MetricPill {
                            visible: !root.settingsOpen
                            label: "Starred"
                            value: mail.starredCount(mail.selectedFolder)
                            surfaceColor: root.softSurface
                        }

                        Astrea.SearchField {
                            id: searchBox
                            visible: !root.settingsOpen
                            Layout.preferredWidth: Math.min(330, Math.max(220, root.width * 0.24))
                            placeholderText: "Search mail"
                            text: mail.searchText
                            onTextEdited: value => root.updateSearch(value)
                            onCleared: root.clearFilters()
                        }

                        Astrea.Button {
                            visible: !root.settingsOpen
                            text: ""
                            iconText: "\uf01e"
                            iconFontFamily: "JetBrainsMono Nerd Font"
                            controlWidth: 38
                            controlHeight: 36
                            flat: true
                            enabled: !gmail.busy
                            onClicked: root.refreshMailbox(true)
                        }

                        Astrea.Button {
                            visible: !root.settingsOpen
                            text: "Compose"
                            iconText: "\uf304"
                            iconFontFamily: "JetBrainsMono Nerd Font"
                            primary: true
                            onClicked: root.openCompose("new")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Astrea.Theme.spacingLarge
                        visible: !root.settingsOpen

                        Email.MessageListPane {
                            Layout.preferredWidth: root.listWidth
                            Layout.fillHeight: true
                            messagesModel: mail.visibleMessages
                            selectedFolder: mail.selectedFolder
                            selectedMessageId: mail.selectedMessageId
                            searchText: mail.searchText
                            messageFilter: mail.messageFilter
                            emptyIcon: "\uf0e0"
                            emptyTitle: "You dont have any messages"
                            unreadCount: mail.unreadCount(mail.selectedFolder)
                            starredCount: mail.starredCount(mail.selectedFolder)
                            canLoadMore: root.mailNextPageToken !== ""
                            loadingMore: root.mailLoadingMore
                            resultLabel: root.mailResultSizeEstimate > mail.visibleMessages.count
                                ? mail.visibleMessages.count + " of " + root.mailResultSizeEstimate
                                : ""
                            selectedBg: root.selectedBg
                            selectedBorder: root.selectedBorder
                            softSurface: root.softSurface
                            hoverSurface: root.hoverSurface
                            onFilterRequested: filter => root.setMessageFilter(filter)
                            onMessageRequested: messageId => root.selectMessage(messageId)
                            onClearFiltersRequested: root.clearFilters()
                            onLoadMoreRequested: root.loadMoreMessages()
                        }

                        Email.MessageDetailPane {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            message: mail.activeMessage
                            hasMessage: mail.hasActiveMessage
                            softSurface: root.softSurface
                            onReplyRequested: root.openCompose("reply")
                            onStarRequested: messageId => root.toggleStar(messageId)
                            onArchiveRequested: root.archiveActive()
                            onMarkReadRequested: read => root.setActiveRead(read)
                            onMoveToInboxRequested: root.moveActiveToInbox()
                            onTrashRequested: root.trashActive()
                            onLoadImagesRequested: root.loadActiveImages()
                        }
                    }

                    Email.SetupPanel {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.settingsOpen
                        configured: gmail.configured
                        authenticated: gmail.authenticated
                        busy: gmail.busy
                        account: gmail.account
                        credentialsPath: gmail.credentialsPath
                        tokenPath: gmail.tokenPath
                        statusMessage: gmail.statusMessage
                        softSurface: root.softSurface
                        mailServiceEnabled: root.mailServiceEnabled
                        copyCodesEnabled: root.copyCodesEnabled
                        islandCodesEnabled: root.islandCodesEnabled
                        desktopNotificationsEnabled: root.desktopNotificationsEnabled
                        onConnectRequested: gmail.authenticate()
                        onRefreshRequested: gmail.refreshStatus()
                        onDetailsRequested: root.showGmailDetails()
                        onSettingToggled: (key, value) => root.setEmailSetting(key, value)
                    }
                }
            }
        }

        Email.ComposerSheet {
            anchors.fill: parent
            z: 80
            visible: root.composeOpen
            to: root.composeTo
            subject: root.composeSubject
            messageBody: root.composeBody
            statusText: gmail.busy ? "Gmail working" : mail.statusText
            canSend: root.composeReady && !gmail.busy
            softSurface: root.softSurface
            onCloseRequested: root.closeCompose()
            onToEdited: value => root.composeTo = value
            onSubjectEdited: value => root.composeSubject = value
            onBodyEdited: value => root.composeBody = value
            onSendRequested: root.sendDraft()
        }

        ResizeHandle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: gripSize
            edges: Qt.LeftEdge
            cursorShape: Qt.SizeHorCursor
        }

        ResizeHandle {
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: gripSize
            edges: Qt.RightEdge
            cursorShape: Qt.SizeHorCursor
        }

        ResizeHandle {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: gripSize
            edges: Qt.TopEdge
            cursorShape: Qt.SizeVerCursor
        }

        ResizeHandle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: gripSize
            edges: Qt.BottomEdge
            cursorShape: Qt.SizeVerCursor
        }

        ResizeHandle {
            anchors {
                left: parent.left
                top: parent.top
            }
            width: cornerGripSize
            height: cornerGripSize
            edges: Qt.LeftEdge | Qt.TopEdge
            cursorShape: Qt.SizeFDiagCursor
        }

        ResizeHandle {
            anchors {
                right: parent.right
                top: parent.top
            }
            width: cornerGripSize
            height: cornerGripSize
            edges: Qt.RightEdge | Qt.TopEdge
            cursorShape: Qt.SizeBDiagCursor
        }

        ResizeHandle {
            anchors {
                left: parent.left
                bottom: parent.bottom
            }
            width: cornerGripSize
            height: cornerGripSize
            edges: Qt.LeftEdge | Qt.BottomEdge
            cursorShape: Qt.SizeBDiagCursor
        }

        ResizeHandle {
            anchors {
                right: parent.right
                bottom: parent.bottom
            }
            width: cornerGripSize
            height: cornerGripSize
            edges: Qt.RightEdge | Qt.BottomEdge
            cursorShape: Qt.SizeFDiagCursor
        }
    }

    component ResizeHandle: MouseArea {
        property int edges: Qt.LeftEdge
        readonly property int gripSize: 8
        readonly property int cornerGripSize: 18

        z: 200
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true

        onPressed: mouse => {
            if (!root.startSystemResize(edges))
                mouse.accepted = false
        }
    }
}
