# Test and Qualification Plan

## Testing Philosophy

Every phase must preserve behavior before deleting the old implementation.

The migration should increase testability by moving policy from QML process callbacks into typed C++ and Rust units.

## Existing Baseline

Run first:

```bash
python3 -m unittest src/Apps/Explorer/tests/test_explorer_helper.py
python3 src/Apps/Explorer/tests/test_explorer_qml.py
cargo test --locked --manifest-path src/Core/bridge/apps/explorer/Cargo.toml
```

Also run repository i18n/security/refactor checks relevant to Explorer.

## C++ Tests

Add Qt Test coverage for at least:

### Directory Model

- role names are exactly compatible;
- empty directory;
- 10k+ synthetic entries without QML object explosion;
- model reset on new generation;
- stale generation ignored;
- sort change;
- preview role update;
- recent-only roles.

### Navigation

- back/forward;
- tabs;
- tab close/reorder state;
- search activation/cancel;
- list request A completes after request B and is ignored;
- remote directory disables local watcher/thumbnail warm;
- watcher debounce;
- missing/inaccessible directory.

### Selection

- single selection;
- Ctrl multi-select;
- Shift range;
- select all;
- selection cleared/reconciled after model replacement;
- selected file removed during refresh.

### File Operations

- copy/cut/paste workflow;
- conflict policies;
- progress events;
- backend crash during operation;
- cancel;
- trash/restore/empty;
- archive password/conflict state;
- AppImage result;
- clipboard URI interoperability.

### Backend Client

- one-shot mode parity first;
- stdout/stderr caps;
- invalid JSON;
- timeout;
- process crash;
- request generations;
- later persistent worker: interleaved progress, request IDs, malformed messages, reconnect, outstanding-request failure exactly once.

### Portal

- open/save/select-folder;
- multiple selection;
- cancellation;
- malformed options;
- duplicate completion race;
- result-file publication.

## Rust Tests

Port Python helper tests to Rust before deleting helper commands.

Pay special attention to:

- path traversal rejection;
- broken symlink conflict behavior;
- trash metadata;
- archive absolute/`..` entry rejection;
- overwrite rollback;
- merge failure preservation;
- password handling;
- archive tool absence;
- Open With desktop entry parsing;
- no-shell launch invocation.

## QML Tests

Keep structural tests for visual contracts and add native QML integration tests where deterministic.

Test:

- Main QML loads under native `QQmlApplicationEngine`;
- all required roles exist;
- context menus open;
- dialogs bind to C++ state;
- no `Quickshell` import remains in shipping Explorer QML;
- no `Process {}` remains in shipping Explorer QML;
- exact view/drag behaviors from existing regression tests remain.

## Real Session Qualification

Perform on the real Astrea/Typhon session:

- startup from application launcher;
- open large local directory;
- open network/GVFS location;
- tabs and history;
- list/icon mode;
- all zoom presets;
- drag/drop internal and external;
- clipboard copy/cut/paste;
- image clipboard paste;
- trash/restore/empty;
- archive extract/compress;
- password-protected archive;
- Open With/default app;
- AppImage install;
- executable/shell script/Windows executable launch;
- device mount/unmount/remount;
- recents;
- FileChooser portal from at least one real application.

No phase is complete based only on static tests if it changes a real desktop integration path.
