import QtQuick
import QtWebEngine

Window {
    id: root

    readonly property var appArgs: Qt.application.arguments
    readonly property string htmlPath: Qt.application.arguments.length > 1
        ? Qt.application.arguments[Qt.application.arguments.length - 1]
        : ""
    readonly property string htmlUrl: htmlPath.indexOf("file://") === 0
        ? htmlPath
        : "file://" + htmlPath

    visible: true
    x: argNumber("--x", 0)
    y: argNumber("--y", 0)
    width: argNumber("--width", 980)
    height: argNumber("--height", 760)
    minimumWidth: 760
    minimumHeight: 520
    title: "Astrea Mail"
    color: "#111827"

    WebEngineProfile {
        id: mailProfile

        offTheRecord: true
        httpCacheType: WebEngineProfile.MemoryHttpCache
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
        httpUserAgent: "AstreaMail/0.1"
    }

    WebEngineView {
        id: webView

        anchors.fill: parent
        profile: mailProfile
        url: root.htmlUrl

        settings.autoLoadImages: true
        settings.javascriptEnabled: false
        settings.javascriptCanOpenWindows: false
        settings.javascriptCanAccessClipboard: false
        settings.localStorageEnabled: false
        settings.localContentCanAccessRemoteUrls: false
        settings.localContentCanAccessFileUrls: true
        settings.hyperlinkAuditingEnabled: false
        settings.errorPageEnabled: false
        settings.pluginsEnabled: false
        settings.webGLEnabled: false
        settings.accelerated2dCanvasEnabled: false
        settings.autoLoadIconsForPage: false
        settings.touchIconsEnabled: false
        settings.allowRunningInsecureContent: false
        settings.allowGeolocationOnInsecureOrigins: false
        settings.allowWindowActivationFromJavaScript: false
        settings.playbackRequiresUserGesture: true
        settings.webRTCPublicInterfacesOnly: true
        settings.unknownUrlSchemePolicy: WebEngineSettings.DisallowUnknownUrlSchemes

        onNavigationRequested: request => {
            const target = String(request.url)
            if (request.navigationType !== WebEngineView.LinkClickedNavigation)
                return

            request.action = WebEngineView.IgnoreRequest
            if (target.indexOf("http://") === 0
                    || target.indexOf("https://") === 0
                    || target.indexOf("mailto:") === 0) {
                Qt.openUrlExternally(request.url)
            }
        }
    }

    function argNumber(name, fallback) {
        for (let i = 0; i < root.appArgs.length - 1; i++) {
            if (root.appArgs[i] === name) {
                const parsed = Number(root.appArgs[i + 1])
                if (!isNaN(parsed))
                    return parsed
            }
        }
        return fallback
    }
}
