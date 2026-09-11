import QtQuick 2.15
import QtQml.Models 2.15
import Astrea.Files 1.0 as AstreaFiles

QtObject {
    id: root
    property bool failed: false

    readonly property string firstPath: "/tmp/folder A"
    readonly property string secondPath: "/tmp/测试"
    readonly property string firstUri: "file:///tmp/folder%20A"
    readonly property string secondUri: "file:///tmp/%E6%B5%8B%E8%AF%95"

    function fail(message) {
        console.error(message)
        failed = true
    }

    function assertCondition(condition, message) {
        if (!condition)
            fail(message)
        return condition
    }

    function assertPaths(actual, expected, message) {
        assertCondition(actual.length === expected.length, message + " count")
        for (var i = 0; i < expected.length; i++)
            assertCondition(actual[i] === expected[i], message + " at " + i)
    }

    function makeDrop(urls, uriList, plainText, source, marker) {
        return {
            urls: urls,
            source: source,
            hasText: Boolean(plainText),
            text: plainText,
            accepted: false,
            getDataAsString: function(format) {
                if (format === "text/uri-list")
                    return uriList
                if (format === "text/plain")
                    return plainText
                if (format === "application/x-astrea-explorer-internal-drag")
                    return marker || ""
                return ""
            }
        }
    }

    Component.onCompleted: {
        var expected = [firstPath, secondPath]
        var duplicatedDrop = makeDrop(
            [firstUri, secondUri],
            firstUri + "\r\n" + secondUri + "\r\n",
            firstPath + "\n" + secondPath,
            null,
            "")
        assertPaths(
            AstreaFiles.DragDropSupport.dropPaths(duplicatedDrop),
            expected,
            "duplicate MIME representations")

        var lfOnlyDrop = makeDrop(
            [],
            firstUri + "\n" + secondUri + "\n",
            "",
            null,
            "")
        assertPaths(
            AstreaFiles.DragDropSupport.dropPaths(lfOnlyDrop),
            expected,
            "LF-only URI list")

        var malformedUrlsDrop = makeDrop(
            [firstUri + "\nfile:///tmp/poison"],
            "",
            firstPath + "\n" + secondPath,
            null,
            "")
        assertPaths(
            AstreaFiles.DragDropSupport.dropPaths(malformedUrlsDrop),
            expected,
            "malformed URL channel fallback")

        var malformedPlainDrop = makeDrop(
            [],
            firstUri + "\r\n" + secondUri,
            firstPath + "\n/tmp/bad\u0000path",
            null,
            "")
        assertPaths(
            AstreaFiles.DragDropSupport.dropPaths(malformedPlainDrop),
            expected,
            "malformed plain channel")

        assertCondition(
            AstreaFiles.DragDropSupport.dropModeFor(duplicatedDrop) === "copy",
            "external drop must remain copy mode")
        var internalDrop = makeDrop([firstUri, secondUri], "", "", {}, "")
        assertCondition(
            AstreaFiles.DragDropSupport.dropModeFor(internalDrop) === "move",
            "internal source marker must preserve move mode")
        var markedDrop = makeDrop([firstUri, secondUri], "", "", null, "move")
        assertCondition(
            AstreaFiles.DragDropSupport.dropModeFor(markedDrop) === "move",
            "internal MIME marker must preserve move mode")

        var calls = []
        var appState = {
            currentPath: "/tmp/destination",
            dropFiles: function(urls, destination, mode) {
                calls.push(["dropFiles", urls, destination, mode])
            },
            dropFilePaths: function(paths, destination, mode) {
                calls.push(["dropFilePaths", paths, destination, mode])
            }
        }
        assertCondition(
            AstreaFiles.DragDropSupport.handleDroppedUrls(appState, duplicatedDrop, "/tmp/destination"),
            "shared handler rejected valid paths")
        assertCondition(calls.length === 1, "shared handler made more than one request")
        assertCondition(calls[0][0] === "dropFilePaths", "shared handler used URL-only dispatch")
        assertPaths(calls[0][1], expected, "shared handler paths")
        assertCondition(calls[0][3] === "copy", "external handler did not preserve copy mode")
        assertCondition(duplicatedDrop.accepted, "shared handler did not accept the drop")

        Qt.exit(failed ? 1 : 0)
    }
}
