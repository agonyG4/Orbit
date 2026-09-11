.pragma library

// Selection is path-authoritative through selectedPathsInCurrentFolder; drag
// ownership is intentionally determined only by explicit drag metadata.

function normalizeFileUrl(url) {
    if (url && typeof url.toLocalFile === "function") {
        try {
            const localPath = String(url.toLocalFile() || "")
            if (!/[\r\n\u0000]/.test(localPath))
                return localPath
        } catch (error) {
            return ""
        }
    }

    const value = String(url || "").trim()
    if (/[\r\n\u0000]/.test(value) || value.indexOf("file://") !== 0)
        return ""
    const encoded = value.slice("file://".length)
    try {
        const path = decodeURIComponent(encoded)
        return /[\r\n\u0000]/.test(path) ? "" : path
    } catch (error) {
        return /[\r\n\u0000]/.test(encoded) ? "" : encoded
    }
}

function appendUniquePath(paths, seen, path) {
    const value = String(path || "")
    if (!value || /[\r\n\u0000]/.test(value) || seen[value])
        return
    seen[value] = true
    paths.push(value)
}

function appendUriList(paths, seen, text) {
    const entries = String(text || "").split(/\r?\n/)
    for (var i = 0; i < entries.length; i++) {
        const entry = entries[i].trim()
        if (!entry || entry.indexOf("#") === 0)
            continue
        const path = normalizeFileUrl(entry)
        if (path)
            appendUniquePath(paths, seen, path)
    }
}

function appendPlainPathList(paths, seen, text) {
    const entries = String(text || "").split(/\r?\n/)
    for (var i = 0; i < entries.length; i++) {
        const value = String(entries[i] || "").trim()
        if (value.indexOf("/") === 0)
            appendUniquePath(paths, seen, value)
    }
}

function appendDropUrl(paths, seen, url) {
    if (url && typeof url.toLocalFile === "function") {
        const path = normalizeFileUrl(url)
        if (path)
            appendUniquePath(paths, seen, path)
        return
    }

    const value = String(url || "").trim()
    if (/[\r\n\u0000]/.test(value))
        return
    const path = normalizeFileUrl(value)
    if (path)
        appendUniquePath(paths, seen, path)
}

function dataAsString(drop, format) {
    if (!drop || typeof drop.getDataAsString !== "function")
        return ""
    try {
        return drop.getDataAsString(format) || ""
    } catch (error) {
        return ""
    }
}

function dropPaths(drop) {
    const urls = [].concat((drop && drop.urls) || [])
    const paths = []
    const seen = {}
    for (var i = 0; i < urls.length; i++) {
        appendDropUrl(paths, seen, urls[i])
    }

    appendUriList(paths, seen, dataAsString(drop, "text/uri-list"))
    appendPlainPathList(paths, seen, dataAsString(drop, "text/plain"))
    if (drop && drop.text) {
        appendUriList(paths, seen, drop.text)
        appendPlainPathList(paths, seen, drop.text)
    }

    return paths
}

function dropModeFor(drop, appState) {
    if (drop && drop.source)
        return "move"
    if (dataAsString(drop, "application/x-astrea-explorer-internal-drag") === "move")
        return "move"
    return "copy"
}

function dragImageUrl(previewUrl, fallbackIconUrl) {
    if (previewUrl)
        return previewUrl
    return fallbackIconUrl || ""
}

function handleDroppedUrls(appState, drop, destinationPath) {
    const paths = dropPaths(drop)
    if (!paths || paths.length === 0)
        return false
    appState.dropFilePaths(
        paths,
        destinationPath || appState.currentPath,
        dropModeFor(drop, appState)
    )
    drop.accepted = true
    return true
}
