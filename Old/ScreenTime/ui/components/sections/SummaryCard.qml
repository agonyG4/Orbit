import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea

Rectangle {
    id: root

    property var summaryData: null
    property string mode: "day"
    readonly property bool weekMode: mode === "week"
    readonly property var dayData: summaryData && summaryData.day ? summaryData.day : null
    readonly property var weekData: summaryData && summaryData.week ? summaryData.week : null
    readonly property var displayData: summaryData && summaryData.display ? summaryData.display : null
    readonly property string durationText: weekMode
        ? (weekData ? weekData.duration : "0s")
        : (dayData ? dayData.duration : "0s")
    readonly property string dateText: weekMode
        ? (weekData ? weekData.label : "")
        : (displayData ? displayData.selected_day : "")
    readonly property var legendRows: weekMode
        ? (weekData ? weekData.categories : [])
        : (dayData ? dayData.top_categories : [])

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Astrea.Theme.spacingLarge * 2
    radius: Astrea.Theme.cardRadius
    color: Astrea.Theme.cardBg
    border.width: 1
    border.color: Astrea.Theme.cardBorder

    function categoryColor(categoryId, index) {
        var id = String(categoryId || "")
        if (id === "development")
            return Astrea.Theme.accent
        if (id === "browser")
            return "#64dce2"
        if (id === "games")
            return "#bf8cff"
        if (id === "media")
            return Astrea.Theme.successColor
        if (id === "system")
            return Astrea.Theme.warningColor
        if (id === "communication")
            return "#ff6fb1"
        if (id === "utilities")
            return "#8bd3ff"
        if (id === "other")
            return "#ffb35c"
        return index % 2 === 0 ? Astrea.Theme.accent : "#64dce2"
    }

    ColumnLayout {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Astrea.Theme.spacingLarge
        }
        spacing: Astrea.Theme.spacingMedium

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Astrea.TextLabel {
                Layout.fillWidth: true
                text: root.dateText
                textColor: Astrea.Theme.textSecondary
                font.pixelSize: Astrea.Theme.fontSizeSubtitle
                elide: Text.ElideRight
            }

            Astrea.DisplayLabel {
                Layout.fillWidth: true
                text: root.durationText
                textColor: Astrea.Theme.textPrimary
                font.pixelSize: 42
                font.weight: Astrea.Theme.fontWeightLight
                elide: Text.ElideRight
            }
        }

        ActivityBarChart {
            rows: root.weekData ? root.weekData.days : []
            hourly: false
        }

        ActivityBarChart {
            visible: !root.weekMode
            Layout.preferredHeight: visible ? 142 : 0
            rows: root.dayData ? root.dayData.hourly : []
            hourly: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Astrea.Theme.spacingMedium

            Repeater {
                model: (root.legendRows || []).slice(0, 3)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: modelData.label || modelData.id
                        textColor: root.categoryColor(modelData.id, index)
                        font.pixelSize: Astrea.Theme.fontSizeSmall
                        font.weight: Astrea.Theme.fontWeightDemiBold
                        elide: Text.ElideRight
                    }

                    Astrea.TextLabel {
                        Layout.fillWidth: true
                        text: modelData.duration || "0s"
                        textColor: Astrea.Theme.textPrimary
                        font.pixelSize: Astrea.Theme.fontSizeNormal
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Astrea.TextLabel {
            Layout.fillWidth: true
            visible: !root.legendRows || root.legendRows.length === 0
            text: qsTr("Sem dados ainda")
            textColor: Astrea.Theme.textTertiary
            font.pixelSize: Astrea.Theme.fontSizeNormal
        }
    }
}
