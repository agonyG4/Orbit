# File-by-File Migration Map

## `Main.qml`

Keep presentation nearly unchanged.

Expected changes should be limited to module/runtime integration if required. `Main.qml` already uses standard Qt Quick `ApplicationWindow`, so it is not the reason Explorer depends on Quickshell.

## `AppState.qml`

Current role: giant QML facade over state objects and environment paths.

Target: C++ `AppState` singleton facade.

Do not port its implementation line-for-line. Preserve its public API and delegate to focused C++ controllers.

## `state/NavigationState.qml`

Target: `NavigationController` + `DirectoryModel` + `DirectoryWatchService`.

Critical contracts:

- request ordering;
- stale result suppression;
- search;
- remote directory policy;
- history/tabs;
- breadcrumb behavior;
- loading/error state.

## `state/SelectionState.qml`

Target: `SelectionController`.

Keep range/multi-selection behavior and existing names visible to delegates.

## `state/FileOperationsState.qml`

Target: `FileOperationsController` plus Rust backend operations.

This is the highest-risk state migration. Preserve progress, conflict, archive password/conflict, trash, clipboard, AppImage, and wallpaper flow.

## `state/PreviewState.qml`

Target: `PreviewController` for scheduling/process behavior.

Keep purely visual/icon helper logic in QML when it is presentation-specific. Do not migrate harmless visual functions just to maximize C++ line count.

## `state/DeviceNetworkState.qml`

Target: `DeviceController` and typed backend responses.

Keep Rust device/mount code.

## `state/RecentState.qml`

Target: `RecentController` / `RecentModel`.

Eliminate `state_json.py` subprocess use for Explorer state. Use Qt JSON and safe atomic writes.

## `components/common/OpenWithMenu.qml`

Keep layout and styling.

Replace helper processes with `OpenWithController` calls. Move `AppIcon` dependency out of `QuickshellComponents` into a neutral shared module only if required to eliminate the Quickshell import; preserve exact appearance.

## `components/common/FileContextMenu.qml`

Keep menu visual structure and `AstreaFiles.FileContextMenu` wrapper.

Replace create/rename/tool-detection process calls with typed controller APIs.

## `components/layout/Sidebar.qml`

Keep visual structure.

Replace property/shortcut helper subprocesses with C++ services. Do not redesign favorites/devices/network presentation.

## `components/layout/Toolbar.qml`

Keep visual structure.

Replace directory suggestion helper process with `NavigationController` completion results.

## `PortalDialog.qml`

Keep visible dialog UI.

Replace Quickshell env/process behavior with a C++ `PortalController` and native application mode.

## `FileDialog.qml`

Preserve visual behavior. Adapt model API only through compatibility roles/methods.

## `explorer_helper.py`

Delete only after the command-by-command migration matrix is complete and zero runtime callers remain.

## Rust Backend Files

Preserve existing module ownership:

```text
entries.rs
file_ops.rs
devices.rs
thumbnails.rs
appimage.rs
json.rs
```

Prefer adding focused modules for migrated Python responsibilities rather than growing `main.rs` or `file_ops.rs` indefinitely, for example:

```text
archive.rs
trash.rs
open_with.rs
server.rs
protocol.rs
```
