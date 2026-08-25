import QtQuick 2.15

// Presentation lifecycle for file/archive operations. Native state remains
// truthful; this object owns only display arbitration and retention timing.
Item {
    id: root

    property QtObject operationState: null

    property int minimumRunningDisplayMs: 500
    property int successHoldMs: 1200
    property int partialSuccessHoldMs: 2200
    property int failureHoldMs: 5000
    property int cancelledHoldMs: 1500
    property int fadeOutMs: 160

    readonly property bool cardVisible: phase !== "hidden"
    readonly property bool indeterminate: totalItems <= 0 && percent <= 0 && !failed

    property string phase: "hidden"
    property real cardOpacity: 0
    property string activeKind: ""
    property string title: ""
    property string detail: ""
    property string destination: ""
    property real progress: 0
    property int percent: 0
    property int completedItems: 0
    property int totalItems: 0
    property string remainingText: ""
    property string error: ""
    property bool failed: false
    property bool terminal: false
    property string terminalState: ""
    property int generation: 0

    property string _runningKind: ""
    property double _runningStartedAt: 0
    property int _minimumGeneration: 0
    property int _holdGeneration: 0
    property int _fadeGeneration: 0
    property var _latestFileSnapshot: ({})
    property var _latestArchiveSnapshot: ({})
    property var _pendingTerminalSnapshot: null
    property string _pendingTerminalKind: ""
    property int _pendingTerminalGeneration: 0

    width: 0
    height: 0
    visible: false

    function _value(object, name, fallback) {
        if (!object || object[name] === undefined || object[name] === null)
            return fallback
        return object[name]
    }

    function _snapshotValue(snapshot, name, fallback) {
        return _value(snapshot, name, fallback)
    }

    function _liveKind() {
        // Archive work has priority while active. A file operation resumes
        // from its latest snapshot as soon as archive work ends.
        if (_snapshotValue(_latestArchiveSnapshot, "running", false))
            return "archive"
        if (_snapshotValue(_latestFileSnapshot, "running", false))
            return "file"
        return ""
    }

    function _kindSnapshot(kind) {
        return kind === "archive" ? _latestArchiveSnapshot : _latestFileSnapshot
    }

    function _basename(path) {
        var value = String(path || "").replace(/\\+$/, "")
        if (value === "")
            return ""
        var slash = value.lastIndexOf("/")
        return slash >= 0 ? value.substring(slash + 1) : value
    }

    function _copyLive(kind, snapshot) {
        activeKind = kind
        terminal = false
        terminalState = ""
        title = _snapshotValue(snapshot, "status", "")
        if (title === "")
            title = kind === "archive" ? "Extracting..." : "Copying..."
        detail = _snapshotValue(snapshot, "fileName", "")
        destination = _basename(_snapshotValue(snapshot, "destination", ""))
        progress = kind === "archive"
            ? 0 : Number(_snapshotValue(snapshot, "progress", 0))
        percent = kind === "archive"
            ? 0 : Number(_snapshotValue(snapshot, "percent", 0))
        completedItems = kind === "archive"
            ? 0 : Number(_snapshotValue(snapshot, "doneCount", 0))
        totalItems = kind === "archive"
            ? 0 : Number(_snapshotValue(snapshot, "totalCount", 0))
        remainingText = kind === "archive"
            ? (_snapshotValue(snapshot, "remainingText", "") || "Aguardando...")
            : ""
        error = _snapshotValue(snapshot, "error", "")
        failed = false
    }

    function _lower(value) {
        return String(value || "").toLowerCase()
    }

    function _terminalSemanticState(kind, snapshot) {
        var state = _lower(_snapshotValue(snapshot, "state", ""))
        var status = _lower(_snapshotValue(snapshot, "status", ""))
        var error = String(_snapshotValue(snapshot, "error", "") || "")
        if (state === "cancelled" || state === "canceled"
                || status.indexOf("cancel") >= 0)
            return "cancelled"
        if (state === "partial-success" || state === "partial_success"
                || state === "partial")
            return "partial-success"
        if (state === "failed" || error !== ""
                || status.indexOf("fail") >= 0 || status.indexOf("falha") >= 0
                || status.indexOf("error") >= 0 || status.indexOf("erro") >= 0)
            return "failed"
        return "success"
    }

    function _copyTerminal(kind, snapshot) {
        var state = _terminalSemanticState(kind, snapshot)
        activeKind = kind
        terminal = true
        terminalState = state
        detail = _snapshotValue(snapshot, "fileName", "")
        destination = _basename(_snapshotValue(snapshot, "destination", ""))
        remainingText = ""
        error = _snapshotValue(snapshot, "error", "")

        if (kind === "archive") {
            if (state === "success") {
                progress = 1
                percent = 100
                completedItems = 1
                totalItems = 1
            } else {
                progress = 0
                percent = 0
                completedItems = 0
                totalItems = 0
            }
        } else {
            progress = Number(_snapshotValue(snapshot, "progress", 0))
            percent = Number(_snapshotValue(snapshot, "percent", 0))
            completedItems = Number(_snapshotValue(snapshot, "doneCount", 0))
            totalItems = Number(_snapshotValue(snapshot, "totalCount", 0))
        }

        if (state === "success") {
            title = "Completed"
            failed = false
        } else if (state === "partial-success") {
            title = "Completed with errors"
            failed = true
        } else if (state === "cancelled") {
            title = "Cancelled"
            failed = false
        } else {
            title = "Failed"
            failed = true
        }
    }

    function _cancelScheduledPresentation() {
        minimumTimer.stop()
        holdTimer.stop()
        fadeAnimation.stop()
    }

    function _clearPendingTerminal() {
        _pendingTerminalSnapshot = null
        _pendingTerminalKind = ""
        _pendingTerminalGeneration = 0
    }

    function _beginLive(kind) {
        generation += 1
        _cancelScheduledPresentation()
        _clearPendingTerminal()
        _runningKind = kind
        _runningStartedAt = Date.now()
        phase = "running"
        cardOpacity = 1
        _copyLive(kind, _kindSnapshot(kind))
    }

    function _beginTerminal(expectedGeneration) {
        if (expectedGeneration !== generation || _runningKind !== ""
                || _pendingTerminalGeneration !== expectedGeneration
                || _pendingTerminalKind === "")
            return

        var kind = _pendingTerminalKind
        var snapshot = _pendingTerminalSnapshot
        _clearPendingTerminal()
        _copyTerminal(kind, snapshot)
        phase = "terminal"
        cardOpacity = 1
        var holdMs = successHoldMs
        if (terminalState === "partial-success")
            holdMs = partialSuccessHoldMs
        else if (terminalState === "failed")
            holdMs = failureHoldMs
        else if (terminalState === "cancelled")
            holdMs = cancelledHoldMs
        _holdGeneration = expectedGeneration
        holdTimer.interval = Math.max(0, holdMs)
        holdTimer.start()
    }

    function _beginFade(expectedGeneration) {
        if (expectedGeneration !== generation || _runningKind !== "")
            return
        phase = "fading"
        _fadeGeneration = expectedGeneration
        if (fadeOutMs <= 0) {
            _hide(expectedGeneration)
            return
        }
        fadeAnimation.duration = fadeOutMs
        fadeAnimation.start()
    }

    function _hide(expectedGeneration) {
        if (expectedGeneration !== generation)
            return
        phase = "hidden"
        cardOpacity = 0
        activeKind = ""
        terminal = false
        terminalState = ""
    }

    function _reconcilePresentation() {
        var liveKind = _liveKind()
        if (liveKind !== "") {
            if (_runningKind !== liveKind || phase === "hidden" || terminal)
                _beginLive(liveKind)
            else
                _copyLive(liveKind, _kindSnapshot(liveKind))
            return
        }

        if (_runningKind === "")
            return

        var endedKind = _runningKind
        _runningKind = ""
        _pendingTerminalKind = endedKind
        _pendingTerminalSnapshot = _kindSnapshot(endedKind)
        _pendingTerminalGeneration = generation
        terminal = false
        terminalState = ""
        var elapsed = Date.now() - _runningStartedAt
        var remaining = Math.max(0, minimumRunningDisplayMs - elapsed)
        if (remaining > 0) {
            phase = "running"
            cardOpacity = 1
            _minimumGeneration = generation
            minimumTimer.interval = remaining
            minimumTimer.start()
        } else {
            _beginTerminal(generation)
        }
    }

    function syncFromOperationState() {
        if (!operationState)
            return
        _latestFileSnapshot = operationState.currentFileOperationSnapshot()
        _latestArchiveSnapshot = operationState.currentArchiveOperationSnapshot()
        _reconcilePresentation()
    }

    function onMinimumTimer() {
        _beginTerminal(_minimumGeneration)
    }

    function onHoldTimer() {
        _beginFade(_holdGeneration)
    }

    function onFadeStopped() {
        if (phase === "fading")
            _hide(_fadeGeneration)
    }

    onOperationStateChanged: syncFromOperationState()

    Component.onCompleted: syncFromOperationState()

    Timer {
        id: minimumTimer
        repeat: false
        onTriggered: root.onMinimumTimer()
    }

    Timer {
        id: holdTimer
        repeat: false
        onTriggered: root.onHoldTimer()
    }

    NumberAnimation {
        id: fadeAnimation
        target: root
        property: "cardOpacity"
        from: 1
        to: 0
        onStopped: root.onFadeStopped()
    }

    Connections {
        target: root.operationState
        function onFileOperationChanged(snapshot) {
            root._latestFileSnapshot = snapshot
            root._reconcilePresentation()
        }
        function onArchiveOperationChanged(snapshot) {
            root._latestArchiveSnapshot = snapshot
            root._reconcilePresentation()
        }
    }
}
