import QtQuick

Item {
    id: root

    property string appName: "Astrea"
    property string appIcon: ""
    property bool appForeground: false
    property string presentation: "banner"
    property string foregroundPresentation: "list"
    property string urgency: "normal"
    property string interruptionLevel: "active"
    property int expireTimeout: -1
    // Notifications are delivered by the native application service. Keep
    // the old surface so shared QML callers remain source-compatible.
    property string notifyPath: ""
    property bool busy: false

    signal delivered(var payload)
    signal failed(string message)

    function value(payload, key, fallback) {
        if (!payload || payload[key] === undefined || payload[key] === null || payload[key] === "")
            return fallback
        return payload[key]
    }

    function presentationFor(payload) {
        if (payload && payload.presentation !== undefined && payload.presentation !== "")
            return payload.presentation
        if (root.appForeground && !(payload && payload.forceBanner))
            return root.foregroundPresentation
        return root.presentation
    }

    function notify(payload) {
        const item = payload || ({})
        const summary = root.value(item, "summary", qsTr("Notification"))
        const args = [
            "--app", root.value(item, "appName", root.appName),
            "--summary", summary,
            "--body", root.value(item, "body", ""),
            "--icon", root.value(item, "appIcon", root.appIcon),
            "--presentation", root.presentationFor(item),
            "--urgency", root.value(item, "urgency", root.urgency),
            "--interruption-level", root.value(item, "interruptionLevel", root.interruptionLevel),
            "--expire-timeout", String(root.value(item, "expireTimeout", root.expireTimeout))
        ]

        const eventId = root.value(item, "eventId", "")
        if (eventId !== "")
            args.push("--event-id", eventId)
        const threadId = root.value(item, "threadId", "")
        if (threadId !== "")
            args.push("--thread-id", threadId)
        const collapseKey = root.value(item, "collapseKey", "")
        if (collapseKey !== "")
            args.push("--collapse-key", collapseKey)

        root.delivered({ ok: true, native: true, arguments: args })
    }
}
