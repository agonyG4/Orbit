# Astrea Explorer Native Qt 6 Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Explorer’s Quickshell-owned runtime with a native Qt 6/C++ application while preserving the existing visual QML contract, Rust CLI behavior, and FileChooser portal integration.

**Architecture:** Controllers depend on the typed `IRustBackendClient` interface. `RustBackendClient` owns protocol translation and delegates transport to an injected `BackendTransport`; the first transport is asynchronous one-shot CLI execution and the later transport is persistent JSONL. QML sees only the C++ compatibility facade and models, never processes, transports, filesystem helpers, or Rust protocol data.

**Tech Stack:** CMake, Ninja, Qt 6.11 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Test`), C++17 or the repository’s available C++ standard, existing Rust `explorer_backend`, existing QML/Qt Quick presentation, Python portal compatibility code, and Cargo tests.

## Global Constraints

- Work only in `/home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS` and its migration-kit documentation; do not modify `/home/agony/GitHub/Orbit/Old` or the unrelated pre-existing root staged/deleted state.
- Stay on the existing `main` checkout; do not create a branch, worktree, detached HEAD, reset, amend, squash, or rewrite unrelated work.
- Preserve the current visual QML contract exactly; every changed visual QML line must be required for runtime wiring and must not change geometry, styling, animation, spacing, typography, icon sizing, or layout hierarchy.
- Enforce the dependency direction: `QML -> compatibility facade/models -> focused controllers -> services -> backend transport -> Rust backend`.
- QML must not call `RustBackendClient`, `QProcess`, filesystem helpers, or a Rust worker directly. Controllers must not manipulate QML objects. Backend transports must not own presentation state.
- Controllers depend on typed `IRustBackendClient`; they never construct `QProcess`, parse JSONL, or depend on a transport implementation.
- Keep the existing Rust CLI commands and behavior. Do not introduce C++/Rust FFI in this migration.
- Keep `explorer_helper.py` and the legacy launcher paths until their production caller counts are zero and replacement tests plus real-session checks pass.
- The native production launcher cannot switch until deterministic shadow-parity tests pass for directory entries/order, navigation, search, selection, previews, device state, recents, and file-operation outcomes.
- Do not claim visual, portal, real-session, sanitizer, or performance qualification unless it was actually run and observed.
- Use focused commits; do not commit generated build output, `target/`, `build/`, caches, secrets, or environment files.

---

## File and module map

The native project is self-contained under `source/AstreaOS/src/Apps/Explorer/native/` because the snapshot has no existing CMake build. Files are split so the public typed API, transport implementations, controllers, services, and tests can evolve independently.

### Native build and application

- Create `src/Apps/Explorer/native/CMakeLists.txt` — Qt targets, install layout, QML module, and CTest registrations.
- Create `src/Apps/Explorer/native/src/main.cpp` — `QGuiApplication`, application identity, command-line mode dispatch, and `QQmlApplicationEngine` startup.
- Create `src/Apps/Explorer/native/src/explorer_application.h/.cpp` — lifecycle and dependency composition; no visual state ownership.
- Create `src/Apps/Explorer/native/qml/NativeBootstrap.qml` — foundation smoke surface only; the existing `Main.qml` remains the visual source of truth until the facade is ready.

### Typed backend boundary

- Create `src/Apps/Explorer/native/src/backend/backend_types.h` — request/value/error types and role-compatible directory entry data.
- Create `src/Apps/Explorer/native/src/backend/backend_transport.h` — transport-only asynchronous wire boundary.
- Create `src/Apps/Explorer/native/src/backend/one_shot_cli_transport.h/.cpp` — bounded `QProcess` ownership and argv-based CLI execution.
- Create `src/Apps/Explorer/native/src/backend/rust_backend_client.h/.cpp` — typed `IRustBackendClient` implementation, request identity, decoding, cancellation, and transport-independent signals.
- Create `src/Apps/Explorer/native/src/backend/fake_backend_client.h/.cpp` — deterministic test double; production code never uses it.
- Later create `src/Apps/Explorer/native/src/backend/persistent_jsonl_transport.h/.cpp` and `src/Core/bridge/apps/explorer/src/{protocol,server}.rs` — persistent transport/protocol without changing controller interfaces.

### Controllers and services

- Create `src/Apps/Explorer/native/src/models/directory_model.h/.cpp` — `QAbstractListModel` role-compatible directory data.
- Create `src/Apps/Explorer/native/src/controllers/navigation_controller.h/.cpp` — path, history, tabs, search, request generations, and watcher policy.
- Create `src/Apps/Explorer/native/src/controllers/selection_controller.h/.cpp` — single, range, multi-select, and model reconciliation.
- Create `src/Apps/Explorer/native/src/controllers/app_state_facade.h/.cpp` — narrow QML-compatible facade delegating to focused objects.
- Create `src/Apps/Explorer/native/src/services/directory_watch_service.h/.cpp` — local watcher and bounded debounce.
- Create `src/Apps/Explorer/native/src/services/settings_service.h/.cpp` — compatibility settings keys/locations and atomic writes.
- Create `src/Apps/Explorer/native/src/services/clipboard_service.h/.cpp` — Qt MIME clipboard ownership.
- Create `src/Apps/Explorer/native/src/services/launch_service.h/.cpp` — argv-safe application/file launch.
- Later create focused device, recent, preview, open-with, file-operation, and portal controllers/services in the same directories; do not grow `AppState` into a god object.

### Tests and parity fixtures

- Create `src/Apps/Explorer/native/tests/CMakeLists.txt` — Qt Test targets and CTest registration.
- Create `src/Apps/Explorer/native/tests/tst_backend_client.cpp` — transport abstraction, typed decoding, generation, timeout, output cap, and cancellation tests.
- Create `src/Apps/Explorer/native/tests/tst_directory_model.cpp` — roles, 10k entries, reset, sort, preview updates, and recent roles.
- Create `src/Apps/Explorer/native/tests/tst_navigation_controller.cpp` — navigation ordering, search, tabs/history, watcher policy, and stale-result rejection.
- Create `src/Apps/Explorer/native/tests/tst_selection_controller.cpp` — selection parity and model reconciliation.
- Create `src/Apps/Explorer/native/tests/parity/` — controlled fixture tree, legacy oracle adapter, native adapter, normalized snapshots, and the pre-switch shadow-parity test.
- Extend `src/Apps/Explorer/tests/test_explorer_qml.py` only for migration-specific structural assertions; preserve existing tests and avoid visual rewrites.

### Integration and cleanup files

- Modify `src/Apps/Explorer/qmldir` only when the C++ module replaces the QML singleton; preserve all visual component registrations.
- Modify `src/Apps/Explorer/Main.qml`, state, and component QML only to replace behavior bindings after the equivalent typed facade/controller is tested; do not alter visual properties.
- Modify `src/System/portal/astrea_filechooser_portal.py` and `src/System/portal/src/lib.rs` to launch `astrea-explorer --portal` with both environment contracts.
- Modify `src/config/hypr/system/programs.conf` or other launch configuration only at the native-switch gate, replacing `qs -p` with the installed binary without changing unrelated desktop shell launchers.
- Extend `src/Core/bridge/apps/explorer/src/` only for typed helper ownership and persistent transport; do not remove CLI compatibility.

## Dependency graph

```text
backend_types
    -> BackendTransport
        -> OneShotCliTransport / PersistentJsonlTransport
            -> RustBackendClient
                -> controllers
                    -> AppState facade + DirectoryModel
                        -> QML presentation
```

The first three implementation tasks establish this graph and its tests before any controller or visual QML migration begins.

## Task List

### Phase 0: Baseline and foundation

### Task 1: Record the baseline and add the native CMake test harness

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/main.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.h`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/qml/NativeBootstrap.qml`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/CMakeLists.txt`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_native_bootstrap.cpp`
- Test: existing `source/AstreaOS/src/Apps/Explorer/tests/test_explorer_helper.py`, `test_explorer_qml.py`, and Rust crate tests

**Interfaces:**
- `ExplorerApplication::run(int argc, char **argv) -> int` owns one `QGuiApplication` and one `QQmlApplicationEngine`.
- `NativeBootstrap.qml` exposes only a deterministic `ready` property used by the smoke test; it is not a replacement visual UI.
- `astrea-explorer --self-test` exits zero after loading the bootstrap module; normal mode loads the bootstrap only until the native facade is wired.

- [ ] **Step 1: Capture the baseline evidence.** Run:

  ```bash
  python3 -m unittest src/Apps/Explorer/tests/test_explorer_helper.py
  python3 src/Apps/Explorer/tests/test_explorer_qml.py
  cargo test --locked --manifest-path src/Core/bridge/apps/explorer/Cargo.toml
  python3 scripts/audit_quickshell_dependencies.py src/Apps/Explorer
  ```

  Record pass counts and current Quickshell/`Process` counts in the implementation commit message or plan notes. Do not interpret baseline failures as native failures.

- [ ] **Step 2: Write the failing Qt bootstrap test.** Make `tst_native_bootstrap.cpp` launch `astrea-explorer --self-test` or instantiate `ExplorerApplication` with an offscreen platform and assert that the engine sees `NativeBootstrap.ready == true`.

- [ ] **Step 3: Run the focused test to verify it fails.** Run:

  ```bash
  cmake -S src/Apps/Explorer/native -B src/Apps/Explorer/native/build -G Ninja
  cmake --build src/Apps/Explorer/native/build
  ctest --test-dir src/Apps/Explorer/native/build -R native_bootstrap --output-on-failure
  ```

  Expected: configure/build or test failure because the native target does not exist yet.

- [ ] **Step 4: Implement the smallest Qt target.** Use `find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)`, `qt_add_executable`, and `qt_add_qml_module`. Set application name `Explorer`, organization `agony`, and domain `local` in C++ to preserve settings compatibility. Do not include or modify the existing visual QML in this foundation target.

- [ ] **Step 5: Run the native and legacy gates.** Run the commands from Step 3 plus all commands from Step 1. Expected: native bootstrap and legacy baselines pass; no launcher or visual QML changes are present.

- [ ] **Step 6: Commit the foundation.**

  ```bash
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native
  git commit -m "build(explorer): add native Qt 6 foundation"
  ```

**Dependencies:** None.

**Estimated scope:** Medium, 8–10 files.

### Task 2: Establish the typed backend transport abstraction

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/backend/backend_types.h`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/backend/backend_transport.h`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/backend/one_shot_cli_transport.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/backend/rust_backend_client.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/backend/fake_backend_client.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_backend_client.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

**Interfaces:**
- `using BackendRequestId = quint64;`
- `struct ListRequest { QString path; bool showHidden; QString sortField; bool sortAscending; bool foldersFirst; bool previews; };`
- `struct SearchRequest { QString rootPath; QString query; bool showHidden; };`
- `struct DirectoryEntry { QString fileName; QString filePath; QUrl fileUrl; bool fileIsDir; bool fileExecutable; bool fileHidden; qint64 fileSize; QDateTime fileModified; QString fileKind; QUrl filePreviewUrl; bool fileRemote; bool fileMetadataLimited; QString fileFilesystem; };`
- `class IRustBackendClient : public QObject` exposes `list(const ListRequest&)`, `search(const SearchRequest&)`, `cancel(BackendRequestId)`, and typed `listReady`, `searchReady`, and `failed` signals.
- `class BackendTransport : public QObject` carries only bounded request bytes and completion/error events; it knows nothing about controllers, QML, or presentation state.
- `RustBackendClient(IRustBackendClient)` decodes the existing CLI JSON into `DirectoryEntry` and delegates to an injected `BackendTransport`.
- `OneShotCliTransport` owns `QProcess`, constructs argv arrays, caps stdout/stderr, enforces a timeout, and emits one terminal event per request. Controllers never include its header.

- [ ] **Step 1: Add failing typed-contract tests.** Cover role-compatible decoding, malformed JSON rejection, request IDs, duplicate terminal-event suppression, nonzero exit mapping, output caps, cancellation, and a fake transport proving that `RustBackendClient` can run without a process.

- [ ] **Step 2: Run the focused test and verify failure.** Run `ctest --test-dir src/Apps/Explorer/native/build -R backend_client --output-on-failure`; expected failure is missing types/implementation.

- [ ] **Step 3: Implement the types and abstract interfaces.** Keep JSON and process details below `RustBackendClient`; expose only typed values/signals to controllers. Ensure IDs are monotonic and wrap-safe, and late/duplicate transport completions are ignored.

- [ ] **Step 4: Implement `OneShotCliTransport`.** Resolve the backend executable through an explicit configured path or deterministic libexec-relative path; never concatenate shell commands. Terminate and reap on cancellation/timeout, and fail a request exactly once.

- [ ] **Step 5: Implement `RustBackendClient` decoding.** Preserve the existing `list` and `search` CLI argv contracts and all role names. Return typed `BackendError { code, message, requestId }` without leaking raw protocol details upward.

- [ ] **Step 6: Run tests and commit.**

  ```bash
  cmake --build src/Apps/Explorer/native/build
  ctest --test-dir src/Apps/Explorer/native/build -R backend_client --output-on-failure
  git add Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native
  git commit -m "feat(explorer): add typed Rust backend client boundary"
  ```

**Dependencies:** Task 1.

**Estimated scope:** Large, 10–12 files.

### Task 3: Implement `DirectoryModel` with role and scale parity

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/models/directory_model.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_directory_model.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

**Interfaces:**
- `class DirectoryModel : public QAbstractListModel` exposes the exact existing role names and `setEntries(QVector<DirectoryEntry>, quint64 generation)`.
- `bool applyEntries(QVector<DirectoryEntry>, quint64 generation)` returns false for stale generations and does one reset for a replacement.
- `bool updatePreview(const QString &filePath, const QUrl &previewUrl, quint64 generation)` updates only the matching row.
- `QVector<QString> paths() const` supports selection reconciliation without exposing internal row storage.

- [ ] **Step 1: Write failing tests** for exact role names/values, empty data, 10,000 entries, stale generation rejection, sort changes, preview updates, and recent-only role additions.
- [ ] **Step 2: Run `ctest -R directory_model` and verify failure.**
- [ ] **Step 3: Implement contiguous typed storage and `QAbstractListModel` methods.** Do not create QML objects or call JavaScript append operations.
- [ ] **Step 4: Run the focused test and verify model reset/row-update signals.**
- [ ] **Step 5: Commit with `feat(explorer): add typed directory model`.**

**Dependencies:** Task 2.

**Estimated scope:** Small, 4–5 files.

### Checkpoint: native foundation

- [ ] Legacy Python, QML structural, and Rust suites still pass.
- [ ] Native CMake configure/build and Tasks 1–3 Qt tests pass.
- [ ] Controllers and QML still have no dependency on `QProcess`, JSONL, or transport implementation headers.
- [ ] Review the three foundation commits before proceeding to controller migration.

## Phase 2: Navigation, selection, and compatibility facade

### Task 4: Add watcher service and generation-safe navigation

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/services/directory_watch_service.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/navigation_controller.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_navigation_controller.cpp`
- Modify: native CMake files

**Interfaces:**
- `NavigationController(IRustBackendClient*, DirectoryModel*, DirectoryWatchService*)` is injected and owns no QML object.
- `BackendRequestId navigateTo(const QString &path)` increments a generation before issuing `list`.
- `BackendRequestId submitSearch(const QString &root, const QString &query)` uses the same stale-result policy.
- `void goBack()`, `void goForward()`, `void createTab(const QString&)`, `void closeTab(int)`, `void switchTab(int)`, and `void refreshCurrentFolder()` preserve current visible behavior.
- `bool remoteDirectoryActive() const` controls watcher/thumbnail policy.
- `DirectoryWatchService::watchLocalDirectory(QString)` and `DirectoryWatchService::clear()` use `QFileSystemWatcher` plus a single-shot debounce timer.

- [ ] **Step 1: Add failing tests** for A/B navigation where B completes before A, search cancellation, tabs/history, inaccessible paths, local watcher debounce, and remote watcher suppression.
- [ ] **Step 2: Verify failure with `ctest -R navigation_controller`.**
- [ ] **Step 3: Implement generation ownership in the controller.** Compare the completion generation and active path before mutating the model or state; cancel superseded backend requests when supported.
- [ ] **Step 4: Implement watcher policy/debounce.** A burst of filesystem events emits one refresh request, and remote paths register no local watcher.
- [ ] **Step 5: Run focused and legacy suites; commit `feat(explorer): add generation-safe navigation core`.**

**Dependencies:** Tasks 2 and 3.

**Estimated scope:** Medium, 7–8 files.

### Task 5: Add selection controller and narrow C++ `AppState` facade

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/selection_controller.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_selection_controller.cpp`
- Modify: native `explorer_application.*`, `CMakeLists.txt`, and QML module registration
- Do not modify visual QML in this task

**Interfaces:**
- `SelectionController(DirectoryModel*)` exposes `selectedFile`, `selectedFiles`, `lastSelectedIndex`, `isSelected`, `clearSelection`, `selectAll`, `selectByName`, and `handleSelection` with the existing QML argument shapes.
- `AppStateFacade` exposes only the compatibility properties/functions needed by unchanged list/icon/sidebar views and delegates to `NavigationController`, `SelectionController`, and `DirectoryModel`.
- The facade owns no `QProcess`, JSON parser, filesystem helper, or visual `QObject*`.

- [ ] **Step 1: Write failing tests** for single, Ctrl multi, Shift range, select-all, replacement reconciliation, and removed selected paths.
- [ ] **Step 2: Run `ctest -R selection_controller` and verify failure.**
- [ ] **Step 3: Implement selection by stable file path, not row pointer.** Reconcile after model replacement and emit only the necessary property/model signals.
- [ ] **Step 4: Implement the facade’s minimal typed surface and register it as the `AppState` singleton for the native QML module.** Keep the legacy `AppState.qml` untouched as fallback source.
- [ ] **Step 5: Run native tests and a QML engine smoke test; commit `feat(explorer): add native selection and AppState facade`.**

**Dependencies:** Tasks 3 and 4.

**Estimated scope:** Medium, 8–10 files.

### Task 6: Build the deterministic native shadow-parity gate

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/parity/fixture_tree.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/parity/legacy_oracle.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/parity/native_oracle.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/parity/parity_snapshot.h/.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_shadow_parity.cpp`
- Modify: native tests CMake registration
- Modify: `source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py` only for a structural parity-gate assertion if needed

**Interfaces:**
- `FixtureTree::create()` returns a temporary deterministic tree containing hidden files, folders, executable files, unicode/space names, nested search content, and conflict cases.
- `LegacyOracle` invokes the existing Rust CLI/Python helper compatibility paths against the fixture and returns normalized typed snapshots.
- `NativeOracle` invokes only `IRustBackendClient`, controllers, models, and services and returns the same normalized snapshot schema.
- `ParitySnapshot` compares directory entries/order, navigation/history/tabs, search, selection, previews, fixture-backed device state, recents, and file-operation outcomes.

- [ ] **Step 1: Write the failing parity test** with explicit snapshot fields and a failure diff that names the first mismatch.
- [ ] **Step 2: Run `ctest -R shadow_parity` and verify it fails because the native oracle is incomplete.**
- [ ] **Step 3: Implement fixture setup and the legacy oracle using existing contracts; normalize paths, timestamps, and nondeterministic IDs.**
- [ ] **Step 4: Implement the native oracle through typed controllers only.** Do not bypass the facade/model/controller dependency direction.
- [ ] **Step 5: Make all required snapshots pass; commit `test(explorer): add native shadow parity gate`.**

**Dependencies:** Tasks 2–5.

**Estimated scope:** Large, 8–12 files.

### Checkpoint: navigation parity

- [ ] Native and legacy snapshots pass for the required deterministic cases.
- [ ] A late A result cannot mutate B’s model, history, selection, or preview state.
- [ ] Existing visual QML remains byte-for-byte unchanged unless a later task records a behavior-only wiring reason.

## Phase 3: State and operations migration

### Task 7: Migrate settings, clipboard, launch, devices, recents, and previews

**Files:**
- Create: `src/Apps/Explorer/native/src/services/settings_service.*`
- Create: `src/Apps/Explorer/native/src/services/clipboard_service.*`
- Create: `src/Apps/Explorer/native/src/services/launch_service.*`
- Create: `src/Apps/Explorer/native/src/controllers/device_controller.*`
- Create: `src/Apps/Explorer/native/src/controllers/recent_controller.*`
- Create: `src/Apps/Explorer/native/src/controllers/preview_controller.*`
- Create: corresponding `tst_*` Qt Test files
- Modify: facade and native CMake files
- Port tests from `src/Apps/Explorer/tests/test_explorer_helper.py` only when the replacement owner is ready

**Interfaces:**
- Services use typed Qt APIs and return typed results/errors; no controller exposes a process or raw JSON to QML.
- `ClipboardService` publishes `text/uri-list` and image MIME bytes through `QMimeData`/`QClipboard`.
- `DeviceController`, `RecentController`, and `PreviewController` consume `IRustBackendClient` or services and expose stable facade properties/signals.

- [ ] **Step 1: Add failing tests** for settings compatibility, URI-list/image clipboard MIME, launch argv safety, fixture-backed device state, recents merge ordering, preview generation, and remote preview suppression.
- [ ] **Step 2: Implement one focused service/controller at a time and run its Qt Test target before wiring QML.**
- [ ] **Step 3: Add facade properties/functions with the same names and value shapes currently consumed by QML.**
- [ ] **Step 4: Run shadow parity again and commit separate focused commits for settings/clipboard/launch and devices/recents/previews.**

**Dependencies:** Tasks 2–6.

**Estimated scope:** Large; split into two or more commits by ownership.

### Task 8: Migrate file-operation orchestration and helper responsibilities

**Files:**
- Create: `src/Apps/Explorer/native/src/controllers/file_operations_controller.*`
- Create: `src/Apps/Explorer/native/src/controllers/open_with_controller.*`
- Create: `src/Apps/Explorer/native/src/services/file_operation_service.*`
- Create: corresponding Qt Test targets
- Modify: Rust `src/Core/bridge/apps/explorer/src/{main.rs,file_ops.rs}` and add focused `archive.rs`, `trash.rs`, and `open_with.rs` modules as needed
- Modify: existing helper tests by porting behavior tests; do not delete `explorer_helper.py` yet

**Interfaces:**
- `FileOperationsController` owns UI-facing workflow state, progress, cancellation, conflict/password prompts, and calls typed backend methods.
- Rust owns destructive filesystem behavior, archive traversal validation, rollback, trash metadata, and desktop-entry/default-app semantics.
- The C++ controller sees typed progress/result/error events, never Rust JSONL or helper stdout.

- [ ] **Step 1: Port the highest-risk helper tests first:** archive absolute/`..` rejection, password flow, conflicts, merge, overwrite rollback, trash metadata, broken symlinks, and no-shell launch.
- [ ] **Step 2: Run the Rust and Python focused tests before changing production ownership.**
- [ ] **Step 3: Add typed Rust CLI operations preserving existing command compatibility and JSONL progress fields.**
- [ ] **Step 4: Implement C++ orchestration and tests for copy/cut/paste, progress, cancellation, conflicts, archive prompts, trash, AppImage, and Open With.**
- [ ] **Step 5: Replace QML helper `Process` behavior with facade calls only; record every changed QML file and preserve all visual properties.**
- [ ] **Step 6: Run helper caller audit and commit focused C++/Rust operation commits.** Do not remove the helper until the audit reports zero production callers.

**Dependencies:** Tasks 2, 5, and 7.

**Estimated scope:** XL; split into low-risk clipboard/shell integration, file operations, archive/trash, and Open With commits.

## Phase 4: Native QML runtime and portal

### Task 9: Wire the unchanged visual QML to the native facade/model

**Files:**
- Modify only behavior/integration lines in `src/Apps/Explorer/Main.qml`, `components/`, `FileDialog.qml`, and `PortalDialog.qml` as proven necessary
- Modify: `src/Apps/Explorer/qmldir`
- Create: native QML module resource manifest if required by CMake
- Extend: `src/Apps/Explorer/tests/test_explorer_qml.py`

**Interfaces:**
- Existing role names and `AppState` names remain valid to QML.
- QML imports only Qt/shared visual modules and the compatibility facade/model module.
- No shipping Explorer QML contains `import Quickshell`, `import Quickshell.Io`, or `Process {}`.

- [ ] **Step 1: Add failing structural tests** for zero Quickshell imports/process nodes in the shipping native QML set and for required role names.
- [ ] **Step 2: Wire only the tested facade/model properties and functions.** Preserve every visual property, delegate hierarchy, animation, and style value.
- [ ] **Step 3: Move `AppIcon` to/reuse a neutral shared Qt module without changing its rendering; do not modify desktop-shell Quickshell runtime.**
- [ ] **Step 4: Run QML lint, offscreen engine smoke, Qt tests, legacy tests, and shadow parity.**
- [ ] **Step 5: Commit `feat(explorer): wire native facade to existing visual QML`.**

**Dependencies:** Tasks 6–8.

**Estimated scope:** Medium, only behavior integration files.

### Task 10: Migrate FileChooser portal to native mode

**Files:**
- Create/modify: `src/Apps/Explorer/native/src/controllers/portal_controller.*`
- Modify: `src/System/portal/astrea_filechooser_portal.py`
- Modify: `src/System/portal/src/lib.rs` and its tests
- Modify: native application mode dispatch and CMake install rules

**Interfaces:**
- `astrea-explorer --portal` consumes bounded `ASTREA_FILE_DIALOG_OPTIONS` or compatibility `BENCH_FILE_DIALOG_OPTIONS`.
- It publishes atomically to `ASTREA_FILE_DIALOG_RESULT_FILE` or compatibility result path and preserves `__ASTREA_FILE_DIALOG__`/`__BENCH_FILE_DIALOG__` prefixes.
- `PortalController` finalizes exactly once across accept, reject, close, timeout, malformed options, and dead consumer.

- [ ] **Step 1: Add failing Python/Rust/Qt tests** for open/save/select-folder/multiple/cancel/malformed/dead-consumer/duplicate completion paths.
- [ ] **Step 2: Implement bounded option parsing and `QSaveFile` result publication.**
- [ ] **Step 3: Update both portal launchers to argv-safe `astrea-explorer --portal`; remove no legacy launcher until both implementations pass.**
- [ ] **Step 4: Run portal unit tests and an available real FileChooser client; report real qualification separately.**
- [ ] **Step 5: Commit `feat(portal): launch native Explorer file chooser`.**

**Dependencies:** Tasks 9 and 5.

**Estimated scope:** Medium, 7–9 files.

## Phase 5: Persistent transport and final cleanup

### Task 11: Add the persistent Rust JSONL worker behind the existing client

**Files:**
- Create: `src/Core/bridge/apps/explorer/src/protocol.rs`
- Create: `src/Core/bridge/apps/explorer/src/server.rs`
- Modify: `src/Core/bridge/apps/explorer/src/main.rs`, `Cargo.toml`, and Cargo lock if dependency changes are required
- Create: `src/Apps/Explorer/native/src/backend/persistent_jsonl_transport.*`
- Extend: `tst_backend_client.cpp` and add `tst_persistent_transport.cpp`

**Interfaces:**
- CLI remains compatible; new mode is `explorer_backend serve --stdio-jsonl`.
- Request envelope: `{"version":1,"id":42,"op":"list","params":{}}`.
- Event envelope: result/progress/error with bounded lines/messages and explicit IDs.
- `PersistentJsonlTransport` implements `BackendTransport`; `RustBackendClient` and all controllers remain unchanged.

- [ ] **Step 1: Add failing Rust protocol/server tests** for malformed input, bounds, IDs, cancellation, request isolation, worker survival after one bad request, and exactly-once outstanding-request failure on EOF.
- [ ] **Step 2: Add failing C++ transport tests** for interleaved progress, reconnect generation, duplicate events, and controller-visible parity.
- [ ] **Step 3: Implement the Rust server and protocol with bounded parsing and no shell commands.**
- [ ] **Step 4: Implement the persistent transport as a drop-in replacement injected into `RustBackendClient`.**
- [ ] **Step 5: Run all backend/client/controller/parity tests and compare one-shot vs persistent behavior.**
- [ ] **Step 6: Commit `feat(explorer): add persistent Rust backend transport`.**

**Dependencies:** Tasks 2, 6, 8, and 10.

**Estimated scope:** Large, 8–12 files.

### Task 12: Switch the launcher only after the parity gate and remove transitional code

**Files:**
- Modify: launch/session configuration and package install rules
- Remove only after audits: obsolete Explorer state QML, helper callers, `QuickshellComponents` alias, and Quickshell portal launch paths
- Modify: `src/Apps/Explorer/tests/test_explorer_qml.py` and add final zero-caller/zero-Quickshell assertions
- Create: final performance/qualification scripts under `src/Apps/Explorer/native/tests/`

**Interfaces:**
- Production launcher starts `astrea-explorer`; desktop-shell Quickshell remains untouched.
- `explorer_helper.py` is removed only after the caller audit, replacement tests, and real workflow smoke pass.
- Native source has zero shipping Explorer Quickshell imports and zero `Process {}` nodes.

- [ ] **Step 1: Run the full deterministic shadow-parity suite** and preserve its machine-readable report as a test artifact, not source output.
- [ ] **Step 2: Run the full Python, Rust, C++, QML lint, and security/i18n gates.**
- [ ] **Step 3: Capture legacy/native screenshots and performance metrics using identical fixture/theme/scale/window state.** Measure PSS, CPU, threads, FDs, spawn count, startup, directory-load, and search latency.
- [ ] **Step 4: Switch the production launcher and run real-session qualification** for the workflows in the migration kit; do not call this complete if the real session is unavailable.
- [ ] **Step 5: Remove only files with zero production callers and rerun all structural/caller audits.**
- [ ] **Step 6: Commit focused cleanup and qualification commits; never amend earlier commits.**

**Dependencies:** Tasks 6, 9, 10, and 11.

**Estimated scope:** Large, split by launcher switch, helper cleanup, and qualification.

## Checkpoints

### Checkpoint A: Foundation

- [ ] Tasks 1–3 pass and are committed.
- [ ] Existing tests still pass; Rust CLI command behavior is unchanged.
- [ ] `IRustBackendClient` and `BackendTransport` are the only backend dependencies visible to controllers.

### Checkpoint B: Navigation shadow parity

- [ ] Tasks 4–6 pass and are committed.
- [ ] Deterministic legacy/native parity passes for entries/order/navigation/search/selection/previews/devices/recents/file operations included so far.
- [ ] No production launcher switch has occurred.

### Checkpoint C: Native runtime

- [ ] Tasks 7–10 pass, QML remains visually unchanged, and both portal launchers pass.
- [ ] Native runtime is selectable for qualification while legacy remains available.

### Checkpoint D: Final architecture

- [ ] Task 11 persistent transport passes without controller changes.
- [ ] Task 12 zero-caller, zero-Quickshell, parity, performance, and real-session evidence is recorded.

## Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Native types accidentally expose transport details to QML | High | Keep `IRustBackendClient`/facade APIs typed; structural tests reject QProcess/JSONL in QML and controllers. |
| Late backend results replace newer navigation | High | Generation IDs are required in client, controller, model, and parity tests. |
| Visual drift while removing process blocks | High | Behavior-only QML diffs, visual hash manifest, identical screenshot fixtures, no styling cleanup. |
| Python archive migration regresses security | Critical | Port path traversal/password/conflict/rollback tests before ownership transfer; migrate archives last. |
| One-shot and persistent transports diverge | High | Both implement `BackendTransport`; run the same client/controller/parity suite against each. |
| Portal implementations diverge | High | Update and test both Python and Rust launchers with the same environment/result contract. |
| Empty-root index state causes unrelated files to be committed | High | Use path-limited `git add`/`git commit --only`; inspect staged diff before each commit. |
| Missing real desktop session blocks qualification | Medium | Report deterministic/automated gates separately and leave launcher switch/cleanup incomplete. |

## Plan self-review

- The transport abstraction is established before controllers and is reused by one-shot and persistent implementations.
- The dependency direction is stated globally and tested in Tasks 2, 5, 6, and 9.
- Shadow parity is a blocking task before the native launcher switch in Task 12.
- Visual QML is preserved and all changes require behavior-only justification.
- Existing Rust CLI and Python helper behavior remain available until zero-caller and parity gates pass.
- Every task names files, interfaces, tests, dependencies, and focused commit boundaries.
