import Quickshell
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import "AstreaComponents" as Astrea
import "backend" as Backend
import "ui/panels" as Panels

ApplicationWindow {
    id: root

    visible: true
    width: 1180
    height: 760
    minimumWidth: 920
    minimumHeight: 560
    title: qsTr("DualSense")
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    font.family: Astrea.Theme.fontFamily
    font.pixelSize: Astrea.Theme.fontSizeNormal
    font.weight: Astrea.Theme.fontWeightNormal
    background: Rectangle { color: "transparent" }
    onClosing: Qt.quit()

    readonly property int pagePad: Astrea.Theme.pageMargin
    readonly property int sidebarWidth: sidebarCollapsed ? 62 : 218
    readonly property int presetPaneWidth: width < 1080 ? 320 : 360
    readonly property bool compactHeader: width < 1040
    readonly property color selectedBg: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.16)
    readonly property color selectedBorder: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.34)
    readonly property color softSurface: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035)
    readonly property color hoverSurface: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.045) : Qt.rgba(1, 1, 1, 0.060)
    readonly property string startSection: Quickshell.env("DUALSENSE_SECTION")
    readonly property var categoryCatalog: [
        { "id": "all", "label": qsTr("All") },
        { "id": "movement", "label": qsTr("Movement") },
        { "id": "combat", "label": qsTr("Combat") },
        { "id": "precision", "label": qsTr("Precision") },
        { "id": "driving", "label": qsTr("Driving") },
        { "id": "utility", "label": qsTr("Utility") }
    ]
    readonly property var presetCatalog: [
        {
            "id": "neutral",
            "label": qsTr("Neutro"),
            "kind": qsTr("Base"),
            "kindKey": "utility",
            "icon": "\uf11b",
            "description": qsTr("Remove efeitos dos gatilhos e deixa o controle pronto para jogos com suporte nativo."),
            "trigger": { "side": "both", "mode": "off" }
        },
        {
            "id": "walking",
            "label": qsTr("Andando"),
            "kind": qsTr("Movimento"),
            "kindKey": "movement",
            "icon": "\uf554",
            "description": qsTr("Pulso alternado leve, inspirado nos modos de passada/galloping para simular caminhada."),
            "trigger": { "side": "both", "mode": "galloping", "start": 1, "stop": 6, "first_foot": 2, "second_foot": 5, "frequency": 18 }
        },
        {
            "id": "running",
            "label": qsTr("Corrida"),
            "kind": qsTr("Movimento"),
            "kindKey": "movement",
            "icon": "\uf70c",
            "description": qsTr("Passadas mais rápidas e marcadas para corrida, perseguição ou sprint."),
            "trigger": { "side": "both", "mode": "galloping", "start": 1, "stop": 7, "first_foot": 3, "second_foot": 7, "frequency": 34 }
        },
        {
            "id": "sidearm",
            "label": qsTr("Arma curta"),
            "kind": qsTr("Combate"),
            "kindKey": "combat",
            "icon": "\ue3f7",
            "description": qsTr("Resistência com quebra curta no R2, parecido com o ponto de disparo de pistola."),
            "trigger": { "side": "right", "mode": "weapon", "start": 2, "stop": 7, "strength": 7 }
        },
        {
            "id": "rifle",
            "label": qsTr("Rajada"),
            "kind": qsTr("Combate"),
            "kindKey": "combat",
            "icon": "\uf05b",
            "description": qsTr("Vibração alternada rápida para armas automáticas, metralhadoras e impactos repetidos."),
            "trigger": { "side": "right", "mode": "machine", "start": 1, "stop": 8, "strength_a": 2, "strength_b": 7, "frequency": 30, "period": 18 }
        },
        {
            "id": "bow",
            "label": qsTr("Arco"),
            "kind": qsTr("Precisao"),
            "kindKey": "precision",
            "icon": "\uf6ad",
            "description": qsTr("Curva progressiva de tensão com soltura forte para arco, corda ou carregamento manual."),
            "trigger": { "side": "both", "mode": "bow", "start": 1, "stop": 8, "strength": 5, "snapforce": 8 }
        },
        {
            "id": "brake",
            "label": qsTr("Freio"),
            "kind": qsTr("Direcao"),
            "kindKey": "driving",
            "icon": "\uf1b9",
            "description": qsTr("Resistência firme no curso médio para freio, acelerador pesado ou embreagem."),
            "trigger": { "side": "both", "mode": "feedback", "position": 3, "strength": 8 }
        },
        {
            "id": "engine",
            "label": qsTr("Motor"),
            "kind": qsTr("Direcao"),
            "kindKey": "driving",
            "icon": "\uf085",
            "description": qsTr("Vibração estável nos gatilhos para motor, trilho, nave ou veículo em marcha."),
            "trigger": { "side": "both", "mode": "vibration", "position": 4, "amplitude": 4, "frequency": 32 }
        },
        {
            "id": "heavy_recoil",
            "label": qsTr("Recuo pesado"),
            "kind": qsTr("Combate"),
            "kindKey": "combat",
            "icon": "\uf140",
            "description": qsTr("Quebra mais rígida para escopeta, rifle pesado ou disparo de impacto."),
            "trigger": { "side": "right", "mode": "weapon", "start": 1, "stop": 8, "strength": 8 }
        },
        {
            "id": "lockpick",
            "label": qsTr("Trava fina"),
            "kind": qsTr("Precisao"),
            "kindKey": "precision",
            "icon": "\uf084",
            "description": qsTr("Resistência leve no fim do curso para lockpick, hack, zoom ou foco manual."),
            "trigger": { "side": "right", "mode": "feedback", "position": 6, "strength": 3 }
        },
        {
            "id": "terrain",
            "label": qsTr("Terreno"),
            "kind": qsTr("Movimento"),
            "kindKey": "movement",
            "icon": "\uf1bb",
            "description": qsTr("Vibração baixa e irregular para lama, neve, cascalho ou passos pesados."),
            "trigger": { "side": "both", "mode": "machine", "start": 2, "stop": 8, "strength_a": 1, "strength_b": 4, "frequency": 16, "period": 32 }
        },
        {
            "id": "turbine",
            "label": qsTr("Turbina"),
            "kind": qsTr("Direcao"),
            "kindKey": "driving",
            "icon": "\uf533",
            "description": qsTr("Pulso contínuo mais alto para nave, turbo, motor elétrico ou velocidade extrema."),
            "trigger": { "side": "both", "mode": "vibration", "position": 3, "amplitude": 6, "frequency": 46 }
        }
    ]

    property bool sidebarCollapsed: false
    property string selectedSection: ["presets", "lightbar", "audio", "device"].indexOf(startSection) >= 0 ? startSection : "presets"
    property string selectedPresetId: "walking"
    property string presetFilter: "all"
    property string presetSearch: ""

    function connectionLabel() {
        if (gamepadBackend.ready)
            return gamepadBackend.device !== "" ? gamepadBackend.device : qsTr("Controle conectado")
        if (!gamepadBackend.installed)
            return qsTr("dualsensectl nao instalado")
        return gamepadBackend.message || qsTr("Sem controle conectado")
    }

    function selectedPreset() {
        return presetById(selectedPresetId) || presetCatalog[0]
    }

    function presetById(presetId) {
        for (var i = 0; i < presetCatalog.length; i++) {
            if (presetCatalog[i].id === presetId)
                return presetCatalog[i]
        }
        return null
    }

    function filteredPresets() {
        var term = presetSearch.toLowerCase().trim()
        var results = []
        for (var i = 0; i < presetCatalog.length; i++) {
            var preset = presetCatalog[i]
            if (presetFilter !== "all" && preset.kindKey !== presetFilter)
                continue
            if (term === "") {
                results.push(preset)
                continue
            }
            var haystack = (preset.label + " " + preset.kind + " " + preset.description + " " + triggerModeLabel(preset.trigger.mode)).toLowerCase()
            if (haystack.indexOf(term) !== -1)
                results.push(preset)
        }
        return results
    }

    function triggerModeLabel(mode) {
        if (mode === "off")
            return qsTr("Off")
        if (mode === "feedback")
            return qsTr("Resistance")
        if (mode === "weapon")
            return qsTr("Weapon")
        if (mode === "bow")
            return qsTr("Bow")
        if (mode === "galloping")
            return qsTr("Galloping")
        if (mode === "machine")
            return qsTr("Machine")
        if (mode === "vibration")
            return qsTr("Vibration")
        return mode
    }

    function triggerSideLabel(side) {
        if (side === "left")
            return qsTr("L2")
        if (side === "right")
            return qsTr("R2")
        return qsTr("L2 + R2")
    }

    function triggerSummary(trigger) {
        if (!trigger || trigger.mode === "off")
            return qsTr("No adaptive trigger effect")

        var parts = [triggerSideLabel(trigger.side), triggerModeLabel(trigger.mode)]
        if (trigger.mode === "feedback")
            parts.push(qsTr("position %1").arg(trigger.position))
        if (trigger.mode === "weapon" || trigger.mode === "bow" || trigger.mode === "galloping" || trigger.mode === "machine")
            parts.push(qsTr("range %1-%2").arg(trigger.start).arg(trigger.stop))
        if (trigger.strength !== undefined)
            parts.push(qsTr("strength %1").arg(trigger.strength))
        if (trigger.frequency !== undefined)
            parts.push(qsTr("%1 Hz").arg(trigger.frequency))
        return parts.join("  /  ")
    }

    function presetTrigger(preset) {
        var next = {}
        var trigger = preset && preset.trigger ? preset.trigger : {}
        for (var key in trigger)
            next[key] = trigger[key]
        next.preset_id = preset ? preset.id : "custom"
        return next
    }

    function triggerArgs(trigger) {
        var values = ["--trigger-side", trigger.side || "both", "--mode", trigger.mode || "off"]
        if (trigger.mode === "feedback")
            return values.concat(["--position", trigger.position || 0, "--strength", trigger.strength || 0])
        if (trigger.mode === "weapon")
            return values.concat(["--start", trigger.start || 0, "--stop", trigger.stop || 0, "--strength", trigger.strength || 0])
        if (trigger.mode === "bow")
            return values.concat(["--start", trigger.start || 0, "--stop", trigger.stop || 0, "--strength", trigger.strength || 0, "--snapforce", trigger.snapforce || 0])
        if (trigger.mode === "galloping")
            return values.concat(["--start", trigger.start || 0, "--stop", trigger.stop || 0, "--first-foot", trigger.first_foot || 0, "--second-foot", trigger.second_foot || 0, "--frequency", trigger.frequency || 0])
        if (trigger.mode === "machine")
            return values.concat(["--start", trigger.start || 0, "--stop", trigger.stop || 0, "--strength-a", trigger.strength_a || 0, "--strength-b", trigger.strength_b || 0, "--frequency", trigger.frequency || 0, "--period", trigger.period || 0])
        if (trigger.mode === "vibration")
            return values.concat(["--position", trigger.position || 0, "--amplitude", trigger.amplitude || 0, "--frequency", trigger.frequency || 0])
        return values
    }

    function applyButtonText() {
        if (gamepadBackend.applying)
            return qsTr("Applying")
        return gamepadBackend.ready ? qsTr("Apply") : qsTr("Save offline")
    }

    function applyPreset(preset) {
        if (!preset)
            return
        selectedPresetId = preset.id
        var trigger = presetTrigger(preset)
        triggerPanel.setTriggerConfig(trigger, true)
        if (gamepadBackend.ready)
            gamepadBackend.applyAction("trigger", triggerArgs(trigger), false)
        else
            gamepadBackend.feedbackMessage = qsTr("Saved offline")
    }

    Backend.DualSenseBackend {
        id: gamepadBackend
    }

    Connections {
        target: gamepadBackend
        function onConfigLoaded() {
            var trigger = gamepadBackend.config && gamepadBackend.config.trigger ? gamepadBackend.config.trigger : null
            if (trigger && trigger.preset_id && root.presetById(trigger.preset_id))
                root.selectedPresetId = trigger.preset_id
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        color: "transparent"
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(0, 0, 0, 0.62)
            shadowBlur: 1.0
            shadowVerticalOffset: 8
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 22
        color: Astrea.Theme.windowBackground
        border.width: 1
        border.color: Astrea.Theme.windowBorder
        clip: true

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

            Astrea.SidebarFrame {
                Layout.preferredWidth: root.sidebarWidth
                Layout.fillHeight: true
                topMargin: 0
                bottomMargin: 0
                leftMargin: 0
                rightMargin: 0
                cornerRadius: 18
                contentTopPadding: Astrea.Theme.spacingMedium
                contentBottomPadding: Astrea.Theme.spacingLarge
                contentSpacing: Astrea.Theme.spacingTiny

                Behavior on Layout.preferredWidth {
                    NumberAnimation {
                        duration: Astrea.Theme.animationNormal
                        easing.type: Easing.OutCubic
                    }
                }

                Item {
                    width: parent.width
                    height: 36

                    Astrea.SidebarCollapseButton {
                        anchors {
                            right: parent.right
                            rightMargin: root.sidebarCollapsed ? 13 : 14
                            verticalCenter: parent.verticalCenter
                        }
                        collapsed: root.sidebarCollapsed
                        controlSize: 30
                        onClicked: root.sidebarCollapsed = !root.sidebarCollapsed
                    }
                }

                Column {
                    width: parent.width - 28
                    x: 14
                    spacing: 2
                    visible: !root.sidebarCollapsed
                    opacity: root.sidebarCollapsed ? 0 : 1

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Astrea.Theme.animationQuick
                            easing.type: Easing.OutCubic
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("DualSense")
                        color: Astrea.Theme.textPrimary
                        font.family: Astrea.Theme.fontFamily
                        font.pixelSize: Astrea.Theme.fontSizeLarge
                        font.weight: Astrea.Theme.fontWeightDemiBold
                        elide: Text.ElideRight
                    }

                    Row {
                        spacing: 7

                        Astrea.StatusDot {
                            anchors.verticalCenter: parent.verticalCenter
                            active: gamepadBackend.ready
                            pulse: gamepadBackend.applying
                        }

                        Text {
                            width: parent.parent.width - 20
                            text: gamepadBackend.ready ? qsTr("Connected") : qsTr("Offline controls")
                            color: Astrea.Theme.textSecondary
                            font.family: Astrea.Theme.fontFamily
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    width: parent.width - 28
                    x: 14
                    height: 1
                    color: Astrea.Theme.cardBorder
                    visible: !root.sidebarCollapsed
                }

                Astrea.Button {
                    width: parent.width - 16
                    height: 36
                    x: 8
                    text: root.sidebarCollapsed ? "" : root.applyButtonText()
                    iconText: "\uf00c"
                    iconFontFamily: "JetBrainsMono Nerd Font"
                    primary: gamepadBackend.ready
                    flat: !gamepadBackend.ready
                    controlHeight: 36
                    enabled: root.selectedSection === "presets" && !gamepadBackend.applying
                    onClicked: root.applyPreset(root.selectedPreset())
                }

                Item { width: 1; height: 5 }

                Astrea.NavItem {
                    width: parent.width
                    label: root.sidebarCollapsed ? "" : qsTr("Presets")
                    sym: "\uf0ad"
                    selected: root.selectedSection === "presets"
                    onClicked: root.selectedSection = "presets"
                }

                Astrea.NavItem {
                    width: parent.width
                    label: root.sidebarCollapsed ? "" : qsTr("Lightbar")
                    sym: "\uf53f"
                    selected: root.selectedSection === "lightbar"
                    onClicked: root.selectedSection = "lightbar"
                }

                Astrea.NavItem {
                    width: parent.width
                    label: root.sidebarCollapsed ? "" : qsTr("Audio")
                    sym: "\uf130"
                    selected: root.selectedSection === "audio"
                    onClicked: root.selectedSection = "audio"
                }

                Astrea.NavItem {
                    width: parent.width
                    label: root.sidebarCollapsed ? "" : qsTr("Device")
                    sym: "\uf2db"
                    selected: root.selectedSection === "device"
                    onClicked: root.selectedSection = "device"
                }

                Item {
                    width: 1
                    height: Math.max(16, parent.height - (root.sidebarCollapsed ? 350 : 420))
                }

                Rectangle {
                    width: parent.width - 28
                    x: 14
                    height: 1
                    color: Astrea.Theme.cardBorder
                    opacity: 0.8
                    visible: !root.sidebarCollapsed
                }

                Astrea.NavItem {
                    width: parent.width
                    label: root.sidebarCollapsed ? "" : qsTr("Refresh")
                    sym: "\uf01e"
                    selected: false
                    onClicked: gamepadBackend.refresh()
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
                            Layout.fillWidth: true
                            text: root.selectedSection === "presets" ? qsTr("Adaptive Presets") :
                                (root.selectedSection === "lightbar" ? qsTr("Lighting") :
                                (root.selectedSection === "audio" ? qsTr("Audio and Haptics") : qsTr("Device")))
                            textColor: Astrea.Theme.textPrimary
                            font.pixelSize: Astrea.Theme.fontSizeHeader
                            font.weight: Astrea.Theme.fontWeightDemiBold
                            elide: Text.ElideRight
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: root.connectionLabel()
                                + (gamepadBackend.battery !== "" ? "  /  " + gamepadBackend.battery : "")
                                + (root.selectedSection === "presets" ? "  /  " + root.selectedPreset().label : "")
                            textColor: Astrea.Theme.textSecondary
                            font.pixelSize: Astrea.Theme.fontSizeNormal
                            elide: Text.ElideRight
                        }
                    }

                    Astrea.SearchField {
                        visible: root.selectedSection === "presets" && !root.compactHeader
                        Layout.preferredWidth: Math.min(320, Math.max(220, root.width * 0.24))
                        placeholderText: qsTr("Search presets")
                        text: root.presetSearch
                        onTextEdited: value => root.presetSearch = value
                        onCleared: root.presetSearch = ""
                    }

                    Astrea.Button {
                        text: ""
                        iconText: "\uf01e"
                        iconFontFamily: "JetBrainsMono Nerd Font"
                        controlWidth: 38
                        controlHeight: 36
                        flat: true
                        enabled: !gamepadBackend.loading
                        onClicked: gamepadBackend.refresh()
                    }

                    Astrea.Button {
                        visible: root.selectedSection === "presets"
                        text: root.applyButtonText()
                        iconText: "\uf00c"
                        iconFontFamily: "JetBrainsMono Nerd Font"
                        primary: gamepadBackend.ready
                        flat: !gamepadBackend.ready
                        enabled: !gamepadBackend.applying
                        onClicked: root.applyPreset(root.selectedPreset())
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Astrea.Theme.spacingLarge
                    visible: root.selectedSection === "presets"

                    Rectangle {
                        Layout.preferredWidth: root.presetPaneWidth
                        Layout.fillHeight: true
                        radius: Astrea.Theme.cardRadius
                        color: Astrea.Theme.cardBg
                        border.width: 1
                        border.color: Astrea.Theme.cardBorder
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Astrea.Theme.spacingMedium
                            spacing: Astrea.Theme.spacing

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Astrea.Theme.spacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Astrea.TextLabel {
                                        text: qsTr("Presets")
                                        textColor: Astrea.Theme.textPrimary
                                        font.pixelSize: Astrea.Theme.fontSizeTitle
                                        font.weight: Astrea.Theme.fontWeightDemiBold
                                    }

                                    Astrea.TextLabel {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 profiles").arg(root.filteredPresets().length)
                                        textColor: Astrea.Theme.textSecondary
                                        font.pixelSize: Astrea.Theme.fontSizeSmall
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Astrea.SearchField {
                                visible: root.compactHeader
                                Layout.fillWidth: true
                                placeholderText: qsTr("Search presets")
                                text: root.presetSearch
                                onTextEdited: value => root.presetSearch = value
                                onCleared: root.presetSearch = ""
                            }

                            Flow {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(30, implicitHeight)
                                spacing: Astrea.Theme.spacingSmall

                                Repeater {
                                    model: root.categoryCatalog

                                    delegate: Rectangle {
                                        id: categoryChip

                                        required property var modelData

                                        readonly property bool active: root.presetFilter === modelData.id

                                        width: categoryLabel.implicitWidth + 24
                                        height: 30
                                        radius: 15
                                        color: active
                                            ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.18)
                                            : (categoryArea.containsMouse ? root.hoverSurface : root.softSurface)
                                        border.width: 1
                                        border.color: active ? root.selectedBorder : Astrea.Theme.cardBorder

                                        Text {
                                            id: categoryLabel
                                            anchors.centerIn: parent
                                            text: categoryChip.modelData.label
                                            color: categoryChip.active ? Astrea.Theme.accent : Astrea.Theme.textSecondary
                                            font.family: Astrea.Theme.fontFamily
                                            font.pixelSize: Astrea.Theme.fontSizeSmall
                                            font.weight: categoryChip.active ? Astrea.Theme.fontWeightDemiBold : Astrea.Theme.fontWeightMedium
                                        }

                                        MouseArea {
                                            id: categoryArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.presetFilter = categoryChip.modelData.id
                                        }
                                    }
                                }
                            }

                            ListView {
                                id: presetList

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: root.filteredPresets()
                                spacing: Astrea.Theme.spacingSmall
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Rectangle {
                                    id: presetRow

                                    required property int index
                                    required property var modelData

                                    width: presetList.width
                                    height: Math.max(78, rowContent.implicitHeight + 22)
                                    radius: Astrea.Theme.controlRadius + 4
                                    color: presetRow.modelData.id === root.selectedPresetId
                                        ? root.selectedBg
                                        : (rowArea.containsMouse ? root.hoverSurface : root.softSurface)
                                    border.width: 1
                                    border.color: presetRow.modelData.id === root.selectedPresetId ? root.selectedBorder : Astrea.Theme.cardBorder

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: Astrea.Theme.animationFast
                                            easing.type: Easing.OutCubic
                                        }
                                    }

                                    RowLayout {
                                        id: rowContent

                                        anchors.fill: parent
                                        anchors.margins: Astrea.Theme.spacingMedium
                                        spacing: Astrea.Theme.spacingMedium

                                        Rectangle {
                                            Layout.preferredWidth: 36
                                            Layout.preferredHeight: 36
                                            radius: 10
                                            color: presetRow.modelData.id === root.selectedPresetId
                                                ? Astrea.Theme.accent
                                                : (Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.070))

                                            Text {
                                                anchors.centerIn: parent
                                                text: presetRow.modelData.icon
                                                color: presetRow.modelData.id === root.selectedPresetId ? Astrea.Theme.accentForeground : Astrea.Theme.textSecondary
                                                font.family: "JetBrainsMono Nerd Font"
                                                font.pixelSize: Astrea.Theme.fontSizeNormal
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Astrea.Theme.spacingSmall

                                                Astrea.TextLabel {
                                                    Layout.fillWidth: true
                                                    text: presetRow.modelData.label
                                                    textColor: Astrea.Theme.textPrimary
                                                    font.pixelSize: Astrea.Theme.fontSizeNormal
                                                    font.weight: Astrea.Theme.fontWeightDemiBold
                                                    elide: Text.ElideRight
                                                }

                                                Astrea.TextLabel {
                                                    text: presetRow.modelData.kind
                                                    textColor: Astrea.Theme.textTertiary
                                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                                }
                                            }

                                            Astrea.TextLabel {
                                                Layout.fillWidth: true
                                                text: root.triggerSummary(presetRow.modelData.trigger)
                                                textColor: Astrea.Theme.textSecondary
                                                font.pixelSize: Astrea.Theme.fontSizeSmall
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: rowArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.selectedPresetId = presetRow.modelData.id
                                        onDoubleClicked: root.applyPreset(presetRow.modelData)
                                    }
                                }
                            }
                        }
                    }

                    ScrollView {
                        id: presetDetailScroll

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: availableWidth
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        ColumnLayout {
                            width: presetDetailScroll.availableWidth
                            spacing: Astrea.Theme.spacingLarge

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(190, presetDetailLayout.implicitHeight + Astrea.Theme.spacingXLarge * 2)
                                radius: Astrea.Theme.cardRadius
                                color: Astrea.Theme.cardBg
                                border.width: 1
                                border.color: Astrea.Theme.cardBorder

                                ColumnLayout {
                                    id: presetDetailLayout

                                    anchors.fill: parent
                                    anchors.margins: Astrea.Theme.spacingXLarge
                                    spacing: Astrea.Theme.spacingLarge

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Astrea.Theme.spacingLarge

                                        Rectangle {
                                            Layout.preferredWidth: 58
                                            Layout.preferredHeight: 58
                                            radius: 16
                                            color: Astrea.Theme.accent

                                            Text {
                                                anchors.centerIn: parent
                                                text: root.selectedPreset().icon
                                                color: Astrea.Theme.accentForeground
                                                font.family: "JetBrainsMono Nerd Font"
                                                font.pixelSize: 24
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Astrea.DisplayLabel {
                                                Layout.fillWidth: true
                                                text: root.selectedPreset().label
                                                textColor: Astrea.Theme.textPrimary
                                                font.pixelSize: Astrea.Theme.fontSizeHeader
                                                font.weight: Astrea.Theme.fontWeightDemiBold
                                                elide: Text.ElideRight
                                            }

                                            Astrea.TextLabel {
                                                Layout.fillWidth: true
                                                text: root.selectedPreset().description
                                                textColor: Astrea.Theme.textSecondary
                                                font.pixelSize: Astrea.Theme.fontSizeNormal
                                                wrapMode: Text.WordWrap
                                            }
                                        }

                                        Astrea.Button {
                                            text: root.applyButtonText()
                                            iconText: "\uf00c"
                                            iconFontFamily: "JetBrainsMono Nerd Font"
                                            primary: gamepadBackend.ready
                                            flat: !gamepadBackend.ready
                                            enabled: !gamepadBackend.applying
                                            onClicked: root.applyPreset(root.selectedPreset())
                                        }
                                    }

                                    Astrea.Divider {
                                        lineColor: Astrea.Theme.cardBorder
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Astrea.Theme.spacing

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 54
                                            radius: Astrea.Theme.controlRadius + 4
                                            color: root.softSurface
                                            border.width: 1
                                            border.color: Astrea.Theme.cardBorder

                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: root.triggerSideLabel(root.selectedPreset().trigger.side)
                                                    textColor: Astrea.Theme.textPrimary
                                                    font.pixelSize: Astrea.Theme.fontSizeNormal
                                                    font.weight: Astrea.Theme.fontWeightDemiBold
                                                }

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: qsTr("Trigger")
                                                    textColor: Astrea.Theme.textTertiary
                                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                                }
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 54
                                            radius: Astrea.Theme.controlRadius + 4
                                            color: root.softSurface
                                            border.width: 1
                                            border.color: Astrea.Theme.cardBorder

                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: root.triggerModeLabel(root.selectedPreset().trigger.mode)
                                                    textColor: Astrea.Theme.textPrimary
                                                    font.pixelSize: Astrea.Theme.fontSizeNormal
                                                    font.weight: Astrea.Theme.fontWeightDemiBold
                                                }

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: qsTr("Mode")
                                                    textColor: Astrea.Theme.textTertiary
                                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                                }
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 54
                                            radius: Astrea.Theme.controlRadius + 4
                                            color: root.softSurface
                                            border.width: 1
                                            border.color: Astrea.Theme.cardBorder

                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: gamepadBackend.ready ? qsTr("Live apply") : qsTr("Saved offline")
                                                    textColor: gamepadBackend.ready ? Astrea.Theme.successColor : Astrea.Theme.textPrimary
                                                    font.pixelSize: Astrea.Theme.fontSizeNormal
                                                    font.weight: Astrea.Theme.fontWeightDemiBold
                                                }

                                                Astrea.TextLabel {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: qsTr("Apply state")
                                                    textColor: Astrea.Theme.textTertiary
                                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Panels.AdaptiveTriggerPanel {
                                id: triggerPanel
                                Layout.fillWidth: true
                                backend: gamepadBackend
                            }
                        }
                    }
                }

                ScrollView {
                    id: lightbarScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.selectedSection === "lightbar"
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: lightbarScroll.availableWidth
                        spacing: Astrea.Theme.spacingLarge

                        Panels.LightbarPanel {
                            Layout.fillWidth: true
                            backend: gamepadBackend
                        }

                        Panels.LedPanel {
                            Layout.fillWidth: true
                            backend: gamepadBackend
                        }
                    }
                }

                ScrollView {
                    id: audioScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.selectedSection === "audio"
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: audioScroll.availableWidth
                        spacing: Astrea.Theme.spacingLarge

                        Panels.MicrophonePanel {
                            Layout.fillWidth: true
                            backend: gamepadBackend
                        }

                        Panels.AttenuationPanel {
                            Layout.fillWidth: true
                            backend: gamepadBackend
                        }
                    }
                }

                ScrollView {
                    id: deviceScroll

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.selectedSection === "device"
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: deviceScroll.availableWidth
                        spacing: Astrea.Theme.spacingLarge

                        Panels.DevicePanel {
                            Layout.fillWidth: true
                            backend: gamepadBackend
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(144, infoText.implicitHeight + Astrea.Theme.spacingXLarge * 2)
                            radius: Astrea.Theme.cardRadius
                            color: Astrea.Theme.cardBg
                            border.width: 1
                            border.color: Astrea.Theme.cardBorder

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Astrea.Theme.spacingXLarge
                                spacing: Astrea.Theme.spacingMedium

                                Astrea.SectionHeader {
                                    Layout.fillWidth: true
                                    text: qsTr("Firmware and bridge")
                                }

                                Astrea.TextLabel {
                                    id: infoText
                                    Layout.fillWidth: true
                                    text: gamepadBackend.info !== "" ? gamepadBackend.info : qsTr("Conecte um DualSense para ver detalhes de firmware. As configuracoes continuam editaveis offline.")
                                    textColor: Astrea.Theme.textSecondary
                                    font.family: Astrea.Theme.monoFontFamily
                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: 34
            color: "transparent"
            visible: gamepadBackend.applying || gamepadBackend.lastError !== "" || gamepadBackend.feedbackMessage !== ""

            Astrea.TextLabel {
                anchors.centerIn: parent
                text: gamepadBackend.applying ? qsTr("Applying changes...") : (gamepadBackend.lastError !== "" ? gamepadBackend.lastError : gamepadBackend.feedbackMessage)
                textColor: gamepadBackend.lastError !== "" ? Astrea.Theme.errorColor : Astrea.Theme.textSecondary
                font.pixelSize: Astrea.Theme.fontSizeSmall
            }
        }
    }
}
