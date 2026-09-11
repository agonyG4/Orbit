import QtQuick 2.15
import QtQml.Models 2.15
import "../../qml/utils/ModelAdapters.js" as ModelAdapters

QtObject {
    id: root

    property var iconNamesModel: ListModel {
        ListElement { value: "folder-link" }
        ListElement { value: "inode-directory" }
    }

    property var emblemNamesModel: ListModel {
        ListElement { value: "symbolic-link-symbolic" }
        ListElement { value: "readonly-symbolic" }
    }

    property var modelDataNamesModel: ListModel {
        ListElement { modelData: "folder-link" }
        ListElement { modelData: "inode-directory" }
    }

    property var sourceModel: ListModel {}
    property var displayModel: ListModel {}
    property var sectionModel: ListModel {}
    property int projectionRebuilds: 0

    function fail(message) {
        console.error(message)
        Qt.exit(1)
    }

    function assertCondition(condition, message) {
        if (!condition)
            fail(message)
        return condition
    }

    function replaceModelStrings(model, values) {
        model.clear()
        var normalized = ModelAdapters.stringList(values)
        for (var i = 0; i < normalized.length; i++)
            model.append({ value: normalized[i] })
    }

    function syncProjectionMetadata() {
        var source = sourceModel.get(0)
        var row = displayModel.get(0)
        if (!ModelAdapters.stringListEquals(row.fileIconNames, source.fileIconNames)
                || !ModelAdapters.stringListEquals(row.fileEmblemNames, source.fileEmblemNames)) {
            replaceModelStrings(row.fileIconNames, source.fileIconNames)
            replaceModelStrings(row.fileEmblemNames, source.fileEmblemNames)
        }
        displayModel.setProperty(0, "fileIconMetadataReady", source.fileIconMetadataReady)

        var section = sectionModel.get(0)
        var items = section.items
        for (var i = 0; i < ModelAdapters.listCount(items); i++) {
            var item = ModelAdapters.listAt(items, i)
            if (item.sourceIndex === 0) {
                replaceModelStrings(item.fileIconNames, source.fileIconNames)
                replaceModelStrings(item.fileEmblemNames, source.fileEmblemNames)
            }
        }
    }

    Component.onCompleted: {
        assertCondition(ModelAdapters.listCount(iconNamesModel) === 2,
                "nested icon model count was not readable")
        assertCondition(ModelAdapters.stringList(iconNamesModel)[0] === "folder-link",
                "first nested icon name was not normalized")
        assertCondition(ModelAdapters.stringList(iconNamesModel)[1] === "inode-directory",
                "second nested icon name was not normalized")
        assertCondition(ModelAdapters.stringList(modelDataNamesModel)[0] === "folder-link"
                    && ModelAdapters.stringList(modelDataNamesModel)[1] === "inode-directory",
                "modelData-backed string values were not normalized")

        assertCondition(ModelAdapters.listCount(emblemNamesModel) === 2,
                "nested emblem model count was not readable")
        var emblems = ModelAdapters.stringList(emblemNamesModel)
        assertCondition(emblems[0] === "symbolic-link-symbolic",
                "first nested emblem was not preserved")
        assertCondition(emblems[1] === "readonly-symbolic",
                "second nested emblem was not preserved")

        sectionModel.append({
            title: "Folders",
            items: [
                { sourceIndex: 0, fileName: "first", filePath: "/first" },
                { sourceIndex: 1, fileName: "second", filePath: "/second" }
            ]
        })
        var sectionItems = sectionModel.get(0).items
        assertCondition(ModelAdapters.listCount(sectionItems) === 2,
                "nested section items count was not readable")
        var firstItem = ModelAdapters.listAt(sectionItems, 0)
        var secondItem = ModelAdapters.listAt(sectionItems, 1)
        assertCondition(firstItem.fileName === "first" && firstItem.filePath === "/first",
                "first nested section item was not readable")
        assertCondition(secondItem.fileName === "second" && secondItem.filePath === "/second",
                "second nested section item was not readable")

        assertCondition(ModelAdapters.stringListEquals(
                    ["folder-link", "inode-directory"], iconNamesModel),
                "array and nested model should compare equal")
        assertCondition(!ModelAdapters.stringListEquals(["folder-link"], iconNamesModel),
                "different list lengths should not compare equal")
        assertCondition(!ModelAdapters.stringListEquals(["folder-link", "other"], iconNamesModel),
                "different list values should not compare equal")
        assertCondition(!ModelAdapters.stringListEquals(
                    ["inode-directory", "folder-link"], iconNamesModel),
                "different list order should not compare equal")

        sourceModel.append({
            sourceIndex: 0,
            fileName: "first",
            filePath: "/first",
            fileIconNames: [{ value: "initial" }],
            fileEmblemNames: [{ value: "initial" }],
            fileIconMetadataReady: false
        })
        displayModel.append({
            sourceIndex: 0,
            fileName: "first",
            filePath: "/first",
            fileIconNames: [{ value: "initial" }],
            fileEmblemNames: [{ value: "initial" }],
            fileIconMetadataReady: false
        })
        sectionModel.clear()
        sectionModel.append({
            title: "Folders",
            items: [{
                sourceIndex: 0,
                fileName: "first",
                filePath: "/first",
                fileIconNames: [{ value: "initial" }],
                fileEmblemNames: [{ value: "initial" }]
            }]
        })
        replaceModelStrings(displayModel.get(0).fileIconNames, [])
        replaceModelStrings(displayModel.get(0).fileEmblemNames, [])
        replaceModelStrings(
            ModelAdapters.listAt(sectionModel.get(0).items, 0).fileIconNames, [])
        replaceModelStrings(
            ModelAdapters.listAt(sectionModel.get(0).items, 0).fileEmblemNames, [])
        sourceModel.setProperty(0, "fileIconNames", [])
        sourceModel.setProperty(0, "fileEmblemNames", [])
        assertCondition(ModelAdapters.stringList(sourceModel.get(0).fileIconNames).length === 0,
                "source icon metadata was not initially empty")
        assertCondition(ModelAdapters.stringList(sourceModel.get(0).fileEmblemNames).length === 0,
                "source emblem metadata was not initially empty")
        projectionRebuilds = 1

        sourceModel.set(0, {
            sourceIndex: 0,
            fileName: "first",
            filePath: "/first",
            fileIconNames: ["folder-link", "inode-directory"],
            fileEmblemNames: ["symbolic-link-symbolic", "readonly-symbolic"],
            fileIconMetadataReady: true
        })
        syncProjectionMetadata()

        var display = displayModel.get(0)
        assertCondition(ModelAdapters.stringList(display.fileIconNames)[0] === "folder-link",
                "list projection lost the first icon name")
        assertCondition(ModelAdapters.stringList(display.fileIconNames)[1] === "inode-directory",
                "list projection lost the second icon name")
        assertCondition(ModelAdapters.stringList(display.fileEmblemNames).length === 2,
                "list projection lost an emblem")
        assertCondition(Boolean(display.fileIconMetadataReady),
                "list projection did not receive metadata readiness")

        var hydratedItems = sectionModel.get(0).items
        var hydratedTile = ModelAdapters.listAt(hydratedItems, 0)
        var gridIcons = ModelAdapters.stringList(hydratedTile.fileIconNames)
        var gridEmblems = ModelAdapters.stringList(hydratedTile.fileEmblemNames)
        assertCondition(gridIcons[0] === "folder-link" && gridIcons[1] === "inode-directory",
                "grid projection lost ordered icon names")
        assertCondition(gridEmblems[0] === "symbolic-link-symbolic"
                    && gridEmblems[1] === "readonly-symbolic",
                "grid projection lost ordered emblems")
        assertCondition(projectionRebuilds === 1,
                "metadata hydration rebuilt the projection model")

        Qt.exit(0)
    }
}
