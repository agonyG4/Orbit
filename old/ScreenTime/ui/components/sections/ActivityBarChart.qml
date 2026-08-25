import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea

Item {
    id: root

    property var rows: []
    property bool hourly: false
    property int coloredSegmentLimit: 3
    property real maxBarSeconds: scaleMaxSeconds()
    property real averageSeconds: averageFromRows()
    property bool showAverageLine: !hourly && averageSeconds > 0
    property color gridColor: Qt.rgba(Astrea.Theme.textSecondary.r, Astrea.Theme.textSecondary.g, Astrea.Theme.textSecondary.b, 0.16)
    property color remainderColor: Qt.rgba(Astrea.Theme.textSecondary.r, Astrea.Theme.textSecondary.g, Astrea.Theme.textSecondary.b, 0.42)
    property color averageColor: Astrea.Theme.successColor
    readonly property int axisLabelWidth: 34

    Layout.fillWidth: true
    Layout.preferredHeight: hourly ? 152 : 132

    function rawMaxSeconds() {
        var maxValue = 0
        var source = rows || []
        for (var i = 0; i < source.length; i++)
            maxValue = Math.max(maxValue, Number(source[i].seconds || 0))
        return maxValue
    }

    function averageFromRows() {
        var total = 0
        var source = rows || []
        if (source.length === 0)
            return 0
        for (var i = 0; i < source.length; i++)
            total += Number(source[i].seconds || 0)
        return total / source.length
    }

    function niceScaleSeconds(seconds) {
        var value = Math.max(1, Number(seconds || 0))
        var steps = [60, 300, 600, 900, 1800, 3600, 7200, 10800, 14400, 21600, 28800, 43200, 86400]
        for (var i = 0; i < steps.length; i++) {
            if (value <= steps[i])
                return steps[i]
        }
        return Math.ceil(value / 3600) * 3600
    }

    function scaleMaxSeconds() {
        return niceScaleSeconds(Math.max(rawMaxSeconds(), averageFromRows()))
    }

    function rowCount() {
        return Math.max(1, (rows || []).length)
    }

    function formatAxis(seconds) {
        var value = Math.max(0, Number(seconds || 0))
        if (value <= 0)
            return "0"
        if (value >= 3600) {
            var hours = value / 3600
            if (hours >= 10 || Math.abs(hours - Math.round(hours)) < 0.05)
                return Math.round(hours) + "h"
            return (Math.round(hours * 10) / 10) + "h"
        }
        if (value >= 60)
            return Math.round(value / 60) + "m"
        return Math.round(value) + "s"
    }

    function categoryColor(categoryId, index) {
        var id = String(categoryId || "")
        if (id === "__remaining")
            return remainderColor
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

    function segmentRows(row) {
        if (!row || Number(row.seconds || 0) <= 0)
            return []

        var total = Number(row.seconds || 0)
        var categories = row.categories || []
        if (categories.length === 0) {
            return [{
                "id": row.top_category || "",
                "seconds": total
            }]
        }

        var result = []
        var remainder = 0
        var included = 0
        var sorted = categories.slice().sort((left, right) => Number(right.seconds || 0) - Number(left.seconds || 0))
        for (var i = 0; i < sorted.length; i++) {
            var seconds = Number(sorted[i].seconds || 0)
            if (seconds <= 0)
                continue
            if (result.length < coloredSegmentLimit) {
                result.push({
                    "id": sorted[i].id || "",
                    "seconds": seconds
                })
                included += seconds
            } else {
                remainder += seconds
            }
        }

        remainder += Math.max(0, total - included - remainder)
        if (remainder > 0.5) {
            result.push({
                "id": "__remaining",
                "seconds": remainder
            })
        }
        return result
    }

    function labelFor(row, index) {
        if (!row)
            return ""
        if (!hourly)
            return row.short_label || row.label || ""
        if (index === 0)
            return "12 AM"
        if (index === 6)
            return "6 AM"
        if (index === 12)
            return "12 PM"
        if (index === 18)
            return "6 PM"
        return ""
    }

    Item {
        id: chartArea

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: axisLabels.top
            bottomMargin: 8
        }

        Rectangle {
            id: plot

            anchors {
                left: parent.left
                right: axisScale.left
                rightMargin: 4
                top: parent.top
                bottom: parent.bottom
            }
            color: "transparent"
            border.width: 1
            border.color: root.gridColor

            Repeater {
                model: 2

                Rectangle {
                    x: 0
                    y: Math.round((plot.height / 3) * (index + 1))
                    width: plot.width
                    height: 1
                    color: root.gridColor
                }
            }

            Repeater {
                model: root.hourly ? 3 : Math.max(0, root.rowCount() - 1)

                Rectangle {
                    x: root.hourly
                        ? Math.round(plot.width * ((index + 1) / 4))
                        : Math.round(plot.width * ((index + 1) / root.rowCount()))
                    y: 0
                    width: 1
                    height: plot.height
                    color: root.gridColor
                    opacity: 0.55
                }
            }

            Item {
                id: averageGuide

                visible: root.showAverageLine
                x: 0
                y: Math.max(0, Math.min(plot.height - height, Math.round(plot.height - plot.height * Math.min(1, root.averageSeconds / root.maxBarSeconds)) - Math.floor(height / 2)))
                width: plot.width
                height: 12

                Row {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: 4

                    Repeater {
                        model: Math.max(1, Math.ceil(averageGuide.width / 10))

                        Rectangle {
                            width: 5
                            height: 1
                            color: root.averageColor
                            opacity: 0.9
                        }
                    }
                }

                Astrea.TextLabel {
                    x: plot.width + 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "avg"
                    textColor: root.averageColor
                    font.pixelSize: Astrea.Theme.fontSizeTiny
                    font.weight: Astrea.Theme.fontWeightDemiBold
                }
            }

            Row {
                id: bars

                anchors.fill: parent
                anchors.margins: 4
                spacing: root.hourly ? 5 : 12

                Repeater {
                    model: root.rows || []

                    Item {
                        id: barSlot

                        required property var modelData
                        required property int index

                        width: Math.max(root.hourly ? 3 : 18, (bars.width - bars.spacing * (root.rowCount() - 1)) / root.rowCount())
                        height: bars.height
                        readonly property real totalSeconds: Math.max(1, Number(modelData.seconds || 0))
                        readonly property real barHeight: Math.max(modelData.seconds > 0 ? 3 : 0, Math.round(height * Math.min(1, Number(modelData.seconds || 0) / root.maxBarSeconds)))

                        Rectangle {
                            id: barStack

                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: Math.min(parent.width, root.hourly ? 9 : 26)
                            height: barSlot.barHeight
                            radius: Math.min(4, width / 2)
                            color: Qt.rgba(1, 1, 1, 0.08)
                            clip: true

                            Column {
                                anchors.fill: parent
                                spacing: 0

                                Repeater {
                                    model: root.segmentRows(barSlot.modelData)

                                    Rectangle {
                                        required property var modelData
                                        required property int index

                                        width: parent ? parent.width : 0
                                        height: Math.max(1, Math.round(barStack.height * Math.min(1, Number(modelData.seconds || 0) / barSlot.totalSeconds)))
                                        color: root.categoryColor(modelData.id, index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: axisScale

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            width: root.axisLabelWidth

            Astrea.TextLabel {
                anchors {
                    top: parent.top
                    right: parent.right
                }
                text: root.formatAxis(root.maxBarSeconds)
                textColor: Astrea.Theme.textTertiary
                font.pixelSize: Astrea.Theme.fontSizeTiny
                horizontalAlignment: Text.AlignRight
            }

            Astrea.TextLabel {
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                }
                text: root.formatAxis(root.maxBarSeconds / 2)
                textColor: Astrea.Theme.textTertiary
                font.pixelSize: Astrea.Theme.fontSizeTiny
                horizontalAlignment: Text.AlignRight
            }

            Astrea.TextLabel {
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                }
                text: "0"
                textColor: Astrea.Theme.textTertiary
                font.pixelSize: Astrea.Theme.fontSizeTiny
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Row {
        id: axisLabels

        anchors {
            left: parent.left
            right: parent.right
            rightMargin: root.axisLabelWidth + 4
            bottom: parent.bottom
        }
        height: 22
        spacing: root.hourly ? 5 : 12

        Repeater {
            model: root.rows || []

            Astrea.TextLabel {
                width: Math.max(root.hourly ? 3 : 18, (axisLabels.width - axisLabels.spacing * (root.rowCount() - 1)) / root.rowCount())
                text: root.labelFor(modelData, index)
                textColor: Astrea.Theme.textTertiary
                font.pixelSize: Astrea.Theme.fontSizeTiny
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }
}
