import Quickshell.Io
import QtQuick

Item {
    id: root
    visible: false

    property var screenData: null
    property bool appVisible: true
    property bool settingsVisible: false
    property bool loading: true
    property string errorMsg: ""
    property string scriptPath: "/home/agony/GitHub/Bench/ScreenTime/bin/screentime-rs"
    property string selectedWeekStart: ""
    property bool refreshQueued: false
    property var serviceStatus: null
    property bool serviceBusy: false
    property string serviceErrorMsg: ""
    property string pendingServiceAction: "status"
    property string queuedServiceAction: ""
    property bool settingsBusy: false
    property string settingsErrorMsg: ""
    property string pendingSettingsAction: "status"
    property string pendingSettingsApp: ""
    property var queuedSettingsAction: null
    property bool privacyUnlocked: false
    property bool privacyAuthBusy: false
    property string privacyAuthErrorMsg: ""
    property var queuedPrivacySettingsAction: null
    property var doctorData: null
    property bool doctorBusy: false
    property string doctorErrorMsg: ""
    property bool maintenanceBusy: false
    property string maintenanceErrorMsg: ""

    function snapshotCommand() {
        var command = [root.scriptPath, "snapshot", "--json", "--limit", "12"]
        if (selectedWeekStart !== "")
            command.push("--week", selectedWeekStart)
        return command
    }

    function serviceCommand() {
        return [root.scriptPath, "service", root.pendingServiceAction, "--json"]
    }

    function settingsCommand() {
        var command = [root.scriptPath, "settings", root.pendingSettingsAction]
        if (root.pendingSettingsApp !== "")
            command.push(root.pendingSettingsApp)
        command.push("--json")
        return command
    }

    function authCommand() {
        return [root.scriptPath, "auth", "--json"]
    }

    function doctorCommand() {
        return [root.scriptPath, "doctor", "--json"]
    }

    function maintenanceCommand() {
        return [root.scriptPath, "maintenance", "compact-events", "--json"]
    }

    function refresh() {
        if (!appVisible)
            return
        if (snapshotProc.running) {
            refreshQueued = true
            return
        }
        loading = screenData === null
        errorMsg = ""
        snapshotProc.running = true
    }

    function selectWeek(weekStart) {
        var nextWeek = String(weekStart || "")
        if (nextWeek === "" || nextWeek === selectedWeekStart)
            return
        selectedWeekStart = nextWeek
        refresh()
    }

    function runServiceAction(action) {
        var nextAction = String(action || "status")
        if (serviceProc.running) {
            queuedServiceAction = nextAction
            return
        }
        pendingServiceAction = nextAction
        serviceBusy = nextAction !== "status"
        serviceErrorMsg = ""
        serviceProc.running = true
    }

    function refreshService() {
        runServiceAction("status")
    }

    function setCollectorEnabled(enabled) {
        runServiceAction(enabled ? "enable" : "disable")
    }

    function refreshDoctor() {
        if (!settingsVisible || doctorProc.running)
            return
        doctorBusy = true
        doctorErrorMsg = ""
        doctorProc.running = true
    }

    function compactEvents() {
        if (maintenanceProc.running)
            return
        maintenanceBusy = true
        maintenanceErrorMsg = ""
        maintenanceProc.running = true
    }

    function unlockPrivacy() {
        if (privacyUnlocked || privacyAuthBusy)
            return
        privacyAuthErrorMsg = ""
        privacyAuthBusy = true
        authProc.running = true
    }

    function lockPrivacy() {
        privacyUnlocked = false
        queuedPrivacySettingsAction = null
    }

    function runSettingsAction(action, appId) {
        var targetApp = String(appId || "")
        if (targetApp === "")
            return
        var nextAction = String(action || "")
        if (!privacyUnlocked && nextAction !== "status") {
            queuedPrivacySettingsAction = { "action": nextAction, "app": targetApp }
            unlockPrivacy()
            return
        }
        if (settingsProc.running) {
            queuedSettingsAction = { "action": nextAction, "app": targetApp }
            return
        }
        pendingSettingsAction = nextAction
        pendingSettingsApp = targetApp
        settingsBusy = true
        settingsErrorMsg = ""
        settingsProc.running = true
    }

    function setAppHidden(appId, hidden) {
        runSettingsAction(hidden ? "hide-app" : "show-app", appId)
    }

    function removeApp(appId) {
        runSettingsAction("remove-app", appId)
    }

    onAppVisibleChanged: {
        if (appVisible) {
            refresh()
            refreshService()
        }
    }

    onSettingsVisibleChanged: {
        if (settingsVisible) {
            refreshService()
            refreshDoctor()
        }
    }

    Process {
        id: snapshotProc
        command: root.snapshotCommand()
        running: true
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    var parsed = JSON.parse(this.text)
                    root.screenData = parsed
                    if (root.selectedWeekStart === "" && parsed.selected_week)
                        root.selectedWeekStart = String(parsed.selected_week)
                    root.errorMsg = ""
                } catch(e) {
                    root.errorMsg = "Erro ao parsear JSON"
                }
                root.loading = false
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.errorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            if (exitCode !== 0 && root.errorMsg === "")
                root.errorMsg = "Falha ao carregar ScreenTime"
            root.loading = false
            if (root.refreshQueued) {
                root.refreshQueued = false
                root.refresh()
            }
        }
    }

    Process {
        id: serviceProc
        command: root.serviceCommand()
        running: true
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    root.serviceStatus = JSON.parse(this.text)
                    root.serviceErrorMsg = root.serviceStatus && root.serviceStatus.ok === false
                        ? String(root.serviceStatus.last_error || "Falha ao controlar servico")
                        : ""
                } catch(e) {
                    root.serviceErrorMsg = "Erro ao parsear status do servico"
                }
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.serviceErrorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            if (exitCode !== 0 && root.serviceErrorMsg === "")
                root.serviceErrorMsg = "Falha ao controlar servico"
            root.serviceBusy = false
            if (root.pendingServiceAction !== "status")
                root.refresh()
            if (root.queuedServiceAction !== "") {
                var nextAction = root.queuedServiceAction
                root.queuedServiceAction = ""
                root.runServiceAction(nextAction)
            }
        }
    }

    Process {
        id: settingsProc
        command: root.settingsCommand()
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    JSON.parse(this.text)
                    root.settingsErrorMsg = ""
                } catch(e) {
                    root.settingsErrorMsg = "Erro ao parsear settings"
                }
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.settingsErrorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            if (exitCode !== 0 && root.settingsErrorMsg === "")
                root.settingsErrorMsg = "Falha ao atualizar settings"
            root.settingsBusy = false
            root.refresh()
            if (root.queuedSettingsAction !== null) {
                var nextAction = root.queuedSettingsAction
                root.queuedSettingsAction = null
                root.runSettingsAction(nextAction.action, nextAction.app)
            }
        }
    }

    Process {
        id: authProc
        command: root.authCommand()
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    var parsed = JSON.parse(this.text)
                    if (parsed && parsed.authenticated === true) {
                        root.privacyUnlocked = true
                        root.privacyAuthErrorMsg = ""
                    }
                } catch(e) {
                    root.privacyAuthErrorMsg = "Erro ao validar autenticacao"
                }
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.privacyAuthErrorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            root.privacyAuthBusy = false
            if (exitCode !== 0) {
                if (root.privacyAuthErrorMsg === "")
                    root.privacyAuthErrorMsg = "Autenticacao cancelada"
                root.queuedPrivacySettingsAction = null
                return
            }
            if (!root.privacyUnlocked && root.privacyAuthErrorMsg === "")
                root.privacyUnlocked = true
            if (root.queuedPrivacySettingsAction !== null) {
                var nextAction = root.queuedPrivacySettingsAction
                root.queuedPrivacySettingsAction = null
                root.runSettingsAction(nextAction.action, nextAction.app)
            }
        }
    }

    Process {
        id: doctorProc
        command: root.doctorCommand()
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    root.doctorData = JSON.parse(this.text)
                    root.doctorErrorMsg = ""
                } catch(e) {
                    root.doctorErrorMsg = "Erro ao parsear diagnostico"
                }
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.doctorErrorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            if (exitCode !== 0 && root.doctorErrorMsg === "")
                root.doctorErrorMsg = "Falha ao carregar diagnostico"
            root.doctorBusy = false
        }
    }

    Process {
        id: maintenanceProc
        command: root.maintenanceCommand()
        running: false
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    JSON.parse(this.text)
                    root.maintenanceErrorMsg = ""
                } catch(e) {
                    root.maintenanceErrorMsg = "Erro ao parsear manutencao"
                }
            }
        }
        stderr: StdioCollector {
            onStreamFinished: {
                if (this.text.trim().length > 0)
                    root.maintenanceErrorMsg = this.text.trim()
            }
        }
        onExited: exitCode => {
            if (exitCode !== 0 && root.maintenanceErrorMsg === "")
                root.maintenanceErrorMsg = "Falha ao executar manutencao"
            root.maintenanceBusy = false
            root.refreshDoctor()
            root.refresh()
        }
    }

    Timer {
        interval: root.settingsVisible ? 15000 : 5000
        running: root.appVisible
        repeat: true
        onTriggered: root.refresh()
    }

    Timer {
        interval: root.settingsVisible ? 10000 : 15000
        running: root.appVisible
        repeat: true
        onTriggered: root.refreshService()
    }

    Timer {
        interval: 20000
        running: root.appVisible && root.settingsVisible
        repeat: true
        onTriggered: root.refreshDoctor()
    }
}
