import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../AstreaComponents" as Astrea

RowLayout {
    id: row
    property string label: ""
    property int from: 0
    property int to: 255
    property int value: 0
    property int labelWidth: 86
    signal edited(int value)

    spacing: Astrea.Theme.spacingSmall

    Text {
        Layout.preferredWidth: row.labelWidth
        text: row.label
        color: Astrea.Theme.textSecondary
        font.family: Astrea.Theme.fontFamily
        font.pixelSize: Astrea.Theme.fontSizeSmall
        elide: Text.ElideRight
    }

    Slider {
        id: slider

        Layout.fillWidth: true
        from: row.from
        to: row.to
        stepSize: 1
        value: row.value
        onMoved: row.edited(Math.max(row.from, Math.min(row.to, Math.round(value))))

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => {
                var delta = event.angleDelta.y > 0 ? 1 : -1
                row.edited(Math.max(row.from, Math.min(row.to, row.value + delta)))
            }
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            radius: 3
            color: Astrea.Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(1, 1, 1, 0.10)

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: Astrea.Theme.accent
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 18
            height: 18
            radius: 9
            color: Astrea.Theme.accentForeground
            border.width: 3
            border.color: Astrea.Theme.accent
        }
    }

    Text {
        Layout.preferredWidth: 44
        Layout.preferredHeight: 24
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        text: String(row.value)
        color: Astrea.Theme.textPrimary
        font.family: Astrea.Theme.monoFontFamily
        font.pixelSize: Astrea.Theme.fontSizeSmall
    }
}
