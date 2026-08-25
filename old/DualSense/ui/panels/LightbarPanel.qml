import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea
import "../controls" as Control

Control.Card {
    id: panel
    property var backend
    property int red: 0
    property int green: 122
    property int blue: 255
    property int brightness: 180
    property string state: "on"
    readonly property var colorPresets: [
        { "label": qsTr("Astrea"), "red": 255, "green": 48, "blue": 105 },
        { "label": qsTr("Ice"), "red": 90, "green": 190, "blue": 255 },
        { "label": qsTr("Violet"), "red": 155, "green": 105, "blue": 255 },
        { "label": qsTr("Emerald"), "red": 40, "green": 210, "blue": 140 },
        { "label": qsTr("Amber"), "red": 255, "green": 178, "blue": 66 },
        { "label": qsTr("White"), "red": 255, "green": 255, "blue": 255 }
    ]

    function restoreConfig() {
        if (!backend || !backend.config || !backend.config.lightbar)
            return
        var saved = backend.config.lightbar
        red = saved.red === undefined ? red : saved.red
        green = saved.green === undefined ? green : saved.green
        blue = saved.blue === undefined ? blue : saved.blue
        brightness = saved.brightness === undefined ? brightness : saved.brightness
        state = saved.state === undefined ? state : saved.state
    }

    function saveLightbarConfig() {
        if (!backend)
            return
        backend.saveConfig({
            "lightbar": {
                "red": red,
                "green": green,
                "blue": blue,
                "brightness": brightness,
                "state": state
            }
        })
    }

    function applyColor() {
        saveLightbarConfig()
        if (backend && backend.ready)
            backend.applyLiveAction("lightbar", ["--red", red, "--green", green, "--blue", blue, "--brightness", brightness])
    }

    function queueColorApply() {
        colorApplyTimer.restart()
    }

    function setLightbarColor(colorPreset) {
        red = colorPreset.red
        green = colorPreset.green
        blue = colorPreset.blue
        if (brightness < 90)
            brightness = 180
        queueColorApply()
    }

    Timer {
        id: colorApplyTimer
        interval: 0
        repeat: false
        onTriggered: panel.applyColor()
    }

    Component.onCompleted: restoreConfig()

    Connections {
        target: panel.backend
        function onConfigLoaded() {
            panel.restoreConfig()
        }
    }

    Astrea.SectionHeader {
        Layout.fillWidth: true
        text: qsTr("Lightbar")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Astrea.Theme.spacingLarge

        Rectangle {
            Layout.preferredWidth: 78
            Layout.preferredHeight: 78
            radius: 16
            color: Qt.rgba(red / 255, green / 255, blue / 255, Math.max(0.24, brightness / 255))
            border.width: 1
            border.color: Astrea.Theme.cardBorder

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                radius: 12
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.18)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Control.ValueSlider { Layout.fillWidth: true; label: qsTr("Red"); value: panel.red; onEdited: value => { panel.red = value; panel.queueColorApply() } }
            Control.ValueSlider { Layout.fillWidth: true; label: qsTr("Green"); value: panel.green; onEdited: value => { panel.green = value; panel.queueColorApply() } }
            Control.ValueSlider { Layout.fillWidth: true; label: qsTr("Blue"); value: panel.blue; onEdited: value => { panel.blue = value; panel.queueColorApply() } }
            Control.ValueSlider { Layout.fillWidth: true; label: qsTr("Brightness"); value: panel.brightness; onEdited: value => { panel.brightness = value; panel.queueColorApply() } }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Astrea.Theme.spacingSmall

        Astrea.TextLabel {
            text: qsTr("Swatches")
            textColor: Astrea.Theme.textSecondary
            font.pixelSize: Astrea.Theme.fontSizeSmall
            font.weight: Astrea.Theme.fontWeightDemiBold
        }

        Repeater {
            model: panel.colorPresets

            delegate: Rectangle {
                id: swatch

                required property var modelData

                Layout.preferredWidth: 34
                Layout.preferredHeight: 28
                radius: 10
                color: Qt.rgba(modelData.red / 255, modelData.green / 255, modelData.blue / 255, 1)
                border.width: 1
                border.color: swatchArea.containsMouse ? Astrea.Theme.accent : Astrea.Theme.cardBorder

                MouseArea {
                    id: swatchArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: panel.setLightbarColor(swatch.modelData)
                }
            }
        }

        Item { Layout.fillWidth: true }

        Astrea.TextLabel {
            text: panel.backend && panel.backend.ready ? qsTr("Live") : qsTr("Saved offline")
            textColor: panel.backend && panel.backend.ready ? Astrea.Theme.successColor : Astrea.Theme.textSecondary
            font.pixelSize: Astrea.Theme.fontSizeSmall
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Control.SegmentedButton {
            Layout.fillWidth: true
            options: ["on", "off"]
            value: panel.state
            onSelected: value => {
                panel.state = value
                panel.saveLightbarConfig()
                if (panel.backend && panel.backend.ready)
                    panel.backend.applyLiveAction("lightbar-state", ["--state", value])
            }
        }

        Text {
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            text: panel.backend && panel.backend.ready ? qsTr("Live color") : qsTr("Saved until controller connects")
            color: Astrea.Theme.textSecondary
            font.family: Astrea.Theme.fontFamily
            font.pixelSize: Astrea.Theme.fontSizeSmall
        }
    }
}
