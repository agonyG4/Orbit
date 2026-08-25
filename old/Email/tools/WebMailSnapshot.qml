import QtQuick
import QtWebEngine

Window {
    id: root

    readonly property var appArgs: Qt.application.arguments
    readonly property string htmlPath: argString("--html", "")
    readonly property string outputPath: argString("--output", "")
    readonly property string linksOutputPath: argString("--links-output", "")
    readonly property string htmlUrl: htmlPath.indexOf("file://") === 0
        ? htmlPath
        : "file://" + htmlPath
    readonly property int requestedWidth: argNumber("--width", 820)
    readonly property int maxHeight: argNumber("--max-height", 16000)

    property bool finished: false

    visible: true
    x: -20000
    y: -20000
    width: requestedWidth
    height: 640
    flags: Qt.Tool | Qt.FramelessWindowHint
    color: "#ffffff"

    Component.onCompleted: {
        if (htmlPath === "" || outputPath === "")
            fail("Missing --html or --output")
    }

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

        onLoadingChanged: loadingInfo => {
            if (loadingInfo.status === WebEngineView.LoadSucceededStatus)
                settleTimer.restart()
            else if (loadingInfo.status === WebEngineView.LoadFailedStatus)
                root.fail("WebEngine failed to load preview")
        }
    }

    Timer {
        id: timeoutTimer
        interval: 7000
        repeat: false
        running: true
        onTriggered: root.fail("WebEngine snapshot timed out")
    }

    Timer {
        id: settleTimer
        interval: 420
        repeat: false
        onTriggered: root.measureContent()
    }

    Timer {
        id: captureTimer
        interval: 160
        repeat: false
        onTriggered: root.capture()
    }

    function measureContent() {
        webView.runJavaScript("(() => { const body = document.body; const rect = body.getBoundingClientRect(); const childBottom = Array.from(body.children).reduce((bottom, child) => Math.max(bottom, child.getBoundingClientRect().bottom), 0); return Math.ceil(Math.max(rect.bottom, childBottom)); })()", result => {
            let pageHeight = Number(result)
            if (isNaN(pageHeight) || pageHeight < 1)
                pageHeight = Number(webView.contentsSize.height || 0)
            if (isNaN(pageHeight) || pageHeight < 1)
                pageHeight = 640
            root.height = Math.max(80, Math.min(root.maxHeight, Math.ceil(pageHeight)))
            captureTimer.restart()
        })
    }

    function capture() {
        if (root.finished)
            return
        webView.runJavaScript(root.linkRectsScript(), links => {
            console.log("__ASTREA_WEB_PREVIEW_LINKS__" + JSON.stringify(links || []))
            webView.grabToImage(result => {
                if (!result.saveToFile(root.outputPath))
                    root.fail("Failed to save WebEngine snapshot")
                else
                    root.finish(0)
            })
        })
    }

    function linkRectsScript() {
        return "(() => {"
            + "function n(raw){let v=String(raw||'').trim();if(!v)return '';if(v.indexOf('//')===0)v='https:'+v;if(!/^(https?:|mailto:)/i.test(v))return '';try{const p=new URL(v);if((p.protocol==='http:'||p.protocol==='https:')&&p.host)return p.href;if(p.protocol==='mailto:'&&p.pathname)return p.href;}catch(e){return '';}return '';}"
            + "function l(v){return String(v||'').replace(/\\s+/g,' ').trim().slice(0,96);}"
            + "const out=[];const seen=new Set();"
            + "for(const a of Array.from(document.querySelectorAll('a[href]'))){const u=n(a.getAttribute('href'));if(!u)continue;const label=l(a.innerText||a.getAttribute('aria-label')||a.getAttribute('title')||u)||u;for(const r of Array.from(a.getClientRects())){const w=Math.round(r.width);const h=Math.round(r.height);if(w<3||h<3)continue;const x=Math.max(0,Math.round(r.left));const y=Math.max(0,Math.round(r.top));const k=[u,x,y,w,h].join(':');if(seen.has(k))continue;seen.add(k);out.push({url:u,label:label,x:x,y:y,width:w,height:h});if(out.length>=120)return out;}}"
            + "return out;})()"
    }

    function finish(code) {
        if (root.finished)
            return
        root.finished = true
        timeoutTimer.stop()
        Qt.exit(code)
    }

    function fail(message) {
        console.error(message)
        root.finish(2)
    }

    function argString(name, fallback) {
        for (let i = 0; i < root.appArgs.length - 1; i++) {
            if (root.appArgs[i] === name)
                return root.appArgs[i + 1]
        }
        return fallback
    }

    function argNumber(name, fallback) {
        const parsed = Number(argString(name, String(fallback)))
        return isNaN(parsed) ? fallback : parsed
    }
}
