# Explorer `AppState` Compatibility Boundary

`AppState.qml` remains the only public Explorer contract. The Qt runtime
registers the native projection only as `NativeAppState` under
`Astrea.Explorer.Native 1.0`; there is no native `AppState` registration or
context property. The public QML file does not import the native module
unconditionally: the native executable sets `ASTREA_EXPLORER_NATIVE_RUNTIME`
and the file loads `compatibility/NativeAppStateAdapter.qml` only in that
runtime. Without the marker, it loads the legacy adapter and never resolves
`Astrea.Explorer.Native`, which preserves Quickshell/portal fallback loading.
The wrapper delegates only domains already represented by native controllers
and keeps legacy implementations for the rest.

## Native facade surface

The following QML-observed members are backed by native owners and covered by
Qt tests:

| QML member | Native owner |
| --- | --- |
| `fileModel`, `fileModelRevision`, `fileModelFilling` | `DirectoryModel` and model-reset projection |
| `currentPath`, `history`, `historyIdx`, `tabs`, `activeTabIndex`, `breadcrumbParts` | `NavigationController` |
| `loadingDir`, `loadError`, `remoteDirectoryActive` | `NavigationController` and typed backend failure path |
| `searchActive`, `searchQuery`, `navigateTo`, `submitSearch`, `goBack`, `goForward`, `refreshCurrentFolder` | `NavigationController` |
| `createTab`, `closeTab`, `switchTab` | `NavigationController` |
| `selectedFile`, `selectedFiles`, `lastSelectedIndex`, `isSelected`, `clearSelection`, `selectAll`, `selectByName`, `handleSelection` | `SelectionController` |
| `showPreview`, `viewMode`, `sortField`, `sortAsc`, `showHidden`, `foldersFirst`, `groupingEnabled`, `zoomLevel` | `SettingsService` plus `NavigationController` listing options |
| `sidebarFavoritesJson`, `sidebarHiddenDefaultFavoritesJson` | `SettingsService` |
| `homePath`, `runtimeRoot`, `backendPath`, `helperPath`, `wallpaperManagerPath`, `astreaLaunch`, `windowsRun`, `networkRootPath`, `trashFilesPath`, `trashInfoPath`, `recentVirtualPath`, `isPortalDialog` | resolved runtime paths, process environment, and user paths |
| `dialogActive`, `dialogMode`, `dialogFilePatterns`, `fileMatchesDialogFilter` | native facade dialog projection |
| `sidebarFavorites`, `sidebarHiddenDefaultFavorites`, `sidebarFavoritesRevision`, default favorite helpers | `SettingsService` JSON projection |
| `increaseZoom`, `decreaseZoom`, `resetZoom`, `setZoom` | `SettingsService` zoom setting |
| `DirectoryModel.count`, `DirectoryModel.get(index)` | `DirectoryModel` QML compatibility projection |

The compatibility methods `replaceFileModel`, `updateFileModelMetadata`, and
`removePathsFromFileModel` are also native-bound when the marker is active.
They use the current navigation generation, emit targeted model signals, and
let `SelectionController` reconcile selected paths after updates/removals.

## Ownership at this phase

Native-authoritative domains are navigation/history/tabs/search, directory
listing/model/loading/error state, the `recent://` directory projection via
`RecentController`, directory watching, selection and model refresh
reconciliation, listing/preview/view/zoom settings, sidebar favorite state,
dialog filter state, resolver-rooted runtime paths, and copy/cut clipboard
state including system clipboard publication through `ClipboardService`.

Legacy/transitional domains are paste conflict behavior, archive and file
operation flows, trash/delete/restore, previews and thumbnail warming, recent
item persistence/recording, devices and network dialogs, portal fallback,
open-with and launch menus, helper-backed sidebar actions, and toolbar search
suggestions. The legacy navigation module and its directory/search/watch
`Process` objects remain as guarded fallback/reference code and are disabled
as a group whenever native navigation is authoritative.
The legacy navigation module remains in the source tree for fallback/reference
purposes, but its initialization and directory/search/watch `Process` objects
are guarded whenever the native bridge is active.

`DirectoryModel` keeps the legacy role names (`fileName`, `filePath`,
`fileUrl`, `fileIsDir`, `fileExecutable`, `fileHidden`, `fileSize`,
`fileModified`, `fileKind`, `filePreviewUrl`, `fileRemote`,
`fileMetadataLimited`, `fileFilesystem`, `lastAccessed`, and `recentSource`).
Selection is reconciled by stable path when the model resets, emits row
removals, or receives targeted metadata changes.

## Inventory method

The complete QML symbol inventory was generated with:

```sh
rg -o 'AppState\.[A-Za-z_][A-Za-z0-9_]*' \
  source/AstreaOS/src/Apps/Explorer --glob '*.qml' --glob '*.js' \
  | sed 's/.*AppState\.//' | sort -u
```

The current inventory has 151 names. Members not listed in the native table
are transitional QML state and are intentionally left in `AppState.qml` and
its state modules until a native owner has behavior parity. Representative
groups are:

| Transitional group | Members / responsibility | Replacement candidate |
| --- | --- | --- |
| clipboard and file operations | `clipboardFiles`, `clipboardMode`, `copySelected`, `cutSelected` are native-backed; `pasteFiles`, `pasteConflict*`, `fileOperation*`, `deleteSelected`, `restoreSelected`, `emptyTrash` remain legacy-backed | `FileOperationsController`, `FileOperationService`, `ClipboardService` |
| archive/AppImage/wallpaper | `archive*`, `startArchiveExtraction`, `startFolderCompression`, `installAppImage`, `setAsWallpaper`, `wallpaperApplyRunning` | typed filesystem/archive/application services |
| devices and network | `deviceModel`, `deviceError`, `requestMountDevice`, `requestUnmountDevice`, `requestRemountDevice`, `toggleDeviceAutoMount`, `network*`, `connectToNetwork`, `openNetworkBrowser` | `DeviceController` and a native network/portal service |
| preview and launch | `fileIconName`, `isPreviewableFile`, `portalIconSource`, `sidebarIconSource`, `formatSize`, `formatDate`, `itemColor`, `openItem`, `requestThumbnailWarm`, `scheduleVisibleThumbnailWarm`, zoom helpers | `PreviewController`, `LaunchService`, `OpenWithController` |
| recents and scroll state | `recentVirtualPath`, `rememberScrollPosition` | `RecentController` plus a future native persistence service |
| portal fallback and helper-backed actions | portal dialog fallback, `helperPath`, and helper-backed launch/sidebar actions | `PortalController`, filesystem/launch services |

This boundary prevents the facade from becoming a second monolithic QML state
machine. The legacy singleton and launcher remain available as fallback while
these groups are migrated incrementally.
