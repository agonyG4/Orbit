# Astrea Explorer Native Behavior Completion Implementation Plan

> **For agentic workers:** Execute this plan inline on the existing `main` checkout. Keep each completed slice as a focused commit and preserve unrelated pre-existing worktree state.

**Goal:** Complete the native Explorer behavior boundary and prove that the existing Explorer QML can load through the native C++ facade without switching the production launcher.

**Architecture:** QML consumes only the compatibility facade and native presentation models. Focused controllers consume typed services and `IRustBackendClient`; `RustBackendClient` consumes the injected `BackendTransport`; the initial transport remains asynchronous one-shot CLI execution. The legacy QML/Quickshell runtime remains available as fallback until deterministic parity and visual gates pass.

**Tech Stack:** Qt 6.11, C++17, CMake/CTest, QML/Qt Quick, existing Rust `explorer_backend`, existing Python helper during the transition, and Qt Test.

## Global Constraints

- Work only in `/home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS` plus this plan and native documentation.
- Stay on the existing `main` checkout; do not create a branch or worktree, reset, clean, amend, squash, rewrite history, or discard unrelated changes.
- Preserve all pre-existing unrelated staged, modified, deleted, and untracked state.
- Preserve the existing visual QML contract exactly; QML edits are limited to runtime integration and must not change visual values, layout, animation, typography, icons, or interaction style.
- Keep the dependency direction `QML -> facade/models -> controllers -> services -> typed backend interface -> transport -> Rust backend`.
- QML must not call `RustBackendClient`, `QProcess`, filesystem helpers, Python helpers, or a Rust worker directly.
- Controllers must not manipulate QML objects; transports must not own presentation state.
- Do not implement the persistent JSONL worker, production launcher switch, production portal switch, helper deletion, or shared Quickshell cleanup in this task.
- The native launcher is not production-ready until the deterministic shadow-parity gate passes.

## Existing baseline

- Legacy Python helper tests: 52 passed.
- Legacy Explorer QML structural tests: 17 passed.
- Rust Explorer backend tests: 37 passed.
- Existing native CTest suite: 13 passed.
- Existing native project: `source/AstreaOS/src/Apps/Explorer/native/`.
- Existing native gaps: asynchronous process teardown, three missing focused controllers, incomplete facade, JS `ListModel` reconstruction in list/icon views, bootstrap-only QML integration, and parity oracles that bypass several production controllers.

## Task 1: Make one-shot cancellation asynchronous

**Files:**

- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/backend/one_shot_cli_transport.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/backend/one_shot_cli_transport.cpp`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_backend_client.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt` only if a helper test target is needed

**Interface:** `OneShotCliTransport` retains `BackendTransport::start()` and `cancel()`; cancellation emits at most one terminal event and owns a request until the process reaches `NotRunning`.

- [ ] Add failing tests for cancel-before-output, cancel-while-running, terminate grace expiry, duplicate cancellation, late `finished`, process crash, and destruction with active requests.
- [ ] Run the focused backend test and observe the missing asynchronous lifecycle behavior.
- [ ] Add per-request terminal/cancelling state and a single-shot grace timer. `cancel()` marks the request terminal for publication, calls `terminate()`, and schedules `kill()` without waiting. Process `finished` and timer callbacks only clean up; they never publish a second result.
- [ ] Ensure timeout/output-limit failures use the same asynchronous cleanup path and preserve exactly-once error publication.
- [ ] Add deterministic test-only process fixtures using `python3` argv arrays; do not add sleeps or GUI-thread waits to production transport code.
- [ ] Run backend tests and commit `fix(explorer): make backend cancellation asynchronous`.

## Task 2: Add the missing controller/service boundary

**Files:**

- Create: `native/src/controllers/file_operations_controller.h/.cpp`
- Create: `native/src/controllers/open_with_controller.h/.cpp`
- Create: `native/src/controllers/portal_controller.h/.cpp`
- Create or modify: `native/src/services/file_operation_service.h/.cpp`
- Modify: `native/src/backend/backend_types.h`, `rust_backend_client.h/.cpp` only for typed operations already present in the Rust CLI
- Add: focused Qt tests for each controller
- Modify: `native/CMakeLists.txt`

**Interfaces:** Controllers expose typed properties, signals, and methods for QML-facing state. They accept `IRustBackendClient` or typed services by injection and never accept a `QProcess`, raw JSON, or QML object.

- [ ] Add failing tests for file-operation request state, progress, cancellation, conflict resolution, and exactly-once terminal outcomes.
- [ ] Add failing tests for Open With catalog selection and argv-safe launch requests.
- [ ] Add failing tests for portal mode, selection constraints, accept/reject/close, timeout/dead-consumer completion, and duplicate terminal events.
- [ ] Implement the narrow controllers as orchestration/state holders. Reuse existing Rust operations and Python helper behavior; do not duplicate filesystem-heavy behavior in C++.
- [ ] Add archive traversal, absolute path, symlink escape, partial extraction, rollback, and conflict regressions at the existing Rust/Python ownership layer before routing through the new controller.
- [ ] Commit each independently reviewable controller slice, keeping helper and legacy portal files intact.

## Task 3: Wire the real ownership graph

**Files:**

- Modify: `native/src/explorer_application.h/.cpp`
- Modify: `native/CMakeLists.txt`
- Modify: existing controllers/services only for injected dependencies and signal forwarding

**Target graph:** `ExplorerApplication` owns one backend transport, `RustBackendClient`, all long-lived services, `DirectoryModel`, focused controllers, and `AppStateFacade`; dependencies are passed explicitly with QObject parents or consistent RAII.

- [ ] Add failing ownership/facade delegation tests that construct the same graph as `ExplorerApplication`.
- [ ] Instantiate `DirectoryWatchService`, `ClipboardService`, `SettingsService`, `LaunchService`, `NavigationController`, `SelectionController`, `FileOperationsController`, `PreviewController`, `DeviceController`, `RecentController`, `OpenWithController`, and `PortalController` from the application root.
- [ ] Ensure no controller constructs a transport or process implementation and no service exposes presentation state.
- [ ] Register only the facade/models/controllers required by QML; do not add global singletons beyond the existing `AppState` compatibility singleton.
- [ ] Run all native unit tests and commit `refactor(explorer): wire native application ownership`.

## Task 4: Complete the AppState contract and native model path

**Files:**

- Create: `source/AstreaOS/src/Apps/Explorer/native/docs/APPSTATE_COMPATIBILITY.md`
- Modify: `native/src/controllers/app_state_facade.h/.cpp`
- Modify or create: focused native projection/proxy model files under `native/src/models/`
- Modify: `Apps/Explorer/components/views/FileListView.qml`, `FileIconView.qml`, `FileDialog.qml`, and only other behavior-only QML files proven necessary
- Modify: native and legacy structural tests

- [ ] Generate the AppState manifest from every Explorer QML reference and record QML name, kind, legacy owner, native owner, status, and semantic notes. The native bootstrap gate must have no unexplained missing member.
- [ ] Add failing facade contract tests covering all required properties, methods, signals, models, device/recent/preview/file-operation state, path/format helpers, tabs, sidebar state, and launch/open-with entry points.
- [ ] Implement missing facade members by delegation to focused controllers/services. Keep the facade narrow and do not move behavior into it.
- [ ] Add bounded `count`/`get(row)` compatibility only for QML paths that still require it; expose the bulk list and icon presentation through `DirectoryModel` plus native proxy/projection models.
- [ ] Remove the 10k-entry JavaScript `append()` reconstruction path from the native visual route while retaining delegate dimensions and styling exactly.
- [ ] Add a real 10k presentation-chain regression proving row count, sorting/filtering, stable selection, and absence of per-entry JS model population.
- [ ] Commit `feat(explorer): complete native AppState compatibility facade` and `refactor(explorer): replace QML file-model duplication` as separate logical changes.

## Task 5: Load the actual Explorer QML tree

**Files:**

- Modify: `native/src/explorer_application.*`
- Add or modify: native QML resource/module registration
- Modify: behavior-only integration lines in `Apps/Explorer/Main.qml`, `qmldir`, and Explorer components/states as required
- Create: `native/tests/tst_real_main_qml.cpp`
- Modify: QML structural tests

- [ ] Add a failing integration test that loads the actual Explorer `Main.qml` with the native facade and captures every QML warning/error.
- [ ] Register the same theme/i18n dependencies and real models/controllers needed by `Main.qml`; do not substitute another fake root.
- [ ] Replace only Explorer-specific Quickshell environment/process access with facade/controller calls or Qt-native equivalents. Keep `PortalDialog.qml` and legacy launcher available as fallback until the native portal gate is separately approved.
- [ ] Fail the test on missing properties, methods, signals, models, unresolved imports, and component creation errors.
- [ ] Run `qmllint` and the existing Python QML contract tests. Record every intentional QML source difference in the manifest.
- [ ] Commit `test(explorer): load real Main QML natively`.

## Task 6: Upgrade shadow parity to production controller paths

**Files:**

- Modify: `native/tests/parity/fixture_tree.*`
- Modify: `native/tests/parity/legacy_oracle.*`
- Modify: `native/tests/parity/native_oracle.*`
- Modify: `native/tests/parity/parity_snapshot.*`
- Modify: `native/tests/tst_shadow_parity.cpp`

- [ ] Add deterministic fixture snapshots for directory entries/order, sorting, search, navigation/history/tabs, selection, previews, devices, recents, clipboard semantics, launch, Open With, and file operations.
- [ ] Keep the legacy oracle bound to the real helper/backend implementation and normalize only paths, timestamps, and fixture-specific nondeterminism.
- [ ] Route the native oracle through the same `ExplorerApplication` ownership graph, `IRustBackendClient`, focused controllers, services, and facade/models used in production; do not use `QFile`/`QDir` imitation as a replacement oracle.
- [ ] Compare filesystem trees, relevant metadata, controller terminal/error contracts, selection/model updates, and result ordering after each operation.
- [ ] Produce a machine-readable mismatch that names the slice and first differing field.
- [ ] Commit `test(explorer): qualify native behavior shadow parity` only after the deterministic parity suite passes.

## Task 7: Audit legacy ownership and remaining dependencies

**Files:**

- Create: `native/docs/PYTHON_HELPER_OWNERSHIP.md`
- Create: `native/docs/QUICKSHELL_DEPENDENCY_AUDIT.md`
- Modify: `scripts/audit_quickshell_dependencies.py` only if needed for the report
- Do not delete `explorer_helper.py`, `PortalDialog.qml`, or the legacy launcher

- [ ] Classify every helper command by actual caller as native/Rust replacement, legacy-only, zero callers, or temporarily retained.
- [ ] Classify every Explorer-side Quickshell dependency as native blocker, legacy fallback, portal-only, or shared infrastructure outside scope.
- [ ] Search and classify all remaining `waitForFinished(`, `Process {`, `Quickshell`, `AppState.`, `fileModel.get(`, `displayModel.append(`, and `sectionModel.append(` occurrences.
- [ ] Search native production C++ for `system(`, `popen(`, shell concatenation, and blocking process waits. Test-only legacy oracle waits must be explicitly marked and kept out of GUI runtime.
- [ ] Commit documentation only when it reflects actual current callers and test results.

## Task 8: Qualify without switching production

**Files:**

- Add: developer/test native launch path for `astrea-explorer` real UI if missing
- Add: native qualification/performance scripts under `native/tests/` only when runnable in the current environment
- Modify: no production launcher or portal launcher

- [ ] Run Python, Rust, Qt, real `Main.qml`, shadow parity, 10k presentation, QML lint, archive/security, and i18n checks; report PASS/FAIL/NOT RUN without inference.
- [ ] Run ASan and UBSan only if repository/toolchain support is available; do not claim unavailable sanitizer qualification.
- [ ] Compare legacy and native screenshots for the required states only if a real session is available; otherwise report visual qualification NOT RUN.
- [ ] Measure PSS from `/proc/<pid>/smaps_rollup`, idle CPU, threads, FDs, child spawns, startup, directory load, 10k load, search, and rapid-navigation latency only when actually run.
- [ ] Confirm the legacy launcher remains available and no production switch has occurred.
- [ ] Commit only focused qualification/report changes. The final decision must remain `NATIVE BEHAVIOR COMPLETION NOT COMPLETE` for any unpassed required gate.

## Checkpoints

### Checkpoint A: asynchronous transport and ownership

- [ ] Backend cancellation tests pass with no `waitForFinished()` in production transport.
- [ ] Full `ExplorerApplication` ownership graph constructs in tests.
- [ ] Legacy Python, QML, and Rust suites remain green.

### Checkpoint B: real compatibility

- [ ] The manifest has no unexplained missing AppState member.
- [ ] The real `Main.qml` loads through native Qt with no QML errors.
- [ ] The native presentation chain consumes `DirectoryModel`/native proxy directly and has no 10k JS copy loop.

### Checkpoint C: blocking parity gate

- [ ] All deterministic shadow slices pass through actual native controllers/services.
- [ ] Visual/session/portal/sanitizer/performance status is reported from evidence.
- [ ] Legacy fallback remains available and the production launcher is unchanged.

## Final decision

Return exactly one of the prompt’s required decisions. Unless every required deterministic and real-runtime gate has actually passed, return:

`NATIVE BEHAVIOR COMPLETION NOT COMPLETE — REMAINING GATES:` followed by the exact unresolved gates.
