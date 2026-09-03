import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property url source
    property color tintColor: "white"
    property int sourcePixelSize: 16

    Image {
        id: iconImage
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.sourcePixelSize, root.sourcePixelSize)
        fillMode: Image.PreserveAspectFit
        smooth: true
        asynchronous: false
        visible: false
    }

    Rectangle {
        id: tintSource
        anchors.fill: parent
        color: root.tintColor
        visible: false
    }

    OpacityMask {
        anchors.fill: parent
        source: tintSource
        maskSource: iconImage
        visible: iconImage.status === Image.Ready
    }
}
