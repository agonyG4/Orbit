import QtQuick
import Quickshell
import Quickshell.Io

Item {
    id: client

    readonly property string bundledCliPath: (Quickshell.env("HOME") || "") + "/GitHub/Bench/Email/bin/astrea-email"
    readonly property string envCliPath: Quickshell.env("ASTREA_EMAIL_CLI") || ""
    readonly property string defaultCliPath: envCliPath !== "" ? envCliPath : bundledCliPath

    property string cliPath: defaultCliPath
    property bool configured: false
    property bool authenticated: false
    property bool busy: emailProc.running || _queue.length > 0
    property string account: ""
    property string statusMessage: "Checking email backend"
    property string lastError: ""
    property string credentialsPath: ""
    property string tokenPath: ""
    property string tokenState: "missing"
    property var _queue: []
    property string _action: ""
    property string _buffer: ""
    property string _errorBuffer: ""

    signal statusReady(var payload)
    signal authReady(var payload)
    signal messagesReady(var payload)
    signal messageReady(var payload)
    signal previewReady(var payload)
    signal linksReady(var payload)
    signal settingsReady(var payload)
    signal notifyReady(var payload)
    signal sendReady(var payload)
    signal modifyReady(var payload)
    signal viewReady(var payload)
    signal failed(string action, string message)

    function refreshStatus() {
        runCommand("status", ["status"])
    }

    function authenticate() {
        runCommand("auth", ["auth"])
    }

    function list(folder, messageFilter, query, pageToken, limit, forceRefresh, cacheOnly) {
        const args = [
            "list",
            "--folder", folder,
            "--filter", messageFilter,
            "--query", query || "",
            "--limit", String(limit || 100)
        ]
        if (pageToken && pageToken !== "")
            args.push("--page-token", pageToken)
        if (forceRefresh)
            args.push("--refresh")
        if (cacheOnly)
            args.push("--cache-only")
        runCommand("list", args)
    }

    function send(to, subject, body) {
        runCommand("send", ["send", "--to", to, "--subject", subject, "--body", body || ""])
    }

    function modify(messageId, action) {
        runCommand("modify", ["modify", "--id", messageId, "--action", action])
    }

    function get(messageId, loadImages, forceOriginal) {
        const args = ["get", "--id", messageId]
        if (loadImages)
            args.push("--images")
        if (forceOriginal)
            args.push("--original")
        runCommand("get", args)
    }

    function preview(messageId, loadImages) {
        const args = ["preview", "--id", messageId]
        if (loadImages)
            args.push("--images")
        runCommand("preview", args)
    }

    function links(messageId) {
        runCommand("links", ["links", "--id", messageId])
    }

    function refreshSettings() {
        runCommand("settings", ["settings"])
    }

    function setSetting(key, value) {
        runCommand("settings", ["settings", "--set", key + "=" + (value ? "true" : "false")])
    }

    function notify(limit, desktopNotify, copyCodes, islandNotify) {
        const args = ["notify", "--limit", String(limit || 20)]
        if (desktopNotify === false)
            args.push("--no-desktop")
        if (copyCodes === false)
            args.push("--no-clipboard")
        if (islandNotify === false)
            args.push("--no-island")
        runCommand("notify", args)
    }

    function view(messageId, loadImages, geometry) {
        const args = ["view", "--id", messageId]
        if (loadImages === false)
            args.push("--no-images")
        if (geometry) {
            if (geometry.x !== undefined)
                args.push("--x", String(Math.round(geometry.x)))
            if (geometry.y !== undefined)
                args.push("--y", String(Math.round(geometry.y)))
            if (geometry.width !== undefined)
                args.push("--width", String(Math.round(geometry.width)))
            if (geometry.height !== undefined)
                args.push("--height", String(Math.round(geometry.height)))
        }
        runCommand("view", args)
    }

    function runCommand(action, args) {
        if (emailProc.running || _queue.length > 0) {
            _enqueueCommand(action, args)
            return
        }

        startCommand(action, args)
    }

    function _enqueueCommand(action, args) {
        const queued = _coalescedQueue(action, args)
        const item = { "action": action, "args": args }

        if (action === "get" && args.indexOf("--images") < 0) {
            let insertIndex = queued.length
            for (let i = 0; i < queued.length; i++) {
                if (queued[i].action === "preview" && queued[i].args.indexOf("--images") < 0) {
                    insertIndex = i
                    break
                }
            }
            queued.splice(insertIndex, 0, item)
        } else {
            queued.push(item)
        }

        _queue = queued
    }

    function _argValue(args, flag) {
        const index = args.indexOf(flag)
        if (index < 0 || index + 1 >= args.length)
            return ""
        return String(args[index + 1] || "")
    }

    function _listRequestKey(args) {
        return [
            _argValue(args, "--folder"),
            _argValue(args, "--filter"),
            _argValue(args, "--query"),
            _argValue(args, "--page-token")
        ].join("|")
    }

    function _coalescedQueue(action, args) {
        if (action === "list") {
            const requestKey = _listRequestKey(args)
            const queued = []
            for (let i = 0; i < _queue.length; i++) {
                const item = _queue[i]
                if (item.action === "list" && _listRequestKey(item.args) === requestKey)
                    continue
                queued.push(item)
            }
            return queued
        }

        if (action === "view" && args.indexOf("--no-images") >= 0) {
            const previewQueue = []
            for (let i = 0; i < _queue.length; i++) {
                const item = _queue[i]
                if (item.action === "view" && item.args.indexOf("--no-images") >= 0)
                    continue
                previewQueue.push(item)
            }
            return previewQueue
        }

        if (action === "preview" && args.indexOf("--images") < 0) {
            const queued = []
            for (let i = 0; i < _queue.length; i++) {
                const item = _queue[i]
                if (item.action === "preview" && item.args.indexOf("--images") < 0)
                    continue
                queued.push(item)
            }
            return queued
        }

        if (action === "notify") {
            const queued = []
            for (let i = 0; i < _queue.length; i++) {
                const item = _queue[i]
                if (item.action === "notify")
                    continue
                queued.push(item)
            }
            return queued
        }

        if (action !== "get" || args.indexOf("--images") >= 0)
            return _queue.slice()

        const queued = []
        for (let i = 0; i < _queue.length; i++) {
            const item = _queue[i]
            if (item.action === "get" && item.args.indexOf("--images") < 0)
                continue
            queued.push(item)
        }
        return queued
    }

    function startCommand(action, args) {
        _action = action
        _buffer = ""
        _errorBuffer = ""
        lastError = ""
        emailProc.command = [cliPath].concat(args)
        emailProc.running = true
    }

    function startNextCommand() {
        if (emailProc.running || _queue.length === 0)
            return

        const next = _queue[0]
        _queue = _queue.slice(1)
        startCommand(next.action, next.args)
    }

    function updateStatus(payload) {
        configured = !!payload.configured
        authenticated = !!payload.authenticated
        account = payload.account || ""
        credentialsPath = payload.credentialsPath || ""
        tokenPath = payload.tokenPath || ""
        tokenState = payload.tokenState || "missing"
        statusMessage = payload.message || (authenticated ? "Gmail ready" : "Connect Gmail")
    }

    function dispatch(payload) {
        if (_action === "status") {
            updateStatus(payload)
            statusReady(payload)
        } else if (_action === "auth") {
            updateStatus(payload)
            authReady(payload)
        } else if (_action === "list") {
            messagesReady(payload)
        } else if (_action === "get") {
            messageReady(payload)
        } else if (_action === "preview") {
            previewReady(payload)
        } else if (_action === "links") {
            linksReady(payload)
        } else if (_action === "settings") {
            settingsReady(payload)
        } else if (_action === "notify") {
            notifyReady(payload)
        } else if (_action === "send") {
            sendReady(payload)
        } else if (_action === "modify") {
            modifyReady(payload)
        } else if (_action === "view") {
            viewReady(payload)
        }
    }

    Process {
        id: emailProc
        command: []
        running: false

        stdout: SplitParser {
            onRead: data => client._buffer += data
        }

        stderr: SplitParser {
            onRead: data => client._errorBuffer += data
        }

        onExited: exitCode => {
            var payload = ({ ok: false, message: client._errorBuffer || client._buffer || "Email backend command failed" })
            try {
                if (client._buffer.trim() !== "")
                    payload = JSON.parse(client._buffer)
            } catch (e) {
                payload = ({ ok: false, message: client._buffer || String(e) })
            }

            if (exitCode !== 0 || !payload.ok) {
                client.lastError = payload.message || client._errorBuffer || "Email backend command failed"
                if (client._action === "status")
                    client.updateStatus(payload)
                client.failed(client._action, client.lastError)
            } else {
                client.dispatch(payload)
            }

            client._buffer = ""
            client._errorBuffer = ""
            client.startNextCommand()
        }
    }

    Component.onCompleted: {
        refreshSettings()
        refreshStatus()
    }
}
