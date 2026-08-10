# Current Explorer State Audit

## Reviewed Material

This kit was built from:

- `Explorer.zip`;
- `AstreaOS-Stable(1).zip`;
- the current Explorer application subtree inside AstreaOS;
- the tracked Rust `explorer_backend` crate;
- the current FileChooser portal source;
- the shared QML modules consumed through Explorer's tracked symlinks.

`Explorer.zip` and `src/Apps/Explorer` contain the same 29 regular Explorer files byte-for-byte. The AstreaOS source additionally contains these tracked symlinks:

```text
AstreaComponents       -> ../../Core/components
AstreaFiles            -> ../../Features/Files
AstreaI18n             -> ../../System/i18n
QuickshellComponents   -> ../../Quickshell/components
```

## Current Application Shape

`Main.qml` is already ordinary Qt Quick / Qt Quick Controls. It does **not** require a Quickshell window type. This significantly lowers migration risk.

The Quickshell dependency is concentrated in state/orchestration and process-launch code:

```text
AppState.qml
PortalDialog.qml
state/NavigationState.qml
state/PreviewState.qml
state/RecentState.qml
state/DeviceNetworkState.qml
state/FileOperationsState.qml
components/common/OpenWithMenu.qml
components/common/FileContextMenu.qml
components/layout/Sidebar.qml
components/layout/Toolbar.qml
```

The snapshot contains **37** `Process {}` nodes.

## Central State

`AppState.qml` acts as a singleton facade over:

```text
NavigationState
SelectionState
FileOperationsState
PreviewState
DeviceNetworkState
RecentState
```

The visual QML assumes the `AppState` API heavily. That public QML-facing contract should be preserved during migration rather than rewriting every visual consumer simultaneously.

## Rust Backend

The source crate is located at:

```text
src/Core/bridge/apps/explorer
```

Current top-level CLI commands:

```text
list
search
devices
mount
unmount
remount
warm-thumbnails
install-appimage
file-op
```

The backend owns mature and performance-sensitive functionality including directory enumeration, recursive search, remote-filesystem profiles, device handling, file operations, thumbnail warm-up, and AppImage installation.

It should be preserved and extended, not rewritten in C++.

## Python Helper

`explorer_helper.py` contains 1679 lines and these 19 subcommands:

```text
create-folder
rename
suggest-dirs
which
network-mount-probe
copy-uri-list
scan-conflicts
monitor-dir
trash
restore-trash
empty-trash
paste-image
extract-archive
compress-folder
create-desktop-shortcut
merged-recents
open-with-apps
launch-open-with
set-default-open-with
```

It currently owns a mix of responsibilities that should be divided between C++ and Rust.

## Existing Tests

The reviewed Explorer Python suites contain 69 `unittest` methods. They passed in this artifact environment after restoring the tracked Explorer symlinks to the included shared modules.

The Rust source contains 37 `#[test]` functions. Cargo could not be executed in this artifact environment because `rustc`/`cargo` are not installed here. The migration agent must run the Rust suite in the real development environment before changing behavior.

## Important Stale Documentation Note

One historical Astrea data-flow note still mentions `JsonWorker.js`. The current Explorer does not contain that file; a later changelog explicitly records its removal. Source code is authoritative when existing notes conflict with the snapshot.
