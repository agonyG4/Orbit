import QtQuick 2.15

// Compatibility surface retained for visual QML that still references the
// historical state object. Navigation itself is owned by NativeAppState.
QtObject {
    id: navigation

    property QtObject app
    readonly property bool nativeOwned: true
    readonly property bool legacyProcessExecutionEnabled: false
    property int nextTabId: 1
    property string activeDirectoryRequestPath: ""
    property bool remoteDirectoryActive: app ? app.remoteDirectoryActive : false
    property string remoteDirectoryReason: ""
    property string searchRootPath: ""

    function initialize() {}
    function stopTransitionalProcesses() {}
    function refreshCurrentFolder() { return app ? app.refreshCurrentFolder() : 0 }
    function navigateTo(path) { return app ? app.navigateTo(path) : 0 }
    function goBack() { if (app) app.goBack() }
    function goForward() { if (app) app.goForward() }
    function startSearch() { if (app) app.startSearch() }
    function hideSearch() { if (app) app.hideSearch() }
    function submitSearch(query) { return app ? app.submitSearch(query || "") : 0 }
    function clearSearch() { if (app) app.clearSearch() }
    function createTab(path) { if (app) app.createTab(path || "") }
    function closeTab(index) { if (app) app.closeTab(index) }
    function closeTabById(tabId) { if (app) app.closeTabById(tabId) }
    function switchTab(index) { if (app) app.switchTab(index) }
    function switchTabById(tabId) { if (app) app.switchTabById(tabId) }
    function tabIndexById(tabId) { return app ? app.tabIndexById(tabId) : -1 }
    function moveTab(fromIndex, toIndex) { if (app) app.moveTab(fromIndex, toIndex) }
}
