//@ pragma IconTheme WhiteSur-dark
import Quickshell
import QtQuick
import QtQuick.Layouts
import "AstreaComponents" as Astrea
import "components/common" as Common
import "components/sections" as Sections
import "state" as State

FloatingWindow {
    id: root

    title: "ScreenTime"
    implicitWidth: 430
    implicitHeight: 760
    maximized: false
    fullscreen: false
    visible: true
    color: Astrea.Theme.windowBackground

    property string mode: "day"
    property bool showCategories: false
    property bool showingSettings: false
    readonly property var activeData: screenTime.screenData
        ? (mode === "week" ? screenTime.screenData.week : screenTime.screenData.day)
        : null
    readonly property var currentRows: usageRows()
    readonly property real currentTotalSeconds: activeData ? Number(activeData.seconds || 0) : 0
    readonly property var weekData: screenTime.screenData && screenTime.screenData.week ? screenTime.screenData.week : null
    readonly property var serviceData: screenTime.serviceStatus

    function usageRows() {
        if (!screenTime.screenData)
            return []
        var rows = []
        if (mode === "week")
            rows = showCategories ? (screenTime.screenData.week.categories || []) : (screenTime.screenData.week.apps || [])
        else
            rows = showCategories ? (screenTime.screenData.day.top_categories || []) : (screenTime.screenData.day.top_apps || [])
        return rows.slice().sort((left, right) => Number(right.seconds || 0) - Number(left.seconds || 0))
    }

    function statusText() {
        if (!screenTime.screenData || !screenTime.screenData.display)
            return ""
        var status = screenTime.screenData.display.generated_at || ""
        var health = screenTime.screenData.display.health || ""
        return health ? status + " - " + health : status
    }

    function hasHealthWarning() {
        if (!screenTime.screenData || !screenTime.screenData.health)
            return false
        return !screenTime.screenData.health.running || String(screenTime.screenData.health.last_error || "").length > 0
    }

    function weekPositionText() {
        if (!weekData || !weekData.history_count || weekData.history_index < 0)
            return ""
        return qsTr("%1 de %2").arg(Number(weekData.history_index) + 1).arg(weekData.history_count)
    }

    function selectPreviousWeek() {
        if (weekData && weekData.previous_start)
            screenTime.selectWeek(weekData.previous_start)
    }

    function selectNextWeek() {
        if (weekData && weekData.next_start)
            screenTime.selectWeek(weekData.next_start)
    }

    function serviceToggleChecked() {
        if (!serviceData)
            return false
        if (serviceData.collector_active !== undefined)
            return !!serviceData.collector_active
        return !!serviceData.enabled || !!serviceData.active
    }

    function serviceStatusText() {
        if (screenTime.serviceBusy)
            return qsTr("Aplicando alteracao")
        if (screenTime.serviceErrorMsg !== "")
            return screenTime.serviceErrorMsg
        if (!serviceData)
            return qsTr("Carregando servico")
        var state = serviceData.display ? String(serviceData.display.state || "") : ""
        var unit = serviceData.display && serviceData.display.unit
            ? String(serviceData.display.unit)
            : String(serviceData.unit || "astrea-screentimed.service")
        return state !== "" ? state + " - " + unit : unit
    }

    onVisibleChanged: {
        if (!visible)
            Qt.quit()
    }

    onMaximizedChanged: {
        if (maximized)
            maximized = false
    }

    onFullscreenChanged: {
        if (fullscreen)
            fullscreen = false
    }

    State.ScreenTimeState {
        id: screenTime
        appVisible: root.visible
        settingsVisible: root.showingSettings
    }

    Rectangle {
        anchors.fill: parent
        color: Astrea.Theme.windowBackground

        Item {
            anchors.fill: parent
            visible: screenTime.loading

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 320)
                spacing: Astrea.Theme.spacingSmall

                Astrea.DisplayLabel {
                    Layout.fillWidth: true
                    text: qsTr("ScreenTime")
                    textColor: Astrea.Theme.textPrimary
                    font.pixelSize: Astrea.Theme.fontSizeHeader
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    text: qsTr("Carregando")
                    textColor: Astrea.Theme.textTertiary
                    font.pixelSize: Astrea.Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Item {
            anchors.fill: parent
            visible: !screenTime.loading && screenTime.errorMsg !== "" && screenTime.screenData === null

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 340)
                spacing: Astrea.Theme.spacingMedium

                Astrea.DisplayLabel {
                    Layout.fillWidth: true
                    text: qsTr("ScreenTime")
                    textColor: Astrea.Theme.textPrimary
                    font.pixelSize: Astrea.Theme.fontSizeHeader
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    text: screenTime.errorMsg
                    textColor: Astrea.Theme.errorColor
                    font.pixelSize: Astrea.Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        Flickable {
            anchors.fill: parent
            visible: screenTime.screenData !== null
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: mainLayout.implicitHeight + 44

            ColumnLayout {
                id: mainLayout

                width: Math.min(parent.width - 40, 390)
                x: Math.max(20, (parent.width - width) / 2)
                y: 22
                spacing: Astrea.Theme.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Astrea.Theme.spacing

                    Rectangle {
                        id: settingsButton

                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: 22
                        color: settingsHover.hovered ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        scale: settingsPress.pressed ? 0.97 : 1

                        Behavior on color { ColorAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }

                        HoverHandler { id: settingsHover }

                        Astrea.TextLabel {
                            anchors.centerIn: parent
                            text: root.showingSettings ? "<" : "⚙"
                            textColor: Astrea.Theme.textPrimary
                            font.pixelSize: root.showingSettings ? 28 : 22
                            font.weight: Astrea.Theme.fontWeightLight
                        }

                        MouseArea {
                            id: settingsPress
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.showingSettings = !root.showingSettings
                        }
                    }

                    Astrea.DisplayLabel {
                        Layout.fillWidth: true
                        text: root.showingSettings ? qsTr("Settings") : qsTr("ScreenTime")
                        textColor: Astrea.Theme.textPrimary
                        font.pixelSize: Astrea.Theme.fontSizeTitle
                        font.weight: Astrea.Theme.fontWeightDemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                    }
                }

                Sections.SettingsPanel {
                    visible: root.showingSettings
                    screenData: screenTime.screenData
                    serviceData: root.serviceData
                    serviceBusy: screenTime.serviceBusy
                    serviceErrorMsg: screenTime.serviceErrorMsg
                    settingsBusy: screenTime.settingsBusy
                    settingsErrorMsg: screenTime.settingsErrorMsg
                    privacyUnlocked: screenTime.privacyUnlocked
                    privacyAuthBusy: screenTime.privacyAuthBusy
                    privacyAuthErrorMsg: screenTime.privacyAuthErrorMsg
                    doctorData: screenTime.doctorData
                    doctorBusy: screenTime.doctorBusy
                    doctorErrorMsg: screenTime.doctorErrorMsg
                    maintenanceBusy: screenTime.maintenanceBusy
                    maintenanceErrorMsg: screenTime.maintenanceErrorMsg
                    onCollectorEnabledChanged: enabled => screenTime.setCollectorEnabled(enabled)
                    onPrivacyUnlockRequested: screenTime.unlockPrivacy()
                    onPrivacyLockRequested: screenTime.lockPrivacy()
                    onAppHiddenChanged: (appId, hidden) => screenTime.setAppHidden(appId, hidden)
                    onAppRemoved: appId => screenTime.removeApp(appId)
                    onDoctorRefreshRequested: screenTime.refreshDoctor()
                    onEventsCompactRequested: screenTime.compactEvents()
                }

                Common.SegmentedControl {
                    Layout.fillWidth: true
                    visible: !root.showingSettings
                    model: [
                        { "label": qsTr("Semana"), "value": "week" },
                        { "label": qsTr("Dia"), "value": "day" }
                    ]
                    currentValue: root.mode
                    onValueChanged: value => root.mode = value
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.mode === "week" ? 46 : 0
                    visible: !root.showingSettings && root.mode === "week"
                    radius: Astrea.Theme.controlRadius
                    color: Qt.rgba(1, 1, 1, 0.045)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.065)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 5
                        anchors.rightMargin: 5
                        spacing: Astrea.Theme.spacingSmall

                        Astrea.Button {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            text: "<"
                            flat: true
                            enabled: root.weekData !== null && root.weekData.previous_start !== ""
                            controlHeight: 36
                            horizontalPadding: 0
                            fontPixelSize: Astrea.Theme.fontSizeLarge
                            onClicked: root.selectPreviousWeek()
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Astrea.TextLabel {
                                Layout.fillWidth: true
                                text: root.weekData ? root.weekData.label : ""
                                textColor: Astrea.Theme.textPrimary
                                font.pixelSize: Astrea.Theme.fontSizeNormal
                                font.weight: Astrea.Theme.fontWeightDemiBold
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            Astrea.TextLabel {
                                Layout.fillWidth: true
                                text: root.weekPositionText()
                                textColor: Astrea.Theme.textTertiary
                                font.pixelSize: Astrea.Theme.fontSizeTiny
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }

                        Astrea.Button {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            text: ">"
                            flat: true
                            enabled: root.weekData !== null && root.weekData.next_start !== ""
                            controlHeight: 36
                            horizontalPadding: 0
                            fontPixelSize: Astrea.Theme.fontSizeLarge
                            onClicked: root.selectNextWeek()
                        }
                    }
                }

                Astrea.TextLabel {
                    Layout.fillWidth: true
                    visible: !root.showingSettings
                    Layout.topMargin: Astrea.Theme.spacingSmall
                    text: qsTr("Screen Time")
                    textColor: Astrea.Theme.textSecondary
                    font.pixelSize: Astrea.Theme.fontSizeTitle
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    elide: Text.ElideRight
                }

                Sections.SummaryCard {
                    visible: !root.showingSettings
                    summaryData: screenTime.screenData
                    mode: root.mode
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.showingSettings
                    Layout.leftMargin: Astrea.Theme.spacing
                    Layout.rightMargin: Astrea.Theme.spacing
                    Layout.preferredHeight: 32
                    radius: Astrea.Theme.controlRadius
                    color: Qt.rgba(1, 1, 1, 0.035)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.06)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Astrea.Theme.spacingMedium
                        anchors.rightMargin: Astrea.Theme.spacingMedium
                        spacing: Astrea.Theme.spacingSmall

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 4
                            color: root.hasHealthWarning() ? Astrea.Theme.warningColor : Astrea.Theme.successColor
                        }

                        Astrea.TextLabel {
                            Layout.fillWidth: true
                            text: root.statusText()
                            textColor: root.hasHealthWarning() ? Astrea.Theme.warningColor : Astrea.Theme.textTertiary
                            font.pixelSize: Astrea.Theme.fontSizeSmall
                            elide: Text.ElideRight
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: !root.showingSettings
                    Layout.topMargin: Astrea.Theme.spacingLarge
                    spacing: Astrea.Theme.spacing

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: qsTr("Mais Usados")
                        textColor: Astrea.Theme.textSecondary
                        font.pixelSize: Astrea.Theme.fontSizeTitle
                        font.weight: Astrea.Theme.fontWeightDemiBold
                        elide: Text.ElideRight
                    }

                    Astrea.Button {
                        text: root.showCategories ? qsTr("Mostrar Apps") : qsTr("Mostrar Categorias")
                        flat: true
                        fontPixelSize: Astrea.Theme.fontSizeLarge
                        foregroundColor: Astrea.Theme.accent
                        controlHeight: 34
                        horizontalPadding: 8
                        onClicked: root.showCategories = !root.showCategories
                    }
                }

                Sections.UsageList {
                    visible: !root.showingSettings
                    rows: root.currentRows
                    totalSeconds: root.currentTotalSeconds
                    categoryMode: root.showCategories
                }
            }
        }
    }
}
