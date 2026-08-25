import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea
import "../controls" as Control

Control.Card {
    id: panel
    property var backend

    function indexOfDevice() {
        if (!backend)
            return -1
        for (var i = 0; i < backend.devices.length; i++) {
            if (backend.devices[i] === backend.device)
                return i
        }
        return -1
    }

    Astrea.SectionHeader {
        Layout.fillWidth: true
        text: qsTr("Device")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Astrea.StatusDot {
            active: backend && backend.ready
        }
        Text {
            Layout.fillWidth: true
            text: !backend ? qsTr("Unavailable") : (backend.ready ? qsTr("Connected") : (backend.installed ? backend.message : qsTr("dualsensectl missing")))
            color: Astrea.Theme.textPrimary
            font.family: Astrea.Theme.fontFamily
            font.pixelSize: Astrea.Theme.fontSizeNormal
            font.weight: Font.Medium
        }
    }

    Text {
        Layout.fillWidth: true
        visible: backend && backend.battery !== ""
        text: backend ? backend.battery : ""
        color: Astrea.Theme.textSecondary
        font.family: Astrea.Theme.fontFamily
        font.pixelSize: Astrea.Theme.fontSizeSmall
        wrapMode: Text.WordWrap
    }

    Astrea.SelectButton {
        Layout.fillWidth: true
        label: backend && backend.device !== "" ? backend.device : qsTr("No devices")
        options: backend && backend.devices.length ? backend.devices : [qsTr("No devices")]
        selectedIndex: panel.indexOfDevice()
        onSelected: index => {
            if (backend && backend.devices.length && index >= 0) {
                backend.device = backend.devices[index]
                backend.refresh()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Control.PrimaryButton {
            text: qsTr("Refresh")
            enabledState: backend && !backend.loading
            onClicked: backend.refresh()
        }
        Control.PrimaryButton {
            text: qsTr("Power off")
            enabledState: backend && backend.ready && !backend.applying
            onClicked: backend.applyAction("power-off", [])
        }
    }

    Astrea.SettingRow {
        Layout.fillWidth: true
        label: qsTr("Bridge")
        sublabel: backend && backend.binary !== "" ? backend.binary : qsTr("dualsensectl will be resolved from PATH")
    }

    Astrea.SettingRow {
        Layout.fillWidth: true
        isLast: true
        label: qsTr("Saved settings")
        sublabel: backend && backend.configPath !== "" ? backend.configPath : qsTr("Waiting for local configuration path")
    }
}
