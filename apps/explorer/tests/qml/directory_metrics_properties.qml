import QtQuick 2.15
import "../../qml/state/DirectoryMetricsProperties.js" as DirectoryMetricsProperties

Item {
    id: root
    width: 1
    height: 1

    property var messages: ({})
    property var appState: ({})
    property QtObject propertiesA: QtObject {
        property int metricsRequestId: 1
        property string propSize: "Calculating…"
        property string propContains: "Calculating…"
        property string errorText: ""
        property bool isLoading: true
    }
    property QtObject propertiesB: QtObject {
        property int metricsRequestId: 2
        property string propSize: "Calculating…"
        property string propContains: "Calculating…"
        property string errorText: ""
        property bool isLoading: true
    }

    Text { id: visibleSizeA; text: propertiesA.propSize }
    Text { id: visibleCountsA; text: propertiesA.propContains }
    Text { id: visibleSizeB; text: propertiesB.propSize }
    Text { id: visibleCountsB; text: propertiesB.propContains }

    function fail(message) {
        console.error(message)
        Qt.exit(1)
    }

    function assertCondition(condition, message) {
        if (!condition)
            fail(message)
        return condition
    }

    Component.onCompleted: {
        appState = {
            directoryMetricsRequestId: 1,
            directoryMetricsState: "running",
            directoryMetricsBytes: 10,
            directoryMetricsFileCount: 1,
            directoryMetricsDirectoryCount: 0,
            directoryMetricsError: "",
            formatSize: function(bytes) { return bytes + " B" }
        }

        DirectoryMetricsProperties.applyMetricsState(appState, messages, propertiesA)
        assertCondition(visibleSizeA.text === "10 B (Calculating…)",
            "first running update was not rendered")
        assertCondition(visibleCountsA.text === "1 files, 0 folders",
            "first running counts were not rendered")

        appState.directoryMetricsBytes = 20
        appState.directoryMetricsFileCount = 2
        DirectoryMetricsProperties.applyMetricsState(appState, messages, propertiesA)
        assertCondition(visibleSizeA.text === "20 B (Calculating…)",
            "second running update did not refresh visible size")
        assertCondition(visibleCountsA.text === "2 files, 0 folders",
            "second running update did not refresh visible counts")

        appState.directoryMetricsRequestId = 2
        assertCondition(
            DirectoryMetricsProperties.handleMetricsSuperseded(messages, propertiesA, 1),
            "superseded request was not acknowledged")
        assertCondition(propertiesA.metricsRequestId === 0,
            "superseded Properties request remained active")
        assertCondition(propertiesA.propSize !== "Calculating…"
                    && propertiesA.propSize.indexOf("Calculating…") === -1,
            "superseded Properties remained calculating")

        appState.directoryMetricsBytes = 30
        appState.directoryMetricsFileCount = 3
        DirectoryMetricsProperties.applyMetricsState(appState, messages, propertiesB)
        assertCondition(visibleSizeB.text === "30 B (Calculating…)",
            "replacement Properties did not receive live progress")
        assertCondition(visibleCountsB.text === "3 files, 0 folders",
            "replacement Properties did not receive live counts")

        Qt.exit(0)
    }
}
