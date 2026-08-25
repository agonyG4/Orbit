import QtQuick
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea

Rectangle {
    id: root

    property var model: []
    property string currentValue: ""
    property int controlHeight: 40
    property color surfaceColor: Qt.rgba(1, 1, 1, 0.08)
    property color selectedColor: Qt.rgba(1, 1, 1, 0.24)
    property color borderColor: Qt.rgba(1, 1, 1, 0.06)
    property color selectedTextColor: Astrea.Theme.textPrimary
    property color textColor: Astrea.Theme.textSecondary

    signal valueChanged(string value)

    Layout.fillWidth: true
    implicitHeight: controlHeight
    radius: controlHeight / 2
    color: surfaceColor
    border.width: 1
    border.color: borderColor

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3

        Repeater {
            model: root.model || []

            Rectangle {
                id: segment

                required property var modelData
                readonly property string value: String(modelData.value || "")
                readonly property string label: String(modelData.label || value)
                readonly property bool selected: root.currentValue === value

                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Math.max(0, root.controlHeight / 2 - 3)
                color: selected ? root.selectedColor : "transparent"
                scale: pressArea.pressed ? 0.985 : 1

                Behavior on color { ColorAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: Astrea.Theme.animationQuick; easing.type: Easing.OutCubic } }

                Astrea.TextLabel {
                    anchors.centerIn: parent
                    text: segment.label
                    textColor: segment.selected ? root.selectedTextColor : root.textColor
                    font.pixelSize: Astrea.Theme.fontSizeNormal
                    font.weight: Astrea.Theme.fontWeightDemiBold
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: pressArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentValue = segment.value
                        root.valueChanged(segment.value)
                    }
                }
            }
        }
    }
}
