# Transitional Explorer QML State

This inventory is intentionally descriptive. Phase 3 keeps the existing QML
state and fallback paths in place until a native replacement has behavior
parity. The native executable now owns the core navigation, directory model,
selection, settings, backend transport, and directory watcher graph; the
remaining entries below are not silently removed or reimplemented as a second
state machine.

## Migration categories

| Candidate | Current responsibility | Native replacement / owner | Phase 3 status |
| --- | --- | --- | --- |
| `state/NavigationState.qml` (`Process` at lines 561, 599, 655) | directory listing, search, and helper-based directory watching | `NavigationController`, `DirectoryModel`, `DirectoryWatchService`, `RustBackendClient` | Native core wired; legacy QML retained for fallback |
| `state/SelectionState.qml` | selected names, range selection, and model refresh reconciliation | `SelectionController` | Native core wired; QML state retained |
| `state/FileOperationsState.qml` (`Process` at lines 742, 795, 865, 926, 965, 1009, 1024, 1037, 1049, 1064, 1078) | copy/move, archive, clipboard probing, delete/restore/trash, AppImage, wallpaper | `FileOperationsController`, `FileOperationService`, `ClipboardService`; archive/trash/AppImage/wallpaper parity still requires service coverage | Transitional; do not remove |
| `state/DeviceNetworkState.qml` (`Process` at lines 194, 207, 227, 251) | network probing/mount and device operations | `DeviceController` for typed backend device operations; network portal behavior remains QML/desktop integration | Transitional; native controller exists but is not yet the sole QML owner |
| `state/PreviewState.qml` (`Process` at lines 767, 837, 870, 879, 888, 916) | thumbnail warming, shell/executable/Windows launching, preview refresh | `PreviewController`, `LaunchService`, `OpenWithController` | Transitional; preview/launch parity gate remains open |
| `state/RecentState.qml` (`Process` at lines 145, 182) | recent item load/save through the helper | `RecentController` plus a future native persistence service | Transitional; legacy helper retained |
| `PortalDialog.qml` (`Process` at line 77) | portal dialog fallback and result delivery | `PortalController` | Transitional; legacy portal entry remains available |
| `components/common/FileContextMenu.qml` (`Process` at lines 386, 623, 783, 792) | archive capability probe, file properties, create-folder, rename | file-operation/filesystem service coverage to be added incrementally | Transitional; behavior retained |
| `components/common/OpenWithMenu.qml` (`Process` at lines 350, 390, 396) | application discovery, default association, launch | `OpenWithController`, `LaunchService` | Transitional; native controller is available but QML still owns the menu flow |
| `components/layout/Sidebar.qml` (`Process` at lines 441, 658) | desktop shortcut and filesystem property actions | filesystem/shortcut service | Transitional; helper path is resolver-rooted |
| `components/layout/Toolbar.qml` (`Process` at line 954) | search suggestions | navigation/search suggestion service | Transitional; no native parity owner yet |
| `AppState.qml` | aggregate aliases, settings persistence, sidebar favorites, startup sequencing | `AppStateFacade`, `SettingsService`, focused controllers | Core properties are projected; legacy-only aliases and startup timers remain documented |

## Quickshell I/O and backend command sites

The unchanged UI imports `Quickshell.Io` from the state files, portal dialog,
toolbar, sidebar, and common menus. It also constructs commands for the
resolver-rooted Explorer backend, `explorer_helper.py`, `astrea-launch`, and
the Windows runner. These are the exact boundaries to migrate when equivalent
native service behavior is proven. The native loader does not delete or rewrite
these imports in Phase 3.

The current development checkout does not contain an installed
`quickshell-ioplugin`; a direct `qmlscene` load therefore reports:

```text
module "Quickshell.Io" plugin "quickshell-ioplugin" not found
```

That environment dependency is recorded as a validation blocker, not treated
as a successful native QML parity result.

## Synchronization timers

Timers in `AppState.qml`, `NavigationState.qml`, `FileOperationsState.qml`,
`DeviceNetworkState.qml`, `PreviewState.qml`, `Main.qml`, and the visual file
views serve either startup sequencing, debounce/coalescing, thumbnail warming,
or temporary process synchronization. Visual debounce and animation timers
remain QML-owned. Backend synchronization timers are migration candidates for
the corresponding C++ service/controller and must not be removed until signal
and cancellation behavior matches.

## Next migration order

1. Complete native QML runtime dependency packaging (`Quickshell.Io`) and run
   the real `Main.qml` smoke test in the supported runtime.
2. Expose the already-typed `FileOperationsController` and `DeviceController`
   through a compatibility projection after operation parity tests exist.
3. Move recent/preview/launch and portal flows behind their existing native
   owners, one responsibility at a time.
4. Retire each legacy `Process {}` only after a focused parity test covers
   success, failure, cancellation, and model/state refresh behavior.
