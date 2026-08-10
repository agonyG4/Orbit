# Implementation Prompt — Migrate Astrea Explorer from Quickshell to Native Qt 6/C++

You are migrating the existing AstreaOS Explorer application from a Quickshell-owned runtime into a native Qt 6 application.

This is an architecture migration, **not a visual redesign**.

## Non-Negotiable Goals

1. Preserve Explorer's current visual appearance and user-visible interaction behavior.
2. Remove Quickshell as an Explorer application runtime dependency.
3. Move application state, models, process ownership, filesystem watching, clipboard integration, and orchestration from QML/JavaScript into C++/Qt.
4. Keep the existing Rust `explorer_backend`; do not rewrite it in C++.
5. Gradually migrate `explorer_helper.py` responsibilities to the correct C++ or Rust owner, then remove the helper only when it has zero production callers.
6. Preserve and migrate the XDG FileChooser portal integration.
7. Keep QML/Qt Quick as the presentation layer.

## Repository Rules

- Work on the actual current `main` branch.
- Inspect the actual current HEAD before editing; the supplied migration kit is a reviewed snapshot/reference, not an instruction to reset newer valid work.
- If current `main` is a clean descendant/evolution of the reviewed snapshot, continue from it.
- Do not create a worktree, detached HEAD, or implementation branch unless the user explicitly changes this rule.
- Do not reset, amend, squash, or rewrite unrelated existing work.
- Repository source changes belong under `src/`, not the installed live runtime, except for explicit real-session qualification.
- All new Markdown documentation must be written in English.
- Keep commits focused and incremental.

## Read First

Read the complete migration kit, especially:

```text
docs/00_EXECUTIVE_DECISION.md
docs/01_CURRENT_STATE_AUDIT.md
docs/03_RUST_BACKEND_CONTRACT.md
docs/04_PYTHON_HELPER_DECOMPOSITION.md
docs/05_TARGET_QT6_CPP_ARCHITECTURE.md
docs/06_VISUAL_PARITY_CONTRACT.md
docs/07_MIGRATION_ROADMAP.md
docs/09_FILECHOOSER_PORTAL_MIGRATION.md
docs/11_TEST_AND_QUALIFICATION_PLAN.md
docs/12_RISK_REGISTER.md
```

Inspect the real current repository afterward and update assumptions when the code has legitimately advanced.

## Current Architecture to Replace

The reviewed snapshot contains approximately:

```text
23 QML files
37 QML Process nodes
526-line AppState.qml
1,085-line FileOperationsState.qml
925-line PreviewState.qml
686-line NavigationState.qml
1,679-line explorer_helper.py
3,609 lines of Rust backend source
```

Do not interpret this as “rewrite everything in C++”.

The required boundary is:

```text
C++ = application behavior/state/models/orchestration
Rust = heavy filesystem/backend operations
QML = presentation/layout/animation/delegates
```

## Architecture Target

Build:

```text
astrea-explorer
  QGuiApplication
  QQmlApplicationEngine
  C++ AppState compatibility facade
  C++ focused controllers/services/models
  RustBackendClient
  existing visual QML
```

Preserve the current `AppState` QML-facing API as much as practical so visual files do not need a broad rewrite.

The C++ `AppState` must be a facade, not a god object. Delegate implementation to focused classes.

Required focused ownership should include equivalents of:

```text
ExplorerApplication
NavigationController
SelectionController
FileOperationsController
PreviewController
DeviceController
RecentController
OpenWithController
PortalController
DirectoryModel : QAbstractListModel
RustBackendClient
ClipboardService
DirectoryWatchService
SettingsService
LaunchService
```

Names may change to match repository conventions, but ownership boundaries must remain clear.

## Preserve the Rust Backend

Current backend source:

```text
src/Core/bridge/apps/explorer
```

Preserve existing commands and behavior:

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

During the early native migration, call the existing CLI asynchronously from C++ so parity can be reached without simultaneously redesigning backend transport.

After native parity is proven, add a backward-compatible persistent mode such as:

```text
explorer_backend serve --stdio-jsonl
```

with bounded request IDs, errors, progress events, cancellation where meaningful, and backend-generation handling.

Do not remove existing CLI commands.

Do not introduce direct C++/Rust FFI in this milestone unless measurements after the persistent worker prove that IPC is a meaningful bottleneck and a separate reviewed design is approved.

## Directory Model Contract

Replace QML `ListModel` population with `QAbstractListModel` while preserving existing role names used by visual QML:

```text
fileName
filePath
fileUrl
fileIsDir
fileExecutable
fileHidden
fileSize
fileModified
fileKind
filePreviewUrl
fileRemote
fileMetadataLimited
fileFilesystem
```

Preserve recent-specific data currently consumed by the recent view.

Large directory population must not create one JavaScript object append operation per item on the GUI thread.

## Async Correctness

Every directory/search/backend operation must have request identity/generation semantics.

Required regression:

```text
navigate to A
navigate to B before A finishes
B finishes
A finishes late
-> UI remains on B
-> A cannot replace B model/state
```

No blocking `waitForFinished()` on the GUI thread for normal operations.

## Remove Quickshell Ownership

Final shipping Explorer QML must contain no:

```text
import Quickshell
import Quickshell.Io
Process {
```

Do not create a permanent clone of `Quickshell.Io.Process` just to make tests pass.

Temporary migration adapters are allowed only while the old runtime remains a fallback and must be removed before final acceptance.

External commands that remain necessary must be owned by typed C++ services using argv arrays and bounded output/lifetime handling.

## Python Helper Migration

Do not delete `explorer_helper.py` up front.

Migrate command by command according to the kit matrix.

Preferred ownership:

### C++ / Qt

```text
suggest-dirs
which
network-mount-probe where appropriate
copy-uri-list
monitor-dir
paste-image
create-desktop-shortcut
merged-recents
```

### Rust backend

```text
create-folder
rename
scan-conflicts
trash
restore-trash
empty-trash
extract-archive
compress-folder
open-with-apps
launch-open-with
set-default-open-with
```

Adjust individual ownership only when the real code provides a stronger existing abstraction, and document the reason.

Archive behavior must preserve all current path traversal, password, conflict, merge, rollback, and error/progress tests before the Python implementation is removed.

## Clipboard

Replace Explorer's `wl-copy` / `wl-paste` dependency with Qt clipboard APIs where Explorer itself owns the clipboard action.

Preserve `text/uri-list` interoperability and image MIME bytes.

Do not regress clipboard interoperability with other Wayland applications.

## Directory Watching

Replace the helper monitor process with C++ `QFileSystemWatcher` or an equally appropriate Qt/native watcher abstraction.

Preserve the current policy that remote/GVFS-like directories do not receive expensive local watcher/thumbnail behavior.

Debounce refreshes and avoid refresh storms.

## Visual Freeze

Do not intentionally change:

```text
window geometry
sidebar width
layout hierarchy
spacing
padding
colors
theme behavior
font metrics
radii
borders
icon sizes
selection/hover appearance
animation timing
preview layout
context menu styling
FileChooser styling
```

Use the supplied visual SHA-256 manifest as a review aid.

Before switching runtime, capture baseline screenshots from the existing Explorer. Compare the native build under the same theme, scale, window geometry, and sample directory.

Any modified visual QML file must have a migration-specific reason.

## Shared Modules

Preserve reuse of:

```text
Core/components
Features/Files
System/i18n
```

Do not fork/copy their controls into Explorer.

Explorer currently also uses `QuickshellComponents` for `AppIcon`. Remove that Explorer-to-Quickshell dependency by moving/reusing the icon component through a neutral shared Qt module if needed, while preserving the exact visual result.

Do not modify the desktop shell's Quickshell runtime merely because Explorer is leaving Quickshell.

## FileChooser Portal

Migration is incomplete until FileChooser uses the native binary.

Implement a native mode such as:

```text
astrea-explorer --portal
```

Preserve current inputs:

```text
ASTREA_FILE_DIALOG_OPTIONS
ASTREA_FILE_DIALOG_RESULT_FILE
```

and compatibility inputs:

```text
BENCH_FILE_DIALOG_OPTIONS
BENCH_FILE_DIALOG_RESULT_FILE
```

Preserve result prefixes:

```text
__ASTREA_FILE_DIALOG__
__BENCH_FILE_DIALOG__
```

Update every current Astrea portal launcher implementation that still starts `PortalDialog.qml` through Quickshell, including both Python/Rust portal paths if both remain production-supported.

Use atomic result-file publication and exactly-once completion semantics.

## Required Phase Gates

### Gate 0 — Baseline

Before production migration:

```bash
python3 -m unittest src/Apps/Explorer/tests/test_explorer_helper.py
python3 src/Apps/Explorer/tests/test_explorer_qml.py
cargo test --locked --manifest-path src/Core/bridge/apps/explorer/Cargo.toml
```

Run relevant repository refactor/i18n/security checks and QML lint.

Capture visual/performance baseline.

### Gate 1 — Native C++ unit foundation

C++ services/models compile and pass tests before wiring broad UI behavior.

### Gate 2 — Navigation/model parity

Native model, list/search, navigation, tabs, selection, watcher, remote listing behavior pass deterministic and real-session tests.

### Gate 3 — Operations parity

File operations, devices, previews, recents, clipboard, launch behavior and migrated helper responsibilities pass.

### Gate 4 — Native Explorer switch

`astrea-explorer` runs the unchanged visual product successfully with no Explorer Quickshell imports/process nodes.

Do not delete fallback files yet.

### Gate 5 — Portal

Real FileChooser clients use `astrea-explorer --portal` successfully.

### Gate 6 — Helper removal

`explorer_helper.py` has zero runtime callers and all behavior tests have moved to C++/Rust equivalents.

### Gate 7 — Persistent Rust worker

One-shot CLI remains compatible while native Explorer uses the persistent transport successfully.

### Gate 8 — Final cleanup and performance qualification

Only now remove obsolete Quickshell Explorer launch paths/state files/transitional adapters.

## Testing Requirements

Add Qt Test suites for controllers/models/services and expand Rust tests for migrated helper behavior.

Preserve existing regressions and add tests for:

- 10k-entry model behavior;
- stale list/search results;
- backend crash/restart;
- request cancellation;
- rapid navigation;
- remote path behavior;
- clipboard MIME compatibility;
- file watcher storms;
- archive traversal and rollback;
- portal duplicate completion;
- helper zero-caller final assertion;
- shipping Explorer source contains zero `Quickshell` imports and zero `Process {}` nodes.

Run real workflows listed in the migration kit's qualification plan.

## Performance Qualification

Measure current Quickshell Explorer vs:

1. native Qt + one-shot Rust CLI;
2. native Qt + persistent Rust worker.

Measure combined PSS using `/proc/<pid>/smaps_rollup`, idle CPU, threads, FDs, child spawn count, startup latency, directory-load latency, and search latency.

Do not claim a percentage improvement without measurements.

## Commit Strategy

Use focused commits. A reasonable sequence is:

```text
build(explorer): add native Qt 6 application foundation
feat(explorer): add typed directory model and navigation core
feat(explorer): migrate selection and persisted state to C++
feat(explorer): migrate devices recents and preview orchestration
feat(explorer): migrate file operation workflow to C++
feat(explorer): move helper filesystem operations into Rust
feat(explorer): remove Explorer Quickshell runtime dependency
feat(portal): launch native Astrea Explorer file chooser
feat(explorer): add persistent Rust backend transport
test(explorer): qualify native runtime and visual parity
chore(explorer): remove obsolete Python and Quickshell bridges
```

Do not amend prior commits.

## Completion Report

Return:

- starting and final HEAD;
- commit list;
- exact files created/removed/modified;
- remaining QML file count;
- remaining Explorer `Quickshell` imports (must be zero in shipping runtime);
- remaining Explorer `Process {}` nodes (must be zero in shipping runtime);
- status of `explorer_helper.py` (must have zero runtime callers before removal);
- Rust CLI compatibility status;
- persistent backend protocol status;
- Python/C++/Rust test totals;
- QML lint results;
- visual parity qualification results;
- FileChooser portal qualification;
- current-vs-native PSS/CPU/spawn/latency measurements;
- final `git status --short`.

Do not claim real-session, visual, sanitizer, performance, or portal qualification unless it was actually run and observed.
