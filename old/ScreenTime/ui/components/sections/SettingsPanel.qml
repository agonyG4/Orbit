import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea
import "../common" as Common

ColumnLayout {
    id: root

    property var screenData: null
    property var serviceData: null
    property bool serviceBusy: false
    property string serviceErrorMsg: ""
    property bool settingsBusy: false
    property string settingsErrorMsg: ""
    property bool privacyUnlocked: false
    property bool privacyAuthBusy: false
    property string privacyAuthErrorMsg: ""
    property var doctorData: null
    property bool doctorBusy: false
    property string doctorErrorMsg: ""
    property bool maintenanceBusy: false
    property string maintenanceErrorMsg: ""
    property string pendingRemoveAppId: ""
    readonly property var appRows: sortedByUsage(screenData && screenData.settings ? (screenData.settings.apps || []) : [])

    signal collectorEnabledChanged(bool enabled)
    signal privacyUnlockRequested()
    signal privacyLockRequested()
    signal appHiddenChanged(string appId, bool hidden)
    signal appRemoved(string appId)
    signal doctorRefreshRequested()
    signal eventsCompactRequested()

    Layout.fillWidth: true
    spacing: Astrea.Theme.spacingMedium

    function serviceToggleChecked() {
        if (!serviceData)
            return false
        if (serviceData.collector_active !== undefined)
            return !!serviceData.collector_active
        return !!serviceData.enabled || !!serviceData.active
    }

    function serviceStatusText() {
        if (serviceBusy)
            return qsTr("Aplicando alteracao")
        if (serviceErrorMsg !== "")
            return serviceErrorMsg
        if (!serviceData)
            return qsTr("Carregando servico")
        var state = serviceData.display ? String(serviceData.display.state || "") : ""
        var unit = serviceData.display && serviceData.display.unit
            ? String(serviceData.display.unit)
            : String(serviceData.unit || "astrea-screentimed.service")
        return state !== "" ? state + " - " + unit : unit
    }

    function appLabel(row) {
        if (!row)
            return qsTr("App")
        return row.label || row.name || row.id || qsTr("App")
    }

    function appCategory(row) {
        if (!row)
            return ""
        return row.category || ""
    }

    function sortedByUsage(rows) {
        return (rows || []).slice().sort((left, right) => Number(right.seconds || 0) - Number(left.seconds || 0))
    }

    function privacyStatusText() {
        if (privacyAuthBusy)
            return qsTr("Aguardando senha de login")
        if (privacyAuthErrorMsg !== "")
            return privacyAuthErrorMsg
        if (privacyUnlocked)
            return qsTr("Apps ocultos e acoes privadas liberados")
        return qsTr("Desbloqueie para ver apps ocultos, ocultar ou remover apps")
    }

    function doctorStatusText() {
        if (doctorBusy)
            return qsTr("Atualizando diagnostico")
        if (doctorErrorMsg !== "")
            return doctorErrorMsg
        if (!doctorData)
            return qsTr("Diagnostico nao carregado")
        return doctorData.ok ? qsTr("Sem erros criticos") : qsTr("Requer atencao")
    }

    function eventSummaryText() {
        if (!doctorData || !doctorData.events)
            return qsTr("Historico de eventos indisponivel")
        var lines = Number(doctorData.events.lines || 0)
        var bytes = Number(doctorData.events.bytes || 0)
        return qsTr("%1 eventos - %2").arg(lines).arg(formatBytes(bytes))
    }

    function formatBytes(bytes) {
        var value = Number(bytes || 0)
        if (value >= 1048576)
            return qsTr("%1 MB").arg(Math.round(value / 104857.6) / 10)
        if (value >= 1024)
            return qsTr("%1 KB").arg(Math.round(value / 102.4) / 10)
        return qsTr("%1 B").arg(value)
    }

    function checkColor(row) {
        var severity = String(row && row.severity ? row.severity : "")
        if (severity === "error")
            return Astrea.Theme.errorColor
        if (severity === "warning")
            return Astrea.Theme.warningColor
        return Astrea.Theme.successColor
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 78
        radius: Astrea.Theme.cardRadius
        color: Astrea.Theme.cardBg
        border.width: 1
        border.color: Astrea.Theme.cardBorder

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Astrea.Theme.spacingLarge
            anchors.rightMargin: Astrea.Theme.spacingLarge
            spacing: Astrea.Theme.spacingMedium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    text: qsTr("Coletor em segundo plano")
                    textColor: Astrea.Theme.textPrimary
                    font.pixelSize: Astrea.Theme.fontSizeLarge
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    elide: Text.ElideRight
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    text: root.serviceStatusText()
                    textColor: root.serviceErrorMsg !== "" ? Astrea.Theme.errorColor : Astrea.Theme.textTertiary
                    font.pixelSize: Astrea.Theme.fontSizeSmall
                    elide: Text.ElideRight
                }
            }

            Astrea.ToggleSwitch {
                enabled: !root.serviceBusy
                checked: root.serviceToggleChecked()
                onToggled: targetChecked => root.collectorEnabledChanged(targetChecked)
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Astrea.Theme.spacingSmall

        Astrea.TextLabel {
            Layout.fillWidth: true
            text: qsTr("Diagnostico")
            textColor: Astrea.Theme.textSecondary
            font.pixelSize: Astrea.Theme.fontSizeTitle
            font.weight: Astrea.Theme.fontWeightDemiBold
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(118, diagnosticContent.implicitHeight + Astrea.Theme.spacingMedium * 2)
            radius: Astrea.Theme.cardRadius
            color: Astrea.Theme.cardBg
            border.width: 1
            border.color: Astrea.Theme.cardBorder

            ColumnLayout {
                id: diagnosticContent
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Astrea.Theme.spacingMedium
                }
                spacing: Astrea.Theme.spacingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Astrea.Theme.spacingSmall

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: root.doctorStatusText()
                            textColor: root.doctorData && root.doctorData.ok ? Astrea.Theme.successColor : Astrea.Theme.warningColor
                            font.pixelSize: Astrea.Theme.fontSizeLarge
                            font.weight: Astrea.Theme.fontWeightDemiBold
                            elide: Text.ElideRight
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: root.eventSummaryText()
                            textColor: Astrea.Theme.textTertiary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }

                    Astrea.Button {
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 32
                        text: root.doctorBusy ? qsTr("Lendo") : qsTr("Atualizar")
                        flat: true
                        enabled: !root.doctorBusy
                        fontPixelSize: Astrea.Theme.fontSizeSmall
                        horizontalPadding: Astrea.Theme.spacingSmall
                        onClicked: root.doctorRefreshRequested()
                    }

                    Astrea.Button {
                        Layout.preferredWidth: 86
                        Layout.preferredHeight: 32
                        text: root.maintenanceBusy ? qsTr("Limpando") : qsTr("Compactar")
                        flat: true
                        enabled: !root.maintenanceBusy
                        fontPixelSize: Astrea.Theme.fontSizeSmall
                        horizontalPadding: Astrea.Theme.spacingSmall
                        onClicked: root.eventsCompactRequested()
                    }
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    visible: root.maintenanceErrorMsg !== ""
                    text: root.maintenanceErrorMsg
                    textColor: Astrea.Theme.errorColor
                    font.pixelSize: Astrea.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: root.doctorData && root.doctorData.checks ? root.doctorData.checks.slice(0, 5) : []

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Astrea.Theme.spacingSmall

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 4
                            color: root.checkColor(modelData)
                        }

                        Astrea.TextLabel {
                            Layout.preferredWidth: 92
                            text: modelData.label || modelData.id || ""
                            textColor: Astrea.Theme.textSecondary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: modelData.message || ""
                            textColor: Astrea.Theme.textTertiary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Astrea.Theme.spacingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Astrea.Theme.spacingSmall

            Astrea.TextLabel {
                Layout.fillWidth: true
                text: qsTr("Apps no ScreenTime")
                textColor: Astrea.Theme.textSecondary
                font.pixelSize: Astrea.Theme.fontSizeTitle
                font.weight: Astrea.Theme.fontWeightDemiBold
                elide: Text.ElideRight
            }

            Astrea.Button {
                Layout.preferredWidth: root.privacyUnlocked ? 82 : 118
                Layout.preferredHeight: 32
                text: root.privacyUnlocked ? qsTr("Bloquear") : qsTr("Desbloquear")
                flat: true
                enabled: !root.privacyAuthBusy
                fontPixelSize: Astrea.Theme.fontSizeSmall
                horizontalPadding: Astrea.Theme.spacingSmall
                onClicked: root.privacyUnlocked ? root.privacyLockRequested() : root.privacyUnlockRequested()
            }
        }

        Astrea.TextLabel {
            Layout.fillWidth: true
            text: root.privacyStatusText()
            textColor: root.privacyAuthErrorMsg !== "" ? Astrea.Theme.errorColor : Astrea.Theme.textTertiary
            font.pixelSize: Astrea.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Astrea.TextLabel {
            Layout.fillWidth: true
            visible: root.settingsErrorMsg !== ""
            text: root.settingsErrorMsg
            textColor: Astrea.Theme.errorColor
            font.pixelSize: Astrea.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            visible: !root.privacyUnlocked
            implicitHeight: 126
            radius: Astrea.Theme.cardRadius
            color: Astrea.Theme.cardBg
            border.width: 1
            border.color: Astrea.Theme.cardBorder

            RowLayout {
                anchors.fill: parent
                anchors.margins: Astrea.Theme.spacingLarge
                spacing: Astrea.Theme.spacingMedium

                Rectangle {
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    radius: 12
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)

                    Astrea.TextLabel {
                        anchors.centerIn: parent
                        text: "•"
                        textColor: Astrea.Theme.warningColor
                        font.pixelSize: 28
                        font.weight: Astrea.Theme.fontWeightDemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: qsTr("Privacidade bloqueada")
                        textColor: Astrea.Theme.textPrimary
                        font.pixelSize: Astrea.Theme.fontSizeLarge
                        font.weight: Astrea.Theme.fontWeightDemiBold
                        elide: Text.ElideRight
                    }

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: qsTr("Digite sua senha de login para ver apps ocultos, ocultar apps ou remover historico de um app.")
                        textColor: Astrea.Theme.textTertiary
                        font.pixelSize: Astrea.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Astrea.Button {
                    Layout.preferredWidth: 118
                    Layout.preferredHeight: 34
                    text: root.privacyAuthBusy ? qsTr("Abrindo") : qsTr("Desbloquear")
                    primary: true
                    enabled: !root.privacyAuthBusy
                    fontPixelSize: Astrea.Theme.fontSizeSmall
                    horizontalPadding: Astrea.Theme.spacingSmall
                    onClicked: root.privacyUnlockRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.privacyUnlocked
            implicitHeight: Math.max(78, appList.implicitHeight + Astrea.Theme.spacingMedium * 2)
            radius: Astrea.Theme.cardRadius
            color: Astrea.Theme.cardBg
            border.width: 1
            border.color: Astrea.Theme.cardBorder

            ColumnLayout {
                id: appList
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Astrea.Theme.spacingMedium
                }
                spacing: 0

                Repeater {
                    model: root.appRows || []

                    delegate: Rectangle {
                        id: appRow

                        required property var modelData
                        required property int index
                        readonly property string appId: String(modelData.id || "")
                        readonly property bool hidden: !!modelData.hidden
                        readonly property bool confirmingRemoval: root.pendingRemoveAppId === appId

                        Layout.fillWidth: true
                        Layout.preferredHeight: confirmingRemoval ? 76 : 68
                        radius: Astrea.Theme.cornerRadiusSmall
                        color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.055) : "transparent"

                        HoverHandler { id: rowHover }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: Astrea.Theme.spacingSmall

                            Common.AltTabAppIcon {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                row: appRow.modelData
                                iconRadius: 10
                                fallbackRadius: iconRadius
                                fallbackFontSize: 16
                                sourcePixelSize: 192
                                fallbackColor: Astrea.Theme.cardBg
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Astrea.TextLabel {
                                    Layout.fillWidth: true
                                    text: root.appLabel(appRow.modelData)
                                    textColor: Astrea.Theme.textPrimary
                                    font.pixelSize: Astrea.Theme.fontSizeLarge
                                    elide: Text.ElideRight
                                }

                                Astrea.TextLabel {
                                    Layout.fillWidth: true
                                    text: appRow.hidden ? qsTr("Oculto dos apps") : qsTr("Visivel nos apps")
                                    textColor: appRow.hidden ? Astrea.Theme.warningColor : Astrea.Theme.textTertiary
                                    font.pixelSize: Astrea.Theme.fontSizeSmall
                                    elide: Text.ElideRight
                                }
                            }

                            Astrea.ToggleSwitch {
                                visible: !appRow.confirmingRemoval
                                enabled: !root.settingsBusy && appRow.appId !== "" && appRow.appId !== "unknown"
                                checked: !appRow.hidden
                                onToggled: targetChecked => root.appHiddenChanged(appRow.appId, !targetChecked)
                            }

                            Astrea.Button {
                                visible: !appRow.confirmingRemoval
                                Layout.preferredWidth: 68
                                Layout.preferredHeight: 32
                                text: qsTr("Remover")
                                danger: true
                                flat: true
                                enabled: !root.settingsBusy && appRow.appId !== "" && appRow.appId !== "unknown"
                                fontPixelSize: Astrea.Theme.fontSizeSmall
                                horizontalPadding: Astrea.Theme.spacingSmall
                                onClicked: root.pendingRemoveAppId = appRow.appId
                            }

                            Astrea.Button {
                                visible: appRow.confirmingRemoval
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 32
                                text: qsTr("Cancelar")
                                flat: true
                                enabled: !root.settingsBusy
                                fontPixelSize: Astrea.Theme.fontSizeSmall
                                horizontalPadding: Astrea.Theme.spacingSmall
                                onClicked: root.pendingRemoveAppId = ""
                            }

                            Astrea.Button {
                                visible: appRow.confirmingRemoval
                                Layout.preferredWidth: 74
                                Layout.preferredHeight: 32
                                text: qsTr("Confirmar")
                                danger: true
                                enabled: !root.settingsBusy
                                fontPixelSize: Astrea.Theme.fontSizeSmall
                                horizontalPadding: Astrea.Theme.spacingSmall
                                onClicked: {
                                    root.pendingRemoveAppId = ""
                                    root.appRemoved(appRow.appId)
                                }
                            }
                        }

                        Rectangle {
                            visible: index < (root.appRows || []).length - 1
                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                                leftMargin: 56
                            }
                            height: 1
                            color: Astrea.Theme.cardBorder
                            opacity: 0.65
                        }
                    }
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    visible: !root.appRows || root.appRows.length === 0
                    text: qsTr("Sem apps registrados ainda")
                    textColor: Astrea.Theme.textTertiary
                    font.pixelSize: Astrea.Theme.fontSizeNormal
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
