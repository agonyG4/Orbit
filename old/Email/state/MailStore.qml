import QtQuick

Item {
    id: store

    visible: false
    width: 0
    height: 0

    property alias visibleMessages: filteredModel
    property alias allMessages: mailModel
    property string selectedFolder: "Inbox"
    property string selectedMessageId: ""
    property string searchText: ""
    property string messageFilter: "all"
    property string statusText: "Connect Gmail"
    property var activeMessage: ({})
    property var messageIndex: ({})
    property var visibleMessageIds: []
    property bool visibleMessageIdsExplicit: false
    property int modelRevision: 0
    readonly property bool hasActiveMessage: activeMessage !== null
        && activeMessage.messageId !== undefined
        && activeMessage.messageId !== ""

    function trim(value) {
        return (value || "").replace(/^\s+|\s+$/g, "")
    }

    function shorten(value, limit) {
        const text = trim(value || "").replace(/\s+/g, " ")
        if (text.length <= limit)
            return text
        return text.slice(0, limit - 1) + "..."
    }

    function encodeAttachments(value) {
        try {
            return JSON.stringify(value || [])
        } catch (e) {
            return "[]"
        }
    }

    function decodeAttachments(value) {
        if (value === undefined || value === null || value === "")
            return []
        if (typeof value !== "string")
            return value || []
        try {
            return JSON.parse(value)
        } catch (e) {
            return []
        }
    }

    function encodeLinks(value) {
        try {
            return JSON.stringify(value || [])
        } catch (e) {
            return "[]"
        }
    }

    function decodeLinks(value) {
        if (value === undefined || value === null || value === "")
            return []
        if (typeof value !== "string")
            return value || []
        try {
            return JSON.parse(value)
        } catch (e) {
            return []
        }
    }

    function copyMessage(item) {
        if (!item || item.messageId === undefined)
            return ({})
        return {
            messageId: item.messageId,
            threadId: item.threadId || "",
            historyId: item.historyId || "",
            folder: item.folder,
            fromName: item.fromName,
            fromAddress: item.fromAddress,
            subject: item.subject,
            preview: item.preview,
            body: item.body,
            htmlBody: item.htmlBody || "",
            htmlRenderMode: item.htmlRenderMode || (item.htmlBody ? "html" : "plain"),
            htmlSuppressed: !!item.htmlSuppressed,
            htmlLength: item.htmlLength || 0,
            htmlTableCount: item.htmlTableCount || 0,
            webPreviewUrl: item.webPreviewUrl || "",
            webPreviewWidth: item.webPreviewWidth || 0,
            webPreviewHeight: item.webPreviewHeight || 0,
            webPreviewLinks: decodeLinks(item.webPreviewLinksJson),
            webPreviewLinksReady: !!item.webPreviewLinksReady,
            links: decodeLinks(item.linksJson),
            linkCount: item.linkCount || 0,
            timestamp: item.timestamp,
            tag: item.tag,
            starred: item.starred,
            isRead: item.isRead,
            importance: item.importance,
            attachments: decodeAttachments(item.attachmentsJson),
            hasAttachments: !!item.hasAttachments,
            remoteImageCount: item.remoteImageCount || 0,
            remoteImagesLoadedCount: item.remoteImagesLoadedCount || 0,
            remoteImagesLoaded: !!item.remoteImagesLoaded,
            detailLoaded: !!item.detailLoaded
        }
    }

    function messageById(messageId) {
        const index = findIndex(messageId)
        return index >= 0 ? copyMessage(mailModel.get(index)) : ({})
    }

    function isLocalMessage(messageId) {
        return String(messageId || "").indexOf("local-") === 0
    }

    function findIndex(messageId) {
        const cachedIndex = messageIndex[messageId]
        if (cachedIndex !== undefined
                && cachedIndex >= 0
                && cachedIndex < mailModel.count
                && mailModel.get(cachedIndex).messageId === messageId) {
            return cachedIndex
        }

        for (let i = 0; i < mailModel.count; i++) {
            if (mailModel.get(i).messageId === messageId) {
                messageIndex[messageId] = i
                return i
            }
        }
        return -1
    }

    function containsId(ids, messageId) {
        for (let i = 0; i < ids.length; i++) {
            if (ids[i] === messageId)
                return true
        }
        return false
    }

    function appendUniqueId(ids, idSet, messageId) {
        if (!messageId || idSet[messageId])
            return
        idSet[messageId] = true
        ids.push(messageId)
    }

    function idSetFor(ids) {
        const idSet = ({})
        for (let i = 0; i < ids.length; i++) {
            if (ids[i])
                idSet[ids[i]] = true
        }
        return idSet
    }

    function allMessageKeys() {
        return ["threadId", "historyId", "folder", "fromName", "fromAddress", "subject", "preview", "body", "htmlBody", "htmlRenderMode", "htmlSuppressed", "htmlLength", "htmlTableCount", "webPreviewUrl", "webPreviewWidth", "webPreviewHeight", "webPreviewLinksJson", "webPreviewLinksReady", "linksJson", "linkCount", "timestamp", "tag", "starred", "isRead", "importance", "attachmentsJson", "hasAttachments", "remoteImageCount", "remoteImagesLoadedCount", "remoteImagesLoaded", "detailLoaded"]
    }

    function summaryMessageKeys() {
        return ["threadId", "historyId", "folder", "fromName", "fromAddress", "subject", "preview", "timestamp", "tag", "starred", "isRead", "importance", "hasAttachments", "linkCount", "remoteImageCount", "remoteImagesLoadedCount", "remoteImagesLoaded"]
    }

    function setMessageProperties(index, message, keys) {
        for (let i = 0; i < keys.length; i++)
            mailModel.setProperty(index, keys[i], message[keys[i]])
    }

    function upsertMessage(item, forceFull) {
        const message = normalized(item)
        const index = findIndex(message.messageId)
        if (index < 0) {
            messageIndex[message.messageId] = mailModel.count
            mailModel.append(message)
            return message.messageId
        }

        const existing = mailModel.get(index)
        const useFull = forceFull === true || message.detailLoaded || !existing.detailLoaded
        setMessageProperties(index, message, useFull ? allMessageKeys() : summaryMessageKeys())
        return message.messageId
    }

    function knownIdsForCurrentFilter() {
        const ids = []
        for (let i = 0; i < mailModel.count; i++) {
            const item = mailModel.get(i)
            if (matchesFolder(item) && matchesSearch(item))
                ids.push(item.messageId)
        }
        return ids
    }

    function folderCount(folder) {
        modelRevision
        let total = 0
        for (let i = 0; i < mailModel.count; i++) {
            const item = mailModel.get(i)
            if (folder === "Starred") {
                if (item.starred && item.folder !== "Trash")
                    total += 1
            } else if (folder === "All") {
                if (item.folder !== "Trash")
                    total += 1
            } else if (item.folder === folder) {
                total += 1
            }
        }
        return total
    }

    function unreadCount(folder) {
        modelRevision
        let total = 0
        for (let i = 0; i < mailModel.count; i++) {
            const item = mailModel.get(i)
            const inFolder = folder === "Starred"
                ? item.starred && item.folder !== "Trash"
                : (folder === "All" ? item.folder !== "Trash" : item.folder === folder)
            if (inFolder && !item.isRead)
                total += 1
        }
        return total
    }

    function starredCount(folder) {
        modelRevision
        let total = 0
        for (let i = 0; i < mailModel.count; i++) {
            const item = mailModel.get(i)
            const inFolder = folder === "Starred"
                ? item.starred && item.folder !== "Trash"
                : (folder === "All" ? item.folder !== "Trash" : item.folder === folder)
            if (inFolder && item.starred)
                total += 1
        }
        return total
    }

    function matchesFolder(item) {
        const inFolder = selectedFolder === "Starred"
            ? item.starred && item.folder !== "Trash"
            : (selectedFolder === "All" ? item.folder !== "Trash" : item.folder === selectedFolder)

        if (!inFolder)
            return false
        if (messageFilter === "unread")
            return !item.isRead
        if (messageFilter === "starred")
            return item.starred
        return true
    }

    function matchesSearch(item) {
        const query = trim(searchText).toLowerCase()
        if (query === "")
            return true
        const haystack = [
            item.fromName,
            item.fromAddress,
            item.subject,
            item.preview,
            item.tag
        ].join(" ").toLowerCase()
        return haystack.indexOf(query) !== -1
    }

    function syncActiveMessage() {
        activeMessage = ({})
        if (selectedMessageId === "")
            return

        const message = messageById(selectedMessageId)
        if (message.messageId)
            activeMessage = message
    }

    function rebuildMessages() {
        const previous = selectedMessageId
        let previousStillVisible = false
        filteredModel.clear()

        const ids = visibleMessageIds || []
        if (visibleMessageIdsExplicit) {
            for (let i = 0; i < ids.length; i++) {
                const index = findIndex(ids[i])
                if (index < 0)
                    continue
                const item = mailModel.get(index)
                if (matchesFolder(item) && matchesSearch(item)) {
                    filteredModel.append(copyMessage(item))
                    if (item.messageId === previous)
                        previousStillVisible = true
                }
            }
        } else {
            for (let i = 0; i < mailModel.count; i++) {
                const item = mailModel.get(i)
                if (matchesFolder(item) && matchesSearch(item)) {
                    filteredModel.append(copyMessage(item))
                    if (item.messageId === previous)
                        previousStillVisible = true
                }
            }
        }

        if (!previousStillVisible)
            selectedMessageId = filteredModel.count > 0 ? filteredModel.get(0).messageId : ""

        syncActiveMessage()
        modelRevision += 1
    }

    function selectFolder(folder) {
        selectedFolder = folder
        if (folder === "Starred")
            messageFilter = "all"
        visibleMessageIds = []
        visibleMessageIdsExplicit = false
        statusText = folderCount(folder) + " messages"
        rebuildMessages()
    }

    function setMessageFilter(filter) {
        messageFilter = filter
        visibleMessageIds = []
        visibleMessageIdsExplicit = false
        rebuildMessages()
    }

    function setSearchText(value) {
        searchText = value
        visibleMessageIds = []
        visibleMessageIdsExplicit = false
        rebuildMessages()
    }

    function clearFilters() {
        searchText = ""
        messageFilter = "all"
        visibleMessageIds = []
        visibleMessageIdsExplicit = false
        rebuildMessages()
    }

    function selectMessage(messageId) {
        selectedMessageId = messageId
        setRead(messageId, true, false)
        syncActiveMessage()
        modelRevision += 1
    }

    function selectRelative(delta) {
        if (filteredModel.count === 0)
            return

        let index = 0
        for (let i = 0; i < filteredModel.count; i++) {
            if (filteredModel.get(i).messageId === selectedMessageId) {
                index = i
                break
            }
        }

        index = Math.max(0, Math.min(filteredModel.count - 1, index + delta))
        selectMessage(filteredModel.get(index).messageId)
    }

    function setFolder(messageId, folder, rebuild) {
        const index = findIndex(messageId)
        if (index < 0)
            return
        mailModel.setProperty(index, "folder", folder)
        statusText = folder === "Trash" ? "Moved to Trash" : "Moved to " + folder
        if (rebuild === undefined || rebuild)
            rebuildMessages()
    }

    function setStar(messageId, starred, rebuild) {
        const index = findIndex(messageId)
        if (index < 0)
            return
        mailModel.setProperty(index, "starred", starred)
        if (rebuild === undefined || rebuild)
            rebuildMessages()
    }

    function toggleStar(messageId) {
        const index = findIndex(messageId)
        if (index < 0)
            return
        setStar(messageId, !mailModel.get(index).starred)
    }

    function setRead(messageId, read, rebuild) {
        const index = findIndex(messageId)
        if (index < 0)
            return
        mailModel.setProperty(index, "isRead", read)
        if (rebuild === undefined || rebuild) {
            statusText = read ? "Marked as read" : "Marked as unread"
            rebuildMessages()
        }
    }

    function moveToInbox(messageId) {
        setFolder(messageId, "Inbox", false)
        selectedFolder = "Inbox"
        visibleMessageIds = []
        visibleMessageIdsExplicit = false
        statusText = "Moved to Inbox"
        rebuildMessages()
    }

    function replaceMessages(messages, status) {
        const previous = selectedMessageId
        const ids = []
        const idSet = ({})
        for (let i = 0; i < messages.length; i++) {
            const messageId = upsertMessage(messages[i], false)
            appendUniqueId(ids, idSet, messageId)
        }
        visibleMessageIds = ids
        visibleMessageIdsExplicit = true
        statusText = status || "Gmail synced"
        selectedMessageId = idSet[previous] ? previous : (ids.length > 0 ? ids[0] : "")
        rebuildMessages()
    }

    function appendMessages(messages, status) {
        const ids = (visibleMessageIds || []).slice()
        const idSet = idSetFor(ids)
        for (let i = 0; i < messages.length; i++) {
            const messageId = upsertMessage(messages[i], false)
            appendUniqueId(ids, idSet, messageId)
        }
        visibleMessageIds = ids
        visibleMessageIdsExplicit = true
        statusText = status || "More Gmail messages loaded"
        rebuildMessages()
    }

    function prependMessages(messages, status) {
        const previous = selectedMessageId
        const ids = []
        const idSet = ({})
        for (let i = 0; i < messages.length; i++) {
            const messageId = upsertMessage(messages[i], false)
            appendUniqueId(ids, idSet, messageId)
        }

        const oldIds = visibleMessageIds || []
        for (let j = 0; j < oldIds.length; j++)
            appendUniqueId(ids, idSet, oldIds[j])

        visibleMessageIds = ids
        visibleMessageIdsExplicit = true
        statusText = status || "New Gmail messages"
        selectedMessageId = idSet[previous] ? previous : (ids.length > 0 ? ids[0] : "")
        rebuildMessages()
    }

    function applyRemoteMessage(message) {
        upsertMessage(message, true)
        rebuildMessages()
    }

    function applyRemotePatch(patch) {
        if (!patch || patch.messageId === undefined)
            return
        let index = findIndex(patch.messageId)
        if (index < 0) {
            upsertMessage(patch, false)
            index = findIndex(patch.messageId)
            if (index < 0)
                return
        }

        if (patch.attachments !== undefined)
            mailModel.setProperty(index, "attachmentsJson", encodeAttachments(patch.attachments))
        if (patch.webPreviewLinks !== undefined)
            mailModel.setProperty(index, "webPreviewLinksJson", encodeLinks(patch.webPreviewLinks))
        else if (patch.webPreviewLinksJson !== undefined)
            mailModel.setProperty(index, "webPreviewLinksJson", patch.webPreviewLinksJson)
        if (patch.links !== undefined) {
            mailModel.setProperty(index, "linksJson", encodeLinks(patch.links))
            mailModel.setProperty(index, "linkCount", (patch.links || []).length)
        } else if (patch.linksJson !== undefined) {
            mailModel.setProperty(index, "linksJson", patch.linksJson)
        }

        const keys = ["threadId", "historyId", "folder", "fromName", "fromAddress", "subject", "preview", "body", "htmlBody", "htmlRenderMode", "htmlSuppressed", "htmlLength", "htmlTableCount", "webPreviewUrl", "webPreviewWidth", "webPreviewHeight", "webPreviewLinksReady", "linkCount", "timestamp", "tag", "starred", "isRead", "importance", "hasAttachments", "remoteImageCount", "remoteImagesLoadedCount", "remoteImagesLoaded", "detailLoaded"]
        for (let i = 0; i < keys.length; i++) {
            if (patch[keys[i]] !== undefined)
                mailModel.setProperty(index, keys[i], patch[keys[i]])
        }
        rebuildMessages()
    }

    function normalized(item) {
        return {
            messageId: item.messageId || ("local-message-" + Date.now()),
            threadId: item.threadId || "",
            historyId: item.historyId || "",
            folder: item.folder || "Inbox",
            fromName: item.fromName || "Unknown Sender",
            fromAddress: item.fromAddress || "",
            subject: item.subject || "(No subject)",
            preview: item.preview || "",
            body: item.body || item.preview || "",
            htmlBody: item.htmlBody || "",
            htmlRenderMode: item.htmlRenderMode || (item.htmlBody ? "html" : "plain"),
            htmlSuppressed: !!item.htmlSuppressed,
            htmlLength: item.htmlLength || 0,
            htmlTableCount: item.htmlTableCount || 0,
            webPreviewUrl: item.webPreviewUrl || "",
            webPreviewWidth: item.webPreviewWidth || 0,
            webPreviewHeight: item.webPreviewHeight || 0,
            webPreviewLinksJson: item.webPreviewLinksJson || encodeLinks(item.webPreviewLinks),
            webPreviewLinksReady: !!item.webPreviewLinksReady,
            linksJson: item.linksJson || encodeLinks(item.links),
            linkCount: item.linkCount || ((item.links || []).length),
            timestamp: item.timestamp || "",
            tag: item.tag || "Mail",
            starred: !!item.starred,
            isRead: item.isRead === undefined ? true : !!item.isRead,
            importance: item.importance || "normal",
            attachmentsJson: item.attachmentsJson || encodeAttachments(item.attachments),
            hasAttachments: item.hasAttachments === undefined ? ((item.attachments || []).length > 0) : !!item.hasAttachments,
            remoteImageCount: item.remoteImageCount || 0,
            remoteImagesLoadedCount: item.remoteImagesLoadedCount || 0,
            remoteImagesLoaded: !!item.remoteImagesLoaded,
            detailLoaded: !!item.detailLoaded
        }
    }

    Component.onCompleted: rebuildMessages()

    ListModel {
        id: filteredModel
    }

    ListModel {
        id: mailModel
    }
}
