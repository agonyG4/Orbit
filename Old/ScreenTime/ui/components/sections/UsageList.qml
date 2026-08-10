import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea
import "../common" as Common

Rectangle {
    id: root

    property var rows: []
    property real totalSeconds: 0
    property bool categoryMode: false

    Layout.fillWidth: true
    implicitHeight: Math.max(80, listContent.implicitHeight + Astrea.Theme.spacingMedium * 2)
    radius: Astrea.Theme.cardRadius
    color: Astrea.Theme.cardBg
    border.width: 1
    border.color: Astrea.Theme.cardBorder

    function rowLabel(row) {
        if (!row)
            return qsTr("Unknown")
        return row.label || row.id || qsTr("Unknown")
    }

    function rowClass(row) {
        if (!row)
            return ""
        return row.class || row.id || ""
    }

    function rowTitle(row) {
        if (!row)
            return ""
        return row.last_title || row.title || rowLabel(row)
    }

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
        id: listContent
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: Astrea.Theme.spacingMedium
        }
        spacing: 0

        Repeater {
            model: root.rows || []

            delegate: Rectangle {
                id: rowItem

                Layout.fillWidth: true
                Layout.preferredHeight: 62
                radius: Astrea.Theme.cornerRadiusSmall
                color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.060) : "transparent"
                scale: rowPress.pressed ? 0.992 : 1

                Behavior on color { ColorAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }

                HoverHandler { id: rowHover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: Astrea.Theme.spacingMedium

                    Common.AltTabAppIcon {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42
                        row: modelData
                        iconRadius: 10
                        fallbackRadius: iconRadius
                        fallbackFontSize: 16
                        sourcePixelSize: 192
                        fallbackColor: Astrea.Theme.cardBg
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Astrea.Theme.spacingSmall

                            Astrea.TextLabel {
                                Layout.fillWidth: true
                                text: root.rowLabel(modelData)
                                textColor: Astrea.Theme.textPrimary
                                font.pixelSize: Astrea.Theme.fontSizeLarge
                                elide: Text.ElideRight
                            }

                            Astrea.TextLabel {
                                text: modelData.duration || "0s"
                                textColor: Astrea.Theme.textSecondary
                                font.pixelSize: Astrea.Theme.fontSizeNormal
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 5
                            radius: 3
                            color: Qt.rgba(Astrea.Theme.textSecondary.r, Astrea.Theme.textSecondary.g, Astrea.Theme.textSecondary.b, 0.20)
                            clip: true

                            Rectangle {
                                anchors {
                                    left: parent.left
                                    top: parent.top
                                    bottom: parent.bottom
                                }
                                width: parent.width * Math.min(1, Number(modelData.seconds || 0) / Math.max(1, root.totalSeconds))
                                radius: parent.radius
                                color: root.categoryColor(root.categoryMode ? modelData.id : modelData.category, index)
                            }
                        }
                    }

                    Astrea.TextLabel {
                        text: ">"
                        textColor: Astrea.Theme.textTertiary
                        font.pixelSize: 26
                        opacity: 0.65
                    }
                }

                Rectangle {
                    visible: index < (root.rows || []).length - 1
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        leftMargin: 48
                    }
                    height: 1
                    color: Astrea.Theme.cardBorder
                    opacity: 0.65
                }

                MouseArea {
                    id: rowPress
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        Astrea.TextLabel {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            visible: !root.rows || root.rows.length === 0
            text: qsTr("Sem dados ainda")
            textColor: Astrea.Theme.textTertiary
            font.pixelSize: Astrea.Theme.fontSizeNormal
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
