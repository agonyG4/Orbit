.pragma library

function metricsText(messages, key, fallback, values) {
    var text = (messages && messages[key]) || fallback
    for (var i = 0; i < values.length; ++i)
        text = text.replace("%" + (i + 1), values[i])
    return text
}

function applyMetricsState(appState, messages, target) {
    if (target.metricsRequestId === 0
            || target.metricsRequestId !== appState.directoryMetricsRequestId)
        return false

    var files = Number(appState.directoryMetricsFileCount)
    var folders = Number(appState.directoryMetricsDirectoryCount)
    var counts = metricsText(
        messages,
        "apps.explorer.properties.text.files_and_folders",
        "%1 files, %2 folders",
        [files, folders])
    var state = appState.directoryMetricsState
    if (state === "running") {
        var liveSize = appState.formatSize(Number(appState.directoryMetricsBytes))
        target.propSize = Number(appState.directoryMetricsBytes) > 0
            ? liveSize + " ("
                + metricsText(
                    messages,
                    "apps.explorer.properties.text.calculating",
                    "Calculating…",
                    [])
                + ")"
            : metricsText(
                messages,
                "apps.explorer.properties.text.calculating",
                "Calculating…",
                [])
        target.propContains = counts
        target.errorText = ""
        return true
    }
    if (state === "success") {
        target.propSize = appState.formatSize(Number(appState.directoryMetricsBytes))
        target.propContains = counts
        target.errorText = ""
        return true
    }
    if (state === "partial") {
        target.propSize = metricsText(
            messages,
            "apps.explorer.properties.text.at_least",
            "At least %1",
            [appState.formatSize(Number(appState.directoryMetricsBytes))])
        target.propContains = counts
        target.errorText = appState.directoryMetricsError
            || metricsText(
                messages,
                "apps.explorer.properties.text.some_items_unreadable",
                "Some items could not be read.",
                [])
        return true
    }
    if (state === "failed") {
        target.errorText = appState.directoryMetricsError
            || metricsText(
                messages,
                "apps.explorer.properties.text.metrics_failed",
                "Could not calculate folder size.",
                [])
        return true
    }
    return false
}

function markStartFailure(appState, messages, target) {
    target.metricsRequestId = 0
    target.propSize = metricsText(
        messages,
        "apps.explorer.properties.text.metrics_failed",
        "Could not calculate folder size.",
        [])
    target.propContains = ""
    target.errorText = appState.directoryMetricsError
        || metricsText(
            messages,
            "apps.explorer.properties.text.metrics_failed",
            "Could not calculate folder size.",
            [])
    target.isLoading = false
}

function handleMetricsSuperseded(messages, target, requestId) {
    if (requestId !== target.metricsRequestId)
        return false
    target.metricsRequestId = 0
    target.propSize = metricsText(
        messages,
        "apps.explorer.properties.text.metrics_replaced",
        "Calculation replaced.",
        [])
    target.propContains = ""
    target.errorText = target.propSize
    target.isLoading = false
    return true
}
