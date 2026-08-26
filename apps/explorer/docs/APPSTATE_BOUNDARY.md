# Explorer `AppState` boundary

`AppState.qml` is the only public Explorer QML state contract. The native
runtime registers `NativeAppState` in `Astrea.Explorer.Native 1.0`, and
`AppState.qml` imports that module directly:

```qml
readonly property QtObject nativeAppState: NativeAppState
```

There is one supported runtime path:

```text
NativeAppState -> AppState.qml -> Explorer presentation
```

The executable registers the native singleton before loading the Explorer
QML. No environment marker, capability probe, Loader, legacy adapter, or
fallback AppState exists.

## Native facade surface

The public wrapper preserves the presentation-facing API used by Main,
FileDialog, PortalDialog, Sidebar, Toolbar, PreviewPanel, and the file views.
The native facade owns the following state and operations:

| QML surface | Native owner |
| --- | --- |
| `currentPath`, history, tabs, breadcrumbs, search, navigation | `NavigationController` |
| `fileModel`, loading/error state, model revisions | `DirectoryModel` and `NavigationController` |
| selection properties and selection operations | `SelectionController` |
| listing, preview, zoom, and sidebar settings | `ExplorerSettingsController`, `SidebarFavoritesController` |
| clipboard, file operations, and paste conflicts | `FileOperationsController` and `ClipboardService` |
| archive extraction/compression and continuation state | `ArchiveController` and `FilesystemService` |
| devices and auto-mount state | `DeviceController` |
| recent loading and recording | `RecentController` and `RecentStore` |
| runtime paths, portal state, icons, launch, and filesystem actions | the corresponding facade services/controllers |

`FileOperationsState.qml`, `PreviewState.qml`, and
`DeviceNetworkState.qml` remain QML presentation state objects. They bridge
directly to `AppState.nativeAppState`; they do not select or emulate another
runtime. `FileOperationsState` also synthesizes the aggregate operation
snapshots consumed by `OperationProgressPresenter.qml` from the native
`fileOperationStateChanged()` and `archiveStateChanged()` signals.

## Archive admission

`ArchiveController` owns one archive workflow slot. The facade's
`startArchiveExtraction()` and `startFolderCompression()` calls both enforce
`archiveWorkflowOccupied()`, which includes a running operation, a pending
password continuation, and a pending conflict continuation. Terminal results
release the native slot before the terminal snapshot is published, so the
presenter can continue displaying a terminal card without blocking the next
archive submission.

## Qualification boundary

The native singleton, direct AppState projection, operation snapshot bridge,
archive admission, recent persistence, portal/bootstrap setup, and writable
presentation properties are covered by the Explorer C++/QML tests. Shared
translation tests and the i18n validator run as part of the canonical Orbit
Python workflow. The pre-existing `astrea-notify` NotificationClient contract
mismatch remains a separately recorded baseline issue and is outside this
structural closure.
