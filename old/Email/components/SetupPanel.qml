import QtQuick
import QtQuick.Layouts
import "../AstreaComponents" as Astrea

Rectangle {
    id: panel

    property bool configured: false
    property bool authenticated: false
    property bool busy: false
    property string credentialsPath: ""
    property string tokenPath: ""
    property string account: ""
    property string statusMessage: ""
    property color softSurface: Astrea.Theme.cardBg
    property bool mailServiceEnabled: true
    property bool copyCodesEnabled: true
    property bool islandCodesEnabled: true
    property bool desktopNotificationsEnabled: true
    signal connectRequested()
    signal refreshRequested()
    signal detailsRequested()
    signal settingToggled(string key, bool value)

    radius: Astrea.Theme.cardRadius
    color: Astrea.Theme.cardBg
    border.width: 1
    border.color: Astrea.Theme.cardBorder
    clip: true

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Flickable {
        anchors {
            fill: parent
            margins: Astrea.Theme.spacingXLarge
        }
        contentWidth: width
        contentHeight: settingsContent.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout {
            id: settingsContent

            width: Math.min(parent.width, 760)
            x: Math.max(0, (parent.width - width) / 2)
            spacing: Astrea.Theme.spacingLarge

            RowLayout {
                Layout.fillWidth: true
                spacing: Astrea.Theme.spacing

                Rectangle {
                    Layout.preferredWidth: 46
                    Layout.preferredHeight: 46
                    radius: 16
                    color: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.18)
                    border.width: 1
                    border.color: Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.28)

                    Text {
                        anchors.centerIn: parent
                        text: "\uf013"
                        color: Astrea.Theme.accent
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 21
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Astrea.DisplayLabel {
                        Layout.fillWidth: true
                        text: "Mail settings"
                        textColor: Astrea.Theme.textPrimary
                        font.pixelSize: Astrea.Theme.fontSizeHeader
                        font.weight: Astrea.Theme.fontWeightDemiBold
                    }

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: "Control the mail service, notifications, security-code capture and provider account."
                        textColor: Astrea.Theme.textSecondary
                        font.pixelSize: Astrea.Theme.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Astrea.Theme.controlRadius + 4
                color: panel.softSurface
                border.width: 1
                border.color: Astrea.Theme.cardBorder
                implicitHeight: notificationSettings.implicitHeight + 24

                ColumnLayout {
                    id: notificationSettings
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 14
                        rightMargin: 14
                    }
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: "Service and automation"
                            textColor: Astrea.Theme.textPrimary
                            font.pixelSize: Astrea.Theme.fontSizeNormal
                            font.weight: Astrea.Theme.fontWeightDemiBold
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: "Turn Mail background polling and code automation on or off."
                            textColor: Astrea.Theme.textSecondary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                        }
                    }

                    SettingToggleRow {
                        label: "Mail service"
                        description: "Check for new inbox messages while Mail is open."
                        keyName: "mailServiceEnabled"
                        checked: panel.mailServiceEnabled
                        toggleEnabled: !panel.busy
                        onToggled: (key, value) => panel.settingToggled(key, value)
                    }

                    SettingToggleRow {
                        label: "Desktop notifications"
                        description: "Show a system notification when a new email arrives."
                        keyName: "desktopNotificationsEnabled"
                        checked: panel.desktopNotificationsEnabled
                        toggleEnabled: !panel.busy
                        onToggled: (key, value) => panel.settingToggled(key, value)
                    }

                    SettingToggleRow {
                        label: "Copy security codes"
                        description: "Copy detected login and 2FA codes to the clipboard."
                        keyName: "copyCodesEnabled"
                        checked: panel.copyCodesEnabled
                        toggleEnabled: !panel.busy
                        onToggled: (key, value) => panel.settingToggled(key, value)
                    }

                    SettingToggleRow {
                        label: "Dynamic Island"
                        description: "Show detected security codes in the Astrea Island."
                        keyName: "islandCodesEnabled"
                        checked: panel.islandCodesEnabled
                        toggleEnabled: !panel.busy
                        onToggled: (key, value) => panel.settingToggled(key, value)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Astrea.Theme.controlRadius + 4
                color: panel.softSurface
                border.width: 1
                border.color: Astrea.Theme.cardBorder
                implicitHeight: accountSettings.implicitHeight + 24

                ColumnLayout {
                    id: accountSettings
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 14
                        rightMargin: 14
                    }
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: "Gmail account"
                            textColor: Astrea.Theme.textPrimary
                            font.pixelSize: Astrea.Theme.fontSizeNormal
                            font.weight: Astrea.Theme.fontWeightDemiBold
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: panel.authenticated
                                ? (panel.account || "Connected and ready to sync.")
                                : (panel.configured ? "Authenticate Gmail to load your mailbox." : "Add the OAuth client JSON before syncing mail.")
                            textColor: Astrea.Theme.textSecondary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                        }
                    }

                    SetupRow {
                        label: "OAuth client"
                        value: panel.configured ? "Found" : panel.credentialsPath
                        active: panel.configured
                    }

                    SetupRow {
                        label: "Authentication"
                        value: panel.authenticated ? "Connected" : "Not connected"
                        active: panel.authenticated
                    }

                    SetupRow {
                        label: "Token store"
                        value: panel.tokenPath
                        active: panel.authenticated
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Astrea.Theme.spacing

                        Astrea.Button {
                            text: panel.configured ? "Connect Gmail" : "Show path"
                            iconText: panel.configured ? "\uf0e0" : "\uf05a"
                            iconFontFamily: "JetBrainsMono Nerd Font"
                            primary: panel.configured
                            enabled: !panel.busy
                            onClicked: panel.configured ? panel.connectRequested() : panel.detailsRequested()
                        }

                        Astrea.Button {
                            text: "Refresh"
                            iconText: "\uf01e"
                            iconFontFamily: "JetBrainsMono Nerd Font"
                            flat: true
                            enabled: !panel.busy
                            onClicked: panel.refreshRequested()
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    component SetupRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool active: false

        spacing: 10

        Rectangle {
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            radius: 11
            color: active ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.20) : Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
            border.color: active ? Qt.rgba(Astrea.Theme.accent.r, Astrea.Theme.accent.g, Astrea.Theme.accent.b, 0.34) : Astrea.Theme.cardBorder

            Text {
                anchors.centerIn: parent
                text: active ? "\uf00c" : "\uf111"
                color: active ? Astrea.Theme.accent : Astrea.Theme.textTertiary
                font.family: "JetBrainsMono Nerd Font"
                font.pixelSize: active ? 10 : 7
            }
        }

        Astrea.TextLabel {
            Layout.preferredWidth: 118
            text: parent.label
            textColor: Astrea.Theme.textPrimary
            font.pixelSize: Astrea.Theme.fontSizeSmall
            font.weight: Astrea.Theme.fontWeightDemiBold
            elide: Text.ElideRight
        }

        Astrea.TextLabel {
            Layout.fillWidth: true
            text: parent.value
            textColor: Astrea.Theme.textSecondary
            font.pixelSize: Astrea.Theme.fontSizeSmall
            elide: Text.ElideMiddle
        }
    }

    component SettingToggleRow: RowLayout {
        id: settingRow

        property string keyName: ""
        property string label: ""
        property string description: ""
        property bool checked: false
        property bool toggleEnabled: true
        signal toggled(string key, bool value)

        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Astrea.TextLabel {
                Layout.fillWidth: true
                text: settingRow.label
                textColor: Astrea.Theme.textPrimary
                font.pixelSize: Astrea.Theme.fontSizeSmall
                font.weight: Astrea.Theme.fontWeightDemiBold
                elide: Text.ElideRight
            }

            Astrea.TextLabel {
                Layout.fillWidth: true
                text: settingRow.description
                textColor: Astrea.Theme.textSecondary
                font.pixelSize: Astrea.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }

        Astrea.ToggleSwitch {
            checked: settingRow.checked
            enabled: settingRow.toggleEnabled
            onToggled: targetChecked => settingRow.toggled(settingRow.keyName, targetChecked)
        }
    }
}
