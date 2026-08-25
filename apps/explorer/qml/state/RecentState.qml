import QtQuick 2.15

// Recent storage, projection, and persistence are native. This object keeps
// the old shape available to presentation code during the compatibility step.
QtObject {
    id: recent

    property QtObject app
    readonly property bool nativeOwned: true
    property int persistenceGeneration: 0
    property int saveGeneration: 0

    function load() { if (app && app.nativeAppState) app.nativeAppState.loadRecent() }
    function recordAccess(path, isDirectory, fileUrl) {
        if (app && app.nativeAppState)
            app.nativeAppState.recordRecentAccess(path, isDirectory, fileUrl || "")
    }
    function recentModelItems() { return [] }
}
