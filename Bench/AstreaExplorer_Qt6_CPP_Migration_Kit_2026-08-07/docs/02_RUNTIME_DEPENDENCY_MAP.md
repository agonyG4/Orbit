# Runtime and Dependency Map

## Current Runtime

```text
Quickshell runtime
  |
  +-- Main.qml
  +-- AppState.qml
  |     +-- NavigationState.qml
  |     +-- SelectionState.qml
  |     +-- FileOperationsState.qml
  |     +-- PreviewState.qml
  |     +-- DeviceNetworkState.qml
  |     +-- RecentState.qml
  |
  +-- Quickshell.Io Process nodes
        |
        +-- explorer_backend (Rust)
        +-- explorer_helper.py
        +-- state_json.py
        +-- wallpaper_manager.py
        +-- astrea-launch
        +-- astrea-windows-run
        +-- gio / wl-copy / wl-paste / external tools
```

## Shared Visual Dependencies

Explorer consumes:

```text
AstreaComponents -> Core/components
AstreaFiles      -> Features/Files
AstreaI18n       -> System/i18n
```

Those relationships are product architecture and should remain available to the native application.

`QuickshellComponents -> Quickshell/components` exists only because `OpenWithMenu.qml` currently uses `AppIcon` from the Quickshell component tree. M8 should remove this Explorer-to-Quickshell UI dependency by moving/reusing that component from a neutral shared Qt Quick module without changing its appearance.

## External Integration Dependencies

The current Explorer intentionally interacts with system tools and Astrea services. Native migration does not mean eliminating every subprocess.

Examples that may legitimately remain external:

- `astrea-launch`;
- `astrea-windows-run`;
- the wallpaper manager until it gains a native API;
- `udisksctl` / `lsblk` behind the Rust backend;
- archive tools such as `zip`, `rar`, `tar`, `7z`, or `unzip` behind a backend abstraction;
- `ffmpeg` / ImageMagick behind thumbnail generation where required.

The rule is not “zero subprocesses”. The rule is:

> QML must not own process lifecycle, parse command output, or encode backend policy.

## FileChooser Portal

The FileChooser portal currently launches `Apps/Explorer/PortalDialog.qml` through Quickshell. The native migration is incomplete until the portal launches the native Explorer binary instead.

Target:

```text
xdg-desktop-portal
 -> Astrea FileChooser backend
 -> astrea-explorer --portal
 -> same FileDialog QML
```

The current environment/result compatibility contract must be preserved during the migration.
