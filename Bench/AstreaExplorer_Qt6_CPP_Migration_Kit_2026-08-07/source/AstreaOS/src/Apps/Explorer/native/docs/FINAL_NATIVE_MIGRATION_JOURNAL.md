# Astrea Explorer Final Native Migration Journal

## Scope

The native Qt 6/C++ executable is the canonical Explorer runtime. The existing
QML presentation remains intact behind `AppState.qml`; QML owns presentation
and visual interaction, C++ owns application state and workflow, and Rust owns
filesystem-heavy and security-sensitive work.

## Starting audit — 2026-08-11

Starting commit: `9f9ca49` (`feat(explorer): migrate Recent storage and loading to native C++`). The production QML audit reported 37 `Process {}` nodes and 15 Quickshell import lines. The portal still launched `/usr/bin/qs`, the system launcher used a repository-specific Quickshell path, and production helper callers covered filesystem actions, trash, archives, clipboard image paste, Open With, and portal fallback.

## Final ownership

| Domain | Final owner |
| --- | --- |
| Application lifecycle, QML engine, portal mode | `ExplorerApplication` / `PortalController` |
| Public compatibility boundary | `AppState.qml` -> `NativeAppState` -> `AppStateFacade` |
| Navigation, history, tabs, search, selection, settings, watchers | Native C++ controllers/services and `DirectoryModel` |
| Recent projection, loading, recording, persistence | `RecentController` / `RecentStore` |
| File operations and conflict state | `FileOperationsController` / `FileOperationService` / Rust `file-op` |
| Filesystem utilities, trash, archives, thumbnails, AppImage | C++ `FilesystemService` -> Rust backend |
| Clipboard and image paste | C++ `ClipboardService` / Qt `QClipboard` and `QSaveFile` |
| Translation catalogs | Native bootstrap JSON loading -> QML `AstreaI18n.I18n` presentation facade |
| Open With and default MIME application | Asynchronous C++ desktop catalog / `LaunchService` |
| Ordinary launch and selected-app launch | Typed C++ `LaunchService` |
| Devices and network mounts | `DeviceController` plus typed filesystem/network operations |
| Wallpaper | Typed C++ `WallpaperService` with explicit argv |
| Backend IPC | Persistent JSONL Rust worker; one-shot CLI retained for diagnostics/tests |
| Presentation, delegates, layout, animation, transient visual state | QML |

The inert `LegacyAppStateAdapter.qml` remains only as a test/compatibility
shape for loading the public QML contract without native registration. Native
production startup always registers the facade and uses the native adapter; it
does not provide a Quickshell or Python fallback.

## Architecture decisions and safety

- Rust utility commands return structured JSON with stable operation names and
  error codes. Destructive work stays out of QML.
- Archive extraction validates member paths, rejects absolute/parent escapes,
  rejects link members, extracts into a staging directory, publishes only after
  success, and applies explicit keep-both/rename/overwrite policies.
- Trash writes freedesktop `.trashinfo` files with encoded original paths and
  UTC deletion dates; restore handles missing metadata and destination
  collisions without overwriting an existing target.
- Open With builds one desktop-entry catalog asynchronously, filters cached
  MIME associations, applies the XDG default application, and sorts
  deterministically. Late catalog generations are ignored.
- The persistent worker is versioned JSONL with request IDs, bounded request
  and response/child-stream sizes, timeout/crash failure mapping, worker-side
  child termination for cancellation, and deterministic shutdown cleanup.
- Portal results are written atomically through `QSaveFile`; the portal keeps
  its compatibility result prefixes for stdout fallback while using the result
  file in native mode.
- Shared Astrea components were audited through their symlinked runtime paths.
  Theme, navigation, notification compatibility, and translation components
  no longer import Quickshell or launch QML processes; the old unused
  Explorer-only QuickshellComponents package is excluded from installation.

## Production packaging closure — 2026-08-14

- CMake now owns the required Rust backend, portal, and source-owned
  `astrea-launch` builds, installs them under the canonical Astrea runtime root,
  and fails configuration/build when a required component is absent.
  The service installer validates those installed executables and uses
  `RequiresMountsFor` for the runtime root.
- Runtime resolution reports resource-root, backend, launcher, and portal
  capabilities explicitly. Normal startup requires the backend and launch
  provider; portal startup requires the backend and never falls back to a
  checkout `target/*` binary.
- Desktop-entry identity is derived from canonical XDG application roots,
  including nested desktop files. MIME defaults and added associations use a
  literal-section freedesktop parser with `QLockFile` and `QSaveFile`.
- Persistent worker startup is asynchronous and queues requests while the
  process starts. The portal uses Tokio async child I/O, bounded stdout/stderr
  and result-file reads, per-request `Request.Close` cancellation, and a
  bounded dialog semaphore.
- Validation completed with 25/25 Debug CTest, 25/25 Release CTest, 41/41 Rust
  backend tests, 9/9 portal tests, the strengthened static source gate, and a
  fresh-prefix clean-install harness covering normal and portal self-tests.

## Validation record

The final qualification report is in
`native/docs/FINAL_NATIVE_MIGRATION_QUALIFICATION.md`. The final source audit
must be run with `python3 scripts/verify_native_migration_gate.py`; the QML
dependency audit must report zero process nodes and zero Quickshell imports.

## Final source closure — 2026-08-14

- Orbit history contained the restored `src/bin/astrea-launch` ELF but no
  provider source. The matching source-owned Rust provider was recovered from
  the deployed Astrea source tree at `src/System/launch`, brought into the
  checkout with its manifest, lockfile, implementation, and tests, and wired
  into CMake. The ELF was removed; no production target or gate depends on a
  precompiled launch artifact.
- The provider preserves the observed `LaunchRequest` contract: desktop,
  file, URL, Steam, shell-command, JSON argv, daemon/launchd, doctor, and
  history forms. Launch history remains bounded JSONL under XDG state, and
  daemon requests use a private user Unix socket with safe argv transport.
- XDG behavior is now represented by injectable `XdgPaths`: config/data
  homes, config/data search dirs, current desktop, and HOME all have explicit
  environment-backed defaults. Nested desktop IDs canonicalize `/` to `-`,
  outside-root files do not receive fabricated IDs, and user application roots
  win over system roots.
- MimeApps resolution covers desktop-specific and generic files across the
  user/system XDG locations, applies default/added/removed association
  semantics, validates candidates against application entries, falls back to
  valid associations, and writes only the user config file with lock/re-read
  and atomic replacement.
- Runtime resolution order is explicit `ASTREA_ROOT`, installed
  `<prefix>/share/Astrea` from `<prefix>/bin/astrea-explorer`, user install,
  then development traversal. The clean-install harness isolates HOME,
  XDG config/data/state/cache/runtime directories and performs installed smoke
  without forcing ASTREA_ROOT.
- Portal process cancellation, timeout, and error paths explicitly terminate
  and reap children. An injectable runner is exercised through a private
  `dbus-run-session`: a known request handle is exported, `Request.Close`
  cancels it and removes the object, the service handles a subsequent request,
  and normal completion returns a file URI.
- `/home/agony/GitHub/zip_astrea_explorer_migration_kit.py` now uses
  path-aware generated-directory pruning, retains source `bin`/`obj`/`out`,
  preserves executable modes and internal symlinks, rejects unsafe/dangling
  symlinks, validates compiled-payload exclusion and required source closure,
  and performs atomic deterministic archive generation. Synthetic ZIP tests
  pass 3/3.
- Final evidence: Debug CTest 25/25, Release CTest 25/25, Explorer Rust
  backend 41/41, launch Rust 13/13, portal Rust 9/9, Explorer Python/QML
  20/20, the migration gate PASS, service `bash -n` PASS, and the canonical
  archive 287-entry verification PASS. Fresh extraction then passed the source
  gate and isolated clean-install build/self-test. This is the first point at
  which checkout-to-source-ZIP-to-extraction reproducibility is qualified.
