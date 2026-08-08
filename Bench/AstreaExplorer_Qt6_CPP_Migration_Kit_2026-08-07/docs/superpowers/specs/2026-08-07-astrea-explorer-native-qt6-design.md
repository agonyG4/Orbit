# Astrea Explorer Native Qt 6 Migration Design

## Scope and goals

Migrate the Astrea Explorer application from a Quickshell-owned runtime to a
native Qt 6 application while preserving the current visual QML, interaction
contracts, Rust backend commands, and FileChooser portal behavior. The work is
performed in the reviewed source snapshot at
`source/AstreaOS/`; it does not alter the unrelated `Old/` tree or the
pre-existing root index state.

The migration is staged. The existing QML runtime remains available as a
fallback until a native replacement passes its focused tests and the relevant
phase gate. No visual redesign, Rust rewrite, direct C++/Rust FFI, or
permanent Quickshell compatibility clone is in scope.

## Architecture

The native executable is `astrea-explorer`, built with CMake and Qt 6 using
`QGuiApplication` and `QQmlApplicationEngine`. The first native foundation
introduces the executable, a test target, path/settings compatibility, safe
asynchronous process ownership, and a typed backend boundary without changing
the shipping QML behavior.

The target ownership is:

- C++/Qt owns lifecycle, controllers, state, `QAbstractListModel` models,
  clipboard access, filesystem watching, settings, launch orchestration, and
  bounded process/request handling.
- Rust retains the existing Explorer CLI and all filesystem-heavy behavior.
  A persistent `serve --stdio-jsonl` mode is added only after native parity.
- QML remains the presentation layer. The existing `AppState` API is retained
  through a C++ compatibility facade while focused controllers own behavior.

The focused controller boundary consists of `NavigationController`,
`SelectionController`, `FileOperationsController`, `PreviewController`,
`DeviceController`, `RecentController`, `OpenWithController`,
`PortalController`, `DirectoryModel`, `RustBackendClient`,
`ClipboardService`, `DirectoryWatchService`, `SettingsService`, and
`LaunchService`. The facade delegates to these objects and does not become a
second monolithic state object.

## Migration sequence

1. **Baseline and foundation:** record the current source/runtime contracts,
   add CMake/Qt 6 targets, implement testable typed utilities and the native
   application entry point, and keep the old launcher intact.
2. **Directory/navigation vertical slice:** implement `DirectoryModel`,
   one-shot asynchronous Rust CLI calls, navigation generations, search,
   watcher debounce, remote-directory policy, tabs/history, and selection.
3. **State and behavior slices:** migrate devices, recents, previews, launch,
   clipboard, and file operations in separate focused commits. Port helper
   commands to C++ or Rust according to the reviewed ownership matrix, keeping
   `explorer_helper.py` until every production caller is gone.
4. **Native runtime switch:** register the C++ `AppState` facade, preserve the
   visual QML structure, remove Explorer-only Quickshell imports and `Process`
   nodes, and qualify functional/resource/visual parity.
5. **Portal migration:** add `astrea-explorer --portal`, preserve both
   `ASTREA_*` and `BENCH_*` environment contracts and result prefixes, update
   both portal launchers, and enforce atomic exactly-once completion.
6. **Persistent backend and cleanup:** add bounded request-ID JSONL transport,
   crash/restart generation handling, performance measurements, then remove
   obsolete helper/state/launcher code only after zero-caller and qualification
   gates pass.

## Async and compatibility rules

All directory, search, and backend requests carry a generation/request
identity. A late result is ignored if it does not belong to the active
navigation generation. GUI-thread `waitForFinished()` is prohibited for
normal work. `QProcess` calls use argument arrays, bounded output, explicit
lifetime ownership, cancellation where meaningful, and typed errors.

`DirectoryModel` preserves the existing role names:
`fileName`, `filePath`, `fileUrl`, `fileIsDir`, `fileExecutable`,
`fileHidden`, `fileSize`, `fileModified`, `fileKind`, `filePreviewUrl`,
`fileRemote`, `fileMetadataLimited`, and `fileFilesystem`. Model replacement
must not append one JavaScript object per entry on the GUI thread, including
for 10k-entry test data.

Remote/GVFS-like directories retain the current conservative watcher and
thumbnail policy. Clipboard migration uses Qt MIME APIs while preserving
`text/uri-list` and image bytes. Portal result publication is atomic and
finalization is exactly once across accept, reject, close, timeout, and dead
consumer paths.

## Testing and gates

Before behavior changes, run the existing Python Explorer tests, Rust backend
tests, QML structural checks, and relevant lint/security/i18n checks. Each
native slice adds Qt Test coverage before broad QML wiring. Required
regressions include stale A/B navigation results, 10k-entry models, backend
crash/restart, cancellation, remote paths, watcher storms, clipboard MIME
compatibility, archive traversal/rollback, helper zero-callers, and portal
duplicate completion.

Visual parity is qualified with the supplied manifest and matching real-session
screenshots. Resource and performance claims are evidence-based: compare PSS,
idle CPU, threads, FDs, child spawns, startup, directory load, and search
latency for the old runtime, native one-shot CLI, and persistent worker. No
real-session, visual, portal, sanitizer, or performance result is reported
unless it was actually run.

## Commit and scope discipline

Changes are kept in focused commits on the existing `main` checkout. The
pre-existing root staged/deleted state is not reset, cleaned, or included in
these commits. Every implementation slice lists its changed files, tests, and
remaining intentionally untouched fallback code. New documentation is in
English, source changes stay under `source/AstreaOS/src/`, and shared desktop
Quickshell infrastructure is not modified merely because Explorer is being
migrated.
