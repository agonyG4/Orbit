# Target Qt 6 / C++ Architecture

## High-Level Layout

```text
Astrea Explorer
|
+-- C++ application layer
|   +-- ExplorerApplication
|   +-- AppState facade
|   +-- NavigationController
|   +-- SelectionController
|   +-- FileOperationsController
|   +-- PreviewController
|   +-- DeviceController
|   +-- RecentController
|   +-- OpenWithController
|   +-- PortalController
|   +-- DirectoryModel
|   +-- RustBackendClient
|   +-- ClipboardService
|   +-- DirectoryWatchService
|   +-- SettingsService
|   +-- LaunchService
|
+-- Rust backend
|   +-- existing CLI commands
|   +-- future persistent `serve` mode
|
+-- QML presentation
    +-- Main.qml
    +-- Theme.qml
    +-- FileDialog.qml
    +-- PortalDialog presentation
    +-- components/common
    +-- components/layout
    +-- components/views
```

## `ExplorerApplication`

Own exactly one `QGuiApplication` and one `QQmlApplicationEngine` for the normal Explorer process.

Set application identity in C++ rather than in `Component.onCompleted`:

```text
application name: Explorer
organization: agony (preserve compatibility initially)
domain: local (preserve compatibility initially)
```

Do not change settings paths during the migration unless a dedicated compatibility migration is implemented.

## C++ `AppState` Compatibility Facade

The visual QML already depends heavily on `AppState`. Preserve that surface.

Register a typed C++ singleton under the Explorer QML module instead of forcing all UI files to learn new controller names immediately.

`AppState` should expose stable properties/functions while delegating to focused controllers.

This gives the migration a compatibility seam:

```text
existing visual QML
 -> same AppState names
 -> C++ facade
 -> focused C++ controllers/models
```

Do not turn the C++ facade into another 2,000-line god object. It is an API facade, not the owner of every implementation detail.

## `DirectoryModel`

Replace the mutable QML `ListModel` with `QAbstractListModel`.

Preserve existing roles:

```text
fileName
filePath
fileUrl
fileIsDir
fileExecutable
fileHidden
fileSize
fileModified
fileKind
filePreviewUrl
fileRemote
fileMetadataLimited
fileFilesystem
```

Add recent-only roles only where currently required (`lastAccessed`, `recentSource`) without breaking ordinary directory delegates.

Prefer reset-by-generation or targeted row updates over thousands of `ListModel.append()` calls from JavaScript.

## Navigation

`NavigationController` owns:

- current path;
- tabs;
- back/forward history;
- breadcrumbs;
- search state;
- loading generation;
- remote-directory state;
- request cancellation;
- directory watcher lifetime.

A stale directory/search result must never overwrite a newer navigation request. Use explicit request/generation IDs.

## Selection

`SelectionController` owns selected identities and range selection.

Preserve current visible behavior and `AppState.selectedFiles` compatibility. Do not redesign selection during this migration.

## File Operations

`FileOperationsController` owns:

- clipboard operation state;
- copy/cut/paste;
- conflict workflow;
- file-operation progress;
- archive progress/password/conflict state;
- trash/restore/empty;
- AppImage install;
- wallpaper action handoff.

Destructive filesystem implementation belongs in Rust where practical. C++ owns UI-facing workflow state and cancellation.

## Preview

`PreviewController` owns request scheduling, thumbnail warm generations, open-file dispatch, and preview metadata updates.

Pure visual formatting such as colors and trivial date/size formatting may remain QML initially if moving it provides no correctness/performance benefit.

## Clipboard

Use Qt clipboard APIs rather than `wl-copy`/`wl-paste` for Explorer-owned clipboard workflows.

Keep MIME handling explicit and preserve `text/uri-list` interoperability.

## File Watching

Use `QFileSystemWatcher` for the active local directory and debounce refreshes. Remote directory policy must remain conservative; current code intentionally disables local watcher/thumbnail behavior for remote listings.

## Process Ownership

External commands that remain legitimate must be started by typed C++ services using `QProcess` with:

- argv arrays, never shell command concatenation;
- explicit lifetime ownership;
- output caps;
- timeouts where appropriate;
- cancellation;
- typed success/error mapping.

No production QML file should contain a generic `Process {}` node in the final architecture.
