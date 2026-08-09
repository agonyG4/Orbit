# Transitional Explorer QML State

This inventory is intentionally descriptive. Phase 3 keeps the existing QML
state and fallback paths in place until a native replacement has behavior
parity. The native executable now owns the core navigation, directory model,
selection, settings, backend transport, and directory watcher graph; the
remaining entries below are not silently removed or reimplemented as a second
state machine. Copy/cut state and system clipboard publication are also native;
paste and the remaining file-operation flows stay transitional.

## Migration categories

| Candidate | Current responsibility | Native replacement / owner | Phase 3 status |
| --- | --- | --- | --- |
| `state/NavigationState.qml` (`Process` at lines 561, 599, 655) | legacy directory listing, search, and helper-based directory watching | `NavigationController`, `DirectoryModel`, `DirectoryWatchService`, `RustBackendClient` | `NATIVE_AUTHORITATIVE`; retained as guarded transitional reference |
| `state/SelectionState.qml` | legacy selected names, range selection, and model refresh reconciliation | `SelectionController` | `NATIVE_AUTHORITATIVE`; QML state retained as transitional reference |
| `state/FileOperationsState.qml` (`Process` at lines 742, 795, 865, 926, 965, 1009, 1024, 1037, 1049, 1064, 1078) | paste/conflicts, archive, clipboard probing, delete/restore/trash, AppImage, wallpaper | `FileOperationsController`, `FileOperationService`, `ClipboardService` | Copy/cut and system clipboard are `NATIVE_AUTHORITATIVE`; paste and remaining flows are `LEGACY_TRANSITIONAL` |
| `state/DeviceNetworkState.qml` (`Process` at lines 194, 207, 227, 251) | network probing/mount and device operations | `DeviceController` for typed backend device operations; network portal behavior remains QML/desktop integration | `LEGACY_TRANSITIONAL`; native controller is not yet the sole QML owner |
| `state/PreviewState.qml` (`Process` at lines 767, 837, 870, 879, 888, 916) | thumbnail warming, shell/executable/Windows launching, preview refresh | `PreviewController`, `LaunchService`, `OpenWithController` | `LEGACY_TRANSITIONAL`; preview/launch parity gate remains open |
| `state/RecentState.qml` (`Process` at lines 145, 182) | recent item load/save through the helper | `RecentController` plus a future native persistence service | `LEGACY_TRANSITIONAL`; legacy helper retained |
| `PortalDialog.qml` (`Process` at line 77) | portal dialog fallback and result delivery | `PortalController` | `LEGACY_TRANSITIONAL`; legacy portal entry remains available |
| `components/common/FileContextMenu.qml` (`Process` at lines 386, 623, 783, 792) | archive capability probe, file properties, create-folder, rename | file-operation/filesystem service coverage to be added incrementally | `LEGACY_TRANSITIONAL`; behavior retained |
| `components/common/OpenWithMenu.qml` (`Process` at lines 350, 390, 396) | application discovery, default association, launch | `OpenWithController`, `LaunchService` | `LEGACY_TRANSITIONAL`; native controller is available but QML still owns the menu flow |
| `components/layout/Sidebar.qml` (`Process` at lines 441, 658) | desktop shortcut and filesystem property actions | filesystem/shortcut service | `LEGACY_TRANSITIONAL`; helper path is resolver-rooted |
| `components/layout/Toolbar.qml` (`Process` at line 954) | search suggestions | navigation/search suggestion service | `NOT_YET_MIGRATED`; no native parity owner yet |
| `AppState.qml` | public compatibility contract, native projections, and legacy-only startup/operation flows | `AppStateFacade`, `SettingsService`, focused controllers | `NATIVE_AUTHORITATIVE` for delegated domains; legacy-only aliases and timers remain transitional |

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
