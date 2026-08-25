import QtQuick
import Quickshell
import "../../AstreaComponents" as UI
import "../../ShellComponents" as ShellComponents

Item {
    id: root

    property var row: null
    property int iconRadius: 10
    property int fallbackRadius: iconRadius
    property int fallbackFontSize: 14
    property int sourcePixelSize: 192
    property color fallbackColor: UI.Theme.cardBg
    property color fallbackTextColor: UI.Theme.textPrimary
    readonly property var entry: altTabEntry(row)

    implicitWidth: 42
    implicitHeight: 42

    function rowLabel(value) {
        if (!value)
            return qsTr("App")
        return value.label || value.name || value.id || qsTr("App")
    }

    function rowClass(value) {
        if (!value)
            return ""
        return value.className || value.class || value.initialClass || value.id || ""
    }

    function rowTitle(value) {
        if (!value)
            return ""
        return value.title || value.last_title || value.initialTitle || rowLabel(value)
    }

    function clientNeedsDeepIcon(client) {
        const text = String([
            client.className || "",
            client.initialClass || "",
            client.title || "",
            client.initialTitle || ""
        ].join(" ")).toLowerCase()
        return text.indexOf(".exe") >= 0
            || text.indexOf("wine") >= 0
            || text.indexOf("proton") >= 0
            || text.indexOf("pressure-vessel") >= 0
            || text.indexOf("steam_app_") >= 0
    }

    function displayNameFromMetadata(className, title) {
        const cls = (className || "").trim()
        const windowTitle = (title || "").trim()
        if (cls.toLowerCase().indexOf("steam_app_") === 0 && windowTitle.length > 0)
            return windowTitle
        if (cls.length > 0 && cls !== "org.quickshell")
            return titleCase(cls)
        return windowTitle || "App"
    }

    function astreaLauncherIconPath(name) {
        return "/home/agony/.local/share/applications/astrea-icons/" + name + ".png"
    }

    function iconNameForClient(className, title) {
        const rawClass = String(className || "").trim()
        const cls = String(className || "").toLowerCase()
        const text = String(title || "").toLowerCase()

        if (cls === "org.vinegarhq.sober") return "org.vinegarhq.Sober"
        if (cls.indexOf("zen") >= 0) return "zen-browser"
        if (cls.indexOf("kitty") >= 0) return "kitty"
        if (cls.indexOf("code") >= 0 || cls.indexOf("cursor") >= 0) return "visual-studio-code"
        if (cls.indexOf("spotify") >= 0) return "spotify"
        if (cls.indexOf("discord") >= 0) return "discord"
        const steamGame = cls.match(/^steam_app_(\d+)$/)
        if (steamGame) return "steam_icon_" + steamGame[1]
        if (cls === "steam_app_default") return ""
        if (cls.indexOf("steam") >= 0) return "steam"
        if (text.indexOf("settings") >= 0 || text.indexOf("configura") >= 0) return astreaLauncherIconPath("astrea-settings")
        if (text.indexOf("screen") >= 0 && text.indexOf("time") >= 0) return "clock"
        const desktopIcon = desktopIconForClient(cls, text)
        if (desktopIcon.length > 0) return desktopIcon
        if (text.indexOf("finder") >= 0) return "folder"
        if (text.indexOf("weather") >= 0 || text.indexOf("clima") >= 0) return "weather-clear"
        if (cls.indexOf("org.quickshell") >= 0) return "application-x-executable"
        return rawClass.length > 0 ? "application-x-executable" : ""
    }

    function desktopIconForClient(className, title) {
        if (typeof DesktopEntries === "undefined")
            return ""
        const apps = DesktopEntries.applications ? DesktopEntries.applications.values : []
        let bestIcon = ""
        let bestScore = 0

        for (let entry of apps) {
            if (!entry || entry.noDisplay || !entry.icon)
                continue
            const entryName = String(entry.name || "").toLowerCase()
            const entryId = String(entry.id || entry.desktopId || entry.fileName || "").toLowerCase()
            const entryExec = String(entry.exec || entry.execString || "").toLowerCase()
            const hay = entryName + " " + entryId + " " + entryExec
            let score = 0

            if (className.length > 0 && (hay.indexOf(className) >= 0 || hay.indexOf(className.replace("-bin", "")) >= 0))
                score = 3
            if (title.length > 0 && entryName.length > 0 && (title.indexOf(entryName) >= 0 || entryName.indexOf(title) >= 0))
                score = Math.max(score, className === "org.quickshell" ? 4 : 2)
            if (title.length > 0 && entryName === title)
                score = Math.max(score, 6)
            if (score > 0 && entryId.indexOf("astrea-") >= 0)
                score += 1

            if (score > bestScore) {
                bestScore = score
                bestIcon = entry.icon
            }
        }
        return bestIcon
    }

    function titleCase(text) {
        return (text || "App").replace(/[-_.]+/g, " ").replace(/\b\w/g, c => c.toUpperCase())
    }

    function resolvedIconForRow(value, className, title, initialClass, initialTitle) {
        if (!value)
            return ""
        if (value.astreaIcon || value.astreaIconName)
            return value.astreaIcon || value.astreaIconName

        const candidate = {
            className: className || "",
            initialClass: initialClass || "",
            title: title || "",
            initialTitle: initialTitle || ""
        }
        if (clientNeedsDeepIcon(candidate))
            return value.icon || ""

        return iconNameForClient(className, title)
    }

    function altTabEntry(value) {
        const className = rowClass(value)
        const title = rowTitle(value)
        const initialClass = value ? (value.initialClass || className) : className
        const initialTitle = value ? (value.initialTitle || title) : title
        const icon = resolvedIconForRow(value, className, title, initialClass, initialTitle)
        const needsDeepIcon = clientNeedsDeepIcon({
            className: className,
            initialClass: initialClass,
            title: title,
            initialTitle: initialTitle
        })

        return {
            address: value ? (value.address || "") : "",
            className: className,
            initialClass: initialClass,
            title: title,
            initialTitle: initialTitle,
            name: displayNameFromMetadata(className, title),
            pid: value ? Number(value.pid || 0) : 0,
            icon: icon,
            astreaIcon: value ? (value.astreaIcon || "") : "",
            astreaIconName: value ? (value.astreaIconName || "") : "",
            hideIconFallback: needsDeepIcon && !icon
        }
    }

    ShellComponents.AppIcon {
        anchors.fill: parent
        entry: root.entry
        iconRadius: root.iconRadius
        fallbackRadius: root.fallbackRadius
        fallbackColor: root.fallbackColor
        fallbackTextColor: root.fallbackTextColor
        fallbackFontSize: root.fallbackFontSize
        sourcePixelSize: root.sourcePixelSize
        showFallbackText: !root.entry.hideIconFallback
    }
}
