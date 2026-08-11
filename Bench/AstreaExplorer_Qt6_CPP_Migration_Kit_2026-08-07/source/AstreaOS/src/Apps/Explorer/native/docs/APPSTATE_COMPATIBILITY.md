# Explorer `AppState` Compatibility Boundary

`AppState.qml` remains the only public Explorer contract. The Qt runtime
registers the native projection only as `NativeAppState` under
`Astrea.Explorer.Native 1.0`; there is no native `AppState` registration or
context property. The public QML file does not import the native module
unconditionally: the native executable sets the process-local
engine-owned `astreaNativeAppStateAvailable` capability after registering the
facade, and the file loads `compatibility/NativeAppStateAdapter.qml` only when
that capability is true and the adapter is ready. Without the capability,
it loads the legacy adapter and never resolves `Astrea.Explorer.Native`, which
preserves Quickshell/portal fallback loading.
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
| `loadRecent`, `recordRecentAccess` | `AppStateFacade` and `RecentController`/`RecentStore` |
| `DirectoryModel.count`, `DirectoryModel.get(index)` | `DirectoryModel` QML compatibility projection |

The compatibility methods `replaceFileModel`, `updateFileModelMetadata`, and
`removePathsFromFileModel` are also native-bound when the capability is active.
They use the current navigation generation, emit targeted model signals, and
let `SelectionController` reconcile selected paths after updates/removals.

## Ownership at this phase

Native-authoritative domains are navigation/history/tabs/search, directory
listing/model/loading/error state, the `recent://` directory projection via
`RecentController`, Recent source loading/recording/persistence via
`RecentStore`, directory watching, selection and model refresh
reconciliation, listing/preview/view/zoom settings, sidebar favorite state,
dialog filter state, resolver-rooted runtime paths, and copy/cut clipboard
state including system clipboard publication through `ClipboardService`.

The Recent qualification covers the complete native boundary: desktop and file
entries are normalized by `RecentStore` and `RecentController`, exposed
through the existing `DirectoryModel` roles, and ordered by access timestamp.
Finder snapshots are written with `QSaveFile`; saves run asynchronously and
coalesce to the newest in-memory snapshot. The native load path reads Finder,
launch-history, and XBEL sources off the UI thread, applies request-generation
guards, and reuses one desktop catalog per load. The QML `RecentState` remains
available only as the legacy fallback and self-disables its load/save/record
operations when native navigation is authoritative.
Launch-history entries with missing, malformed, or zero timestamps preserve a
valid target and fall back to its filesystem modification time, matching the
legacy Python helper; valid positive timestamps remain authoritative. Invalid
launch identities and missing launch/XBEL targets remain filtered. Finder is
different by contract: the legacy Finder loader preserves valid serialized
entries after their targets disappear, so the native projection preserves
their serialized identity and metadata while still rejecting empty paths.
Recent source parsing remains bounded: launch history is read newest-first only
until the configured unique-path limit, serialized objects are capped at 1 MiB,
and desktop paths are resolved through one cached application catalog per load
to avoid repeated application-directory scans.

Legacy/transitional domains are paste conflict behavior, archive and file
operation flows, trash/delete/restore, previews and thumbnail warming, recent
item fallback state, devices and network dialogs, portal fallback,
open-with and launch menus, helper-backed sidebar actions, and toolbar search
suggestions. The legacy navigation module and its directory/search/watch
`Process` objects remain as guarded fallback/reference code and are disabled
as a group whenever native navigation is authoritative.
The legacy navigation module remains in the source tree for fallback/reference
purposes, but its initialization and directory/search/watch `Process` objects
are guarded whenever the native bridge is active.

## Future candidates

These are candidates for later, separately qualified migrations; none are part
of this phase:

- paste/conflict and the remaining file-operation, archive, AppImage, and
  trash workflows;
- preview rendering, thumbnail warming, and device/network services;
- portal fallback and open-with/launch menus;
- helper-backed actions, toolbar suggestions, and scroll-state persistence.

The qualification does not add a second state architecture or migrate any new
QML `Process` domain. It verifies the current compatibility boundary with
controlled native completions, including coalesced saves and stale load
snapshots, while retaining the QML helper path only for fallback runtimes.

`DirectoryModel` keeps the legacy role names (`fileName`, `filePath`,
`fileUrl`, `fileIsDir`, `fileExecutable`, `fileHidden`, `fileSize`,
`fileModified`, `fileKind`, `filePreviewUrl`, `fileRemote`,
`fileMetadataLimited`, `fileFilesystem`, `lastAccessed`, `recentSource`, and
the native recent-app `fileIconName` role). Selection is reconciled by stable
path when the model resets, emits row removals, or receives targeted metadata
changes.

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
| recents and scroll state | `recentVirtualPath`, `rememberScrollPosition` | `RecentController`/`RecentStore` for recents; scroll state remains QML |
| portal fallback and helper-backed actions | portal dialog fallback, `helperPath`, and helper-backed launch/sidebar actions | `PortalController`, filesystem/launch services |

This boundary prevents the facade from becoming a second monolithic QML state
machine. The legacy singleton and launcher remain available as fallback while
these groups are migrated incrementally.
