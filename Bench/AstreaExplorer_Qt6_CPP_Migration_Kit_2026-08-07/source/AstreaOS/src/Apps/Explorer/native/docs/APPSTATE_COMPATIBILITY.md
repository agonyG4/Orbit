# Explorer `AppState` Compatibility Boundary

The native `AppStateFacade` is the application-state projection registered by
the Qt runtime. It owns only state already represented by native controllers:
navigation/history/tabs, the directory model, loading/error state, selection,
directory refresh, and persisted listing/preview settings. The QML property
names are preserved where the native owner has a matching contract.

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
| `autoMountDeviceIdsJson`, `sidebarFavoritesJson`, `sidebarHiddenDefaultFavoritesJson` | `SettingsService` |
| `homePath`, `runtimeRoot`, `backendPath`, `helperPath`, `wallpaperManagerPath`, `astreaLaunch`, `windowsRun`, `networkRootPath`, `trashFilesPath`, `trashInfoPath`, `recentVirtualPath`, `isPortalDialog` | resolved runtime paths, process environment, and user paths |
| `dialogActive`, `dialogMode`, `dialogFilePatterns`, `fileMatchesDialogFilter` | native facade dialog projection |
| `sidebarFavorites`, `sidebarHiddenDefaultFavorites`, `sidebarFavoritesRevision`, default favorite helpers | `SettingsService` JSON projection |
| `increaseZoom`, `decreaseZoom`, `resetZoom`, `setZoom` | `SettingsService` zoom setting |
| `DirectoryModel.count`, `DirectoryModel.get(index)` | `DirectoryModel` QML compatibility projection |

`DirectoryModel` keeps the legacy role names (`fileName`, `filePath`,
`fileUrl`, `fileIsDir`, `fileExecutable`, `fileHidden`, `fileSize`,
`fileModified`, `fileKind`, `filePreviewUrl`, `fileRemote`,
`fileMetadataLimited`, `fileFilesystem`, `lastAccessed`, and `recentSource`).
Selection is reconciled by stable path when the model resets.

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
| clipboard and file operations | `clipboardFiles`, `clipboardMode`, `copySelected`, `cutSelected`, `pasteFiles`, `pasteConflict*`, `fileOperation*`, `deleteSelected`, `restoreSelected`, `emptyTrash` | `FileOperationsController`, `FileOperationService`, `ClipboardService` |
| archive/AppImage/wallpaper | `archive*`, `startArchiveExtraction`, `startFolderCompression`, `installAppImage`, `setAsWallpaper`, `wallpaperApplyRunning` | typed filesystem/archive/application services |
| devices and network | `deviceModel`, `deviceError`, `requestMountDevice`, `requestUnmountDevice`, `requestRemountDevice`, `toggleDeviceAutoMount`, `network*`, `connectToNetwork`, `openNetworkBrowser` | `DeviceController` and a native network/portal service |
| preview and launch | `fileIconName`, `isPreviewableFile`, `portalIconSource`, `sidebarIconSource`, `formatSize`, `formatDate`, `itemColor`, `openItem`, `requestThumbnailWarm`, `scheduleVisibleThumbnailWarm`, zoom helpers | `PreviewController`, `LaunchService`, `OpenWithController` |
| recents and favorites | `isRecentPath`, `recentVirtualPath`, `rememberScrollPosition`, `sidebarFavorites`, `sidebarFavoritesRevision`, `visibleDefaultSidebarFavorites`, `pinSidebarFavorite`, `removeSidebarFavorite`, `isSidebarFavorite` | `RecentController`, settings-backed favorites service |
| dialogs and compatibility paths | `dialogActive`, `dialogMode`, `dialogFilePatterns`, `isPortalDialog`, `helperPath`, `networkRootPath`, `trashFilesPath`, `homePath` | native dialog/portal and resolver-root projections |

This boundary prevents the facade from becoming a second monolithic QML state
machine. The legacy singleton and launcher remain available as fallback while
these groups are migrated incrementally.
