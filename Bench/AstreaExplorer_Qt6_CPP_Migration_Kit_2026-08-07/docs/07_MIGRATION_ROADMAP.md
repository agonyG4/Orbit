# Phased Migration Roadmap

The migration should be incremental and gated. Do not rewrite Explorer in one commit.

## Phase 0 — Freeze and Qualify the Baseline

- confirm current `main` and clean tracked worktree;
- record starting commit;
- run existing Python Explorer tests;
- run `cargo test --locked` for the Rust backend;
- run relevant `qmllint` checks;
- run current Quickshell Explorer smoke;
- capture the visual parity screenshots;
- record startup time, idle PSS, process count, directory-load latency, and search latency;
- preserve the current working Explorer as fallback during the migration.

No production behavior changes in this phase.

## Phase 1 — Create the Native Qt 6 Build Foundation

Add native build infrastructure and unit-test targets without replacing the shipping runtime yet.

Create a small C++ application skeleton and typed utility/services:

- environment/path resolver;
- settings compatibility;
- safe process runner;
- backend request abstraction;
- logging/error types.

Use CMake + Qt 6 and follow the repository's current Qt conventions where available.

Do not redesign or relocate visual QML merely to make the CMake tree prettier.

## Phase 2 — Model and Navigation Core

Implement:

- `DirectoryModel : QAbstractListModel`;
- `NavigationController`;
- request generations/cancellation;
- local directory watcher;
- search integration;
- remote-directory policy;
- tab/history state;
- `SelectionController`.

Initially call the existing Rust CLI through a C++ `RustBackendClient` so backend behavior stays unchanged.

Gate: list/search/tab/selection behavior matches the current Explorer and large directories remain responsive.

## Phase 3 — Device, Recent, Preview, and Launch State

Implement typed C++ controllers for:

- devices/network state;
- recent items;
- preview scheduling;
- open-file/launch dispatch;
- persisted settings;
- thumbnail warm scheduling.

Remove the corresponding Quickshell `Process` ownership from state QML.

Gate: behavior parity and no new visual changes.

## Phase 4 — File Operations and Python Helper Migration

Move the helper responsibilities according to `04_PYTHON_HELPER_DECOMPOSITION.md`.

Priority order:

1. low-risk C++ replacements (`which`, clipboard, watcher, suggestions);
2. Rust folder/rename/conflict operations;
3. trash restore/empty;
4. Open With;
5. archive extraction/compression last because it has the largest security/rollback contract.

Keep `explorer_helper.py` available until zero production callers remain.

Gate: every migrated command has equivalent tests and real workflow qualification.

## Phase 5 — Replace the QML `AppState` Implementation

Register a C++ `AppState` facade with the same QML-facing properties and functions used by the existing presentation.

Remove state QML modules only after the equivalent C++ controller is wired and tested.

The goal is not to change every visual callsite. Preserve the `AppState` API wherever practical.

Gate: no visual component needs to understand Rust/backend/process details.

## Phase 6 — Switch the Main Explorer Runtime

Install and launch:

```text
astrea-explorer
```

using native Qt 6:

```text
QGuiApplication
QQmlApplicationEngine
```

Remove all remaining `import Quickshell` / `import Quickshell.Io` dependencies from the normal Explorer runtime.

The old Quickshell entry remains available only as a temporary fallback during qualification, then is removed from normal launch configuration.

Gate: visual, functional, startup, and resource qualification pass.

## Phase 7 — Migrate the FileChooser Portal

Add:

```text
astrea-explorer --portal
```

or an equivalent explicit mode.

Preserve:

```text
ASTREA_FILE_DIALOG_OPTIONS
ASTREA_FILE_DIALOG_RESULT_FILE
BENCH_FILE_DIALOG_OPTIONS (compatibility)
BENCH_FILE_DIALOG_RESULT_FILE (compatibility)
__ASTREA_FILE_DIALOG__ result prefix
__BENCH_FILE_DIALOG__ compatibility prefix
```

Update the portal backends to launch the native binary rather than `qs -p PortalDialog.qml`.

Gate: real XDG FileChooser open/save/folder/multiple-selection behavior passes.

## Phase 8 — Persistent Rust Worker

Only after native parity is stable, add the persistent backend mode.

Switch C++ from one-shot Rust CLI calls to one worker process with request IDs.

Retain the CLI commands.

Gate: no behavior regression, lower spawn count/latency, clean backend crash/restart behavior.

## Phase 9 — Remove Transitional Code

Remove only after zero production callers remain:

- `explorer_helper.py`;
- Quickshell Explorer launcher path;
- Explorer-only Quickshell process compatibility;
- obsolete QML state files;
- obsolete state JSON subprocesses;
- obsolete `QuickshellComponents` alias if Explorer no longer consumes it.

Do not remove shared Quickshell infrastructure used by the desktop shell itself.

## Phase 10 — Performance and Final Qualification

Compare old and new builds using the plan in `13_PERFORMANCE_PLAN.md`.

Commit the final architecture documentation and update Astrea agent notes to make the native Explorer the canonical runtime.
