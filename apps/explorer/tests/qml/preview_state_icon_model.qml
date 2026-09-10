import QtQuick 2.15
import QtQml.Models 2.15

QtObject {
    id: root

    property var iconNamesModel: ListModel {
        ListElement { value: "folder-link" }
        ListElement { value: "inode-directory" }
    }

    property var bridge: QtObject {
        function richFileIconSource(path, isFolder, isExecutable, size, semanticIconName, iconNames, iconFileUrl, iconFileVersion, devicePixelRatio) {
            if (!Array.isArray(iconNames)) {
                console.error("icon names were not converted to a JavaScript array")
                return "bad"
            }
            if (iconNames.length !== 2
                    || iconNames[0] !== "folder-link"
                    || iconNames[1] !== "inode-directory") {
                console.error("icon names were converted incorrectly")
                return "bad"
            }
            return "ok"
        }
    }

    property var nativeAppState: root.bridge
    property int iconThemeRevision: 1

    Component.onCompleted: {
        var component = Qt.createComponent(Qt.resolvedUrl("../../qml/state/PreviewState.qml"))
        if (component.status !== Component.Ready) {
            console.error(component.errorString())
            Qt.exit(3)
            return
        }

        var preview = component.createObject(root, { app: root })
        if (!preview) {
            console.error("could not create PreviewState")
            Qt.exit(4)
            return
        }
        preview.app = root
        if (!preview.app || !preview.app.nativeAppState
                || !preview.bridge
                || typeof preview.bridge.richFileIconSource !== "function") {
            console.error("PreviewState bridge was not connected")
            Qt.exit(6)
            return
        }

        var source = preview.richFileIconSource(
            "/tmp/example.txt", false, false, 32, "", root.iconNamesModel, "", "", 1.0)
        if (source !== "ok") {
            console.error("unexpected icon source: " + source)
            Qt.exit(5)
            return
        }
        Qt.exit(0)
    }
}
