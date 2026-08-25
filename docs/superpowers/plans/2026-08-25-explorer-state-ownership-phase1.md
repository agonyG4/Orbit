# Orbit Explorer State Ownership Closure — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract settings, sidebar favorites, and archive state ownership from `AppStateFacade` while preserving the public QML contract and all current behavior.

**Architecture:** Keep `AppStateFacade` as the only native QML projection and inject a typed dependency bundle. `ExplorerSettingsController` owns `ExplorerSettings` persistence and coordinates navigation/device settings; `SidebarFavoritesController` owns favorite semantics/model transactions; `ArchiveController` owns archive state and completion ordering. The composition root constructs owners explicitly and the QML compatibility chain remains intact.

**Tech Stack:** C++17, Qt 6 Core/Test/Qml, QSettings INI persistence, Qt Quick/QML compatibility adapters, CMake presets, Rust workspace validation, Python unittest gates.

## Global Constraints

- Preserve the complete public `AppState`/`AppStateFacade` property, signal, and invokable contract.
- Preserve `explorer.conf`, its `Explorer` group, all current keys, backend protocol/JSON, runtime/install paths, CLI/D-Bus contracts, and visual behavior.
- Do not reorganize top-level repository layout or remove the QML compatibility layer.
- Keep `DeviceController` authoritative for runtime auto-mount state and `SettingsService` as the storage mechanism.
- Move tests with ownership; replace implementation-detail tests with equivalent behavioral tests rather than deleting coverage.
- Use Rtk for shell commands and commit each meaningful ownership boundary after its focused tests pass.

---

### Task 1: Lock the public facade contract and record baseline

**Files:**
- Modify: `apps/explorer/tests/cpp/tst_app_state_facade.cpp`
- Modify: `apps/explorer/tests/cpp/tst_app_state_compatibility.cpp` only if the end-to-end fixture needs a shared helper

**Interfaces:**
- Consumes: current `AppStateFacade::staticMetaObject`
- Produces: explicit property/type/writable, invokable-signature, and public-signal expectations that remain valid after owner extraction

- [ ] **Step 1: Add explicit Qt meta-object expectations**

Use `QMetaObject::indexOfProperty`, `QMetaProperty::typeName`, `QMetaProperty::isWritable`, and `QMetaObject::indexOfSignal`. Enumerate the current public properties, writable properties, invokable signatures, and adapter-facing signals explicitly; do not iterate the implementation to generate expected values.

- [ ] **Step 2: Run the focused facade test**

Run: `rtk ctest --test-dir build/debug -R '^app_state_facade$' --output-on-failure`

Expected: PASS on the unchanged baseline, proving the contract list matches the current surface.

- [ ] **Step 3: Commit the contract lock**

```bash
rtk git add apps/explorer/tests/cpp/tst_app_state_facade.cpp apps/explorer/tests/cpp/tst_app_state_compatibility.cpp
rtk git commit -m "test: lock Explorer AppState facade contract"
```

### Task 2: Add settings controller and auto-mount regression tests first

**Files:**
- Create: `apps/explorer/src/controllers/explorer_settings_controller.h`
- Create: `apps/explorer/src/controllers/explorer_settings_controller.cpp`
- Create: `apps/explorer/tests/cpp/tst_explorer_settings_controller.cpp`
- Modify: `apps/explorer/src/controllers/device_controller.h`
- Modify: `apps/explorer/src/controllers/device_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_device_controller.cpp`

**Interfaces:**
- Consumes: `Services::SettingsService`, `Services::ExplorerSettings`, `NavigationController`, and `DeviceController`
- Produces: settings-domain getters/setters/signals, `bindNavigation`, `bindDeviceController`, and canonical `DeviceController::setAutoMountDeviceIdsJson`

- [ ] **Step 1: Add failing settings/auto-mount tests**

Cover startup settings application, navigation-originated persistence, facade-style settings writes through controller methods, zoom clamping to `0.75..2.0`, current-path persistence, persisted auto-mount initialization, runtime enable/disable persistence, compatibility JSON update, reload, and malformed JSON canonicalization. Run the new test target before writing production code.

- [ ] **Step 2: Verify RED**

Run: `rtk cmake --build --preset debug --target tst_explorer_settings_controller tst_device_controller`

Expected: the new target is absent or fails to compile because the controller and JSON setter do not exist yet. Keep the failure evidence in the task notes.

- [ ] **Step 3: Implement the minimal settings/device boundary**

Move `ExplorerSettings` loading/state/persistence into the new controller. Apply navigation values before connecting feedback signals; on listing/current-path changes, compare canonical values, persist once, and emit focused settings signals. Add the device JSON setter using the existing array-of-strings normalization and emit `autoMountChanged` only when canonical runtime state changes.

- [ ] **Step 4: Verify GREEN**

Run: `rtk cmake --build --preset debug --target tst_explorer_settings_controller tst_device_controller && rtk ctest --test-dir build/debug -R '^(explorer_settings_controller|device_controller)$' --output-on-failure`

Expected: all focused settings and device tests pass.

- [ ] **Step 5: Commit the settings boundary**

```bash
rtk git add apps/explorer/src/controllers/explorer_settings_controller.* apps/explorer/src/controllers/device_controller.* apps/explorer/tests/cpp/tst_explorer_settings_controller.cpp apps/explorer/tests/cpp/tst_device_controller.cpp
rtk git commit -m "refactor: own Explorer settings and auto-mount persistence"
```

### Task 3: Add sidebar favorites controller and migrate favorite behavior

**Files:**
- Create: `apps/explorer/src/controllers/sidebar_favorites_controller.h`
- Create: `apps/explorer/src/controllers/sidebar_favorites_controller.cpp`
- Create: `apps/explorer/tests/cpp/tst_sidebar_favorites_controller.cpp`
- Modify: `apps/explorer/src/models/sidebar_favorites_model.h` only if the controller needs an existing public model hook
- Modify: `apps/explorer/tests/cpp/tst_sidebar_favorites_model.cpp` only for behavior that belongs to the model itself

**Interfaces:**
- Consumes: `ExplorerSettingsController`, `SidebarFavoritesModel`, home/XDG paths
- Produces: current favorite getters, default/hidden interpretation, pin/remove/reorder APIs, drag transaction APIs, model pointer, revision/change signals

- [ ] **Step 1: Add failing controller tests**

Move behavioral coverage for persisted custom favorites, hidden defaults, default visibility, duplicate prevention, invalid/non-local pin rejection, hide/restore, custom removal, reorder, drag preview/cancel/commit, single persistence, normalized identity, and revision publication into `tst_sidebar_favorites_controller.cpp`.

- [ ] **Step 2: Verify RED**

Run: `rtk cmake --build --preset debug --target tst_sidebar_favorites_controller`

Expected: failure because the controller target/source does not exist.

- [ ] **Step 3: Implement semantic ownership**

Move the existing normalization/default parsing/order logic into the controller. Keep `SidebarFavoritesModel` as the model/transaction primitive, call settings-controller persistence only after a committed semantic change, and retain the current model roles and QML drag behavior.

- [ ] **Step 4: Verify GREEN and facade projection**

Run: `rtk cmake --build --preset debug --target tst_sidebar_favorites_controller tst_app_state_facade && rtk ctest --test-dir build/debug -R '^(sidebar_favorites_controller|sidebar_favorites_model|app_state_facade)$' --output-on-failure`

Expected: dedicated favorite tests and existing facade/model projection tests pass.

- [ ] **Step 5: Commit the sidebar boundary**

```bash
rtk git add apps/explorer/src/controllers/sidebar_favorites_controller.* apps/explorer/tests/cpp/tst_sidebar_favorites_controller.cpp apps/explorer/tests/cpp/tst_sidebar_favorites_model.cpp
rtk git commit -m "refactor: own Explorer sidebar favorite state"
```

### Task 4: Add archive controller and migrate the state machine

**Files:**
- Create: `apps/explorer/src/controllers/archive_controller.h`
- Create: `apps/explorer/src/controllers/archive_controller.cpp`
- Create: `apps/explorer/tests/cpp/tst_archive_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_app_state_facade.cpp` to retain projection/delegation tests and remove only duplicate implementation setup after equivalent controller tests exist

**Interfaces:**
- Consumes: `FilesystemService`, `NavigationController`, `UtilityResult`
- Produces: all archive getters, start/continuation/cancel methods, `archiveWorkflowOccupied`, state/completion signals, and explicit terminal completion data for the facade

- [ ] **Step 1: Add/migrate failing archive tests**

Move every current archive regression into the dedicated target, including mutual exclusion, terminal admission, password/conflict continuations, occupied-state reporting, stale context, unsupported policy, inert cancellation, and re-entrant terminal admission. Add an assertion that the controller releases its slot before its state notification reaches a re-entrant handler.

- [ ] **Step 2: Verify RED**

Run: `rtk cmake --build --preset debug --target tst_archive_controller`

Expected: failure because the controller target/source does not exist.

- [ ] **Step 3: Implement the controller with explicit terminal ordering**

Move the exact current state transitions. Connect to `FilesystemService::operationFinished` inside `ArchiveController`; match only its request ID, update the aggregate terminal snapshot, clear the owned request/slot before emitting the controller state signal, and expose a separate completion signal for the facade’s existing `filesystemActionFinished` forwarding. Preserve extraction navigation after a successful destination result.

- [ ] **Step 4: Verify GREEN**

Run: `rtk cmake --build --preset debug --target tst_archive_controller tst_app_state_facade && rtk ctest --test-dir build/debug -R '^(archive_controller|app_state_facade)$' --output-on-failure`

Expected: all archive invariants and the facade’s remaining projection tests pass.

- [ ] **Step 5: Commit the archive boundary**

```bash
rtk git add apps/explorer/src/controllers/archive_controller.* apps/explorer/tests/cpp/tst_archive_controller.cpp apps/explorer/tests/cpp/tst_app_state_facade.cpp
rtk git commit -m "refactor: own Explorer archive workflow state"
```

### Task 5: Replace the facade constructor and reduce facade ownership

**Files:**
- Modify: `apps/explorer/src/controllers/app_state_facade.h`
- Modify: `apps/explorer/src/controllers/app_state_facade.cpp`
- Modify: all current `AppStateFacade` construction sites under `apps/explorer/src` and `apps/explorer/tests/cpp`
- Modify: `apps/explorer/src/explorer_application.cpp`

**Interfaces:**
- Consumes: the three dedicated controllers and existing controllers/services
- Produces: `AppStateFacadeDependencies`, explicit forwarding getters/setters/invokables, and unchanged Qt meta-object surface

- [ ] **Step 1: Add a compile/test guard for owner absence**

Extend the facade contract/self-review test to assert that the facade source/header contains none of the removed mutable owner fields (`m_settings`, `m_settingsService`, `m_sidebarFavoritesModel`, `m_sidebarFavoritesRevision`, and every listed `m_archive*` member) after the migration. Keep behavioral assertions in dedicated controller suites.

- [ ] **Step 2: Implement typed dependencies**

Forward-declare pointer-only types in the facade header, define the dependency struct with named fields, assert mandatory pointers, and update the application composition root to create settings, navigation/device binding, favorites, archive, then facade in lifetime-safe order. Do not add a DI framework or a second QML facade.

- [ ] **Step 3: Replace settings/favorite/archive implementation with delegation**

Delete only the facade’s duplicated domain state/algorithms and route all existing public members to their owners. Keep unrelated filesystem, device projection, icon, wallpaper, dialog, runtime, selection, and navigation compatibility behavior unchanged.

- [ ] **Step 4: Verify the contract and focused tests**

Run: `rtk cmake --build --preset debug --target tst_app_state_facade tst_app_state_compatibility && rtk ctest --test-dir build/debug -R '^(app_state_facade|app_state_compatibility)$' --output-on-failure`

Expected: explicit API contract, projection, delegated settings/favorites, and archive tests pass.

- [ ] **Step 5: Commit the facade projection boundary**

```bash
rtk git add apps/explorer/src/controllers/app_state_facade.* apps/explorer/src/explorer_application.cpp apps/explorer/tests/cpp/tst_app_state_facade.cpp apps/explorer/tests/cpp/tst_app_state_compatibility.cpp
rtk git commit -m "refactor: make AppStateFacade a typed projection"
```

### Task 6: Close the QML native-state projection and documentation

**Files:**
- Modify: `apps/explorer/qml/state/FileOperationsState.qml`
- Modify: `apps/explorer/tests/cpp/tst_app_state_compatibility.cpp`
- Modify: `apps/explorer/docs/APPSTATE_COMPATIBILITY.md`
- Modify: `apps/explorer/docs/OPERATION_PROGRESS_ARCHITECTURE.md`
- Modify: `docs/ARCHITECTURE.md`

**Interfaces:**
- Consumes: `NativeAppStateAdapter.qml` and the unchanged `AppState.qml` aliases
- Produces: truthful native-state projection and documentation distinguishing public compatibility from internal ownership

- [ ] **Step 1: Add failing end-to-end QML projection tests**

Load the actual `AppState.qml`/adapter chain with a native facade fixture whose representative properties are non-default. Assert the public values for `archivePasswordError`, `archiveConflictVisible`, `archiveConflictDestination`, `archiveConflictName`, `appImageInstallRunning`, and `wallpaperApplyRunning`. Run before changing `FileOperationsState.qml`.

- [ ] **Step 2: Verify RED**

Run: `rtk cmake --build --preset debug --target tst_app_state_compatibility && rtk ctest --test-dir build/debug -R '^app_state_compatibility$' --output-on-failure`

Expected: the hard-coded `FileOperationsState.qml` placeholders make the new assertions fail.

- [ ] **Step 3: Bind native-owned properties to the adapter**

Replace only the native-owned placeholders with adapter-backed bindings. Leave QML-owned transient password input and all presentation/layout code intact.

- [ ] **Step 4: Verify GREEN and update docs**

Run: `rtk cmake --build --preset debug --target tst_app_state_compatibility && rtk ctest --test-dir build/debug -R '^app_state_compatibility$' --output-on-failure`

Update the three docs to describe the compatibility facade, native owners, dependency direction, and follow-up compatibility-retirement boundary.

- [ ] **Step 5: Commit projection/docs**

```bash
rtk git add apps/explorer/qml/state/FileOperationsState.qml apps/explorer/tests/cpp/tst_app_state_compatibility.cpp apps/explorer/docs/APPSTATE_COMPATIBILITY.md apps/explorer/docs/OPERATION_PROGRESS_ARCHITECTURE.md docs/ARCHITECTURE.md
rtk git commit -m "fix: expose native Explorer state through compatibility QML"
```

### Task 7: Full qualification and skeptical diff review

**Files:**
- Modify only any test/build registration/docs corrections required by the preceding tasks

- [ ] **Step 1: Review ownership and deleted coverage**

Run:

```bash
rtk rg -n 'm_settings|m_settingsService|m_sidebarFavoritesModel|m_sidebarFavoritesRevision|m_archive(Request|Path|Destination|ConflictPolicy|Running|PasswordPrompt|Conflict|Progress|Percent|DoneCount|TotalCount|FileName|Status|Error|DestinationResult|PasswordError|ConflictDestination|ConflictName)' apps/explorer/src/controllers/app_state_facade.*
rtk git diff --stat
rtk git diff --check
```

Expected: no removed domain owner fields remain in the facade, no unrelated hotspot changes are present, and whitespace is clean.

- [ ] **Step 2: Run Python/i18n/source gates**

```bash
rtk run 'python3 -m unittest discover -s apps/explorer/tests/qml -p "test_*.py" -v'
rtk run 'python3 -m unittest discover -s shared/qml/Astrea/I18n -p "test_*.py" -v'
rtk run 'python3 shared/qml/Astrea/I18n/validate_i18n.py'
rtk run 'python3 scripts/verify_orbit_source_gate.py'
```

- [ ] **Step 3: Run Rust gates**

```bash
rtk cargo fmt --all -- --check
rtk cargo check --workspace --locked
rtk cargo clippy --workspace --all-targets --locked -- -D warnings
rtk cargo test --workspace --locked
```

- [ ] **Step 4: Run Debug, Release, and clean install gates**

```bash
rtk cmake --preset debug
rtk cmake --build --preset debug
rtk ctest --preset debug --output-on-failure
rtk cmake --preset release
rtk cmake --build --preset release
rtk ctest --preset release --output-on-failure
rtk run 'python3 scripts/verify_explorer_clean_install.py'
```

- [ ] **Step 5: Check final repository hygiene and commit**

Verify no source symlinks, no generated Python caches in source, no active retired migration paths, and `rtk git status --short` is understood. Commit the final verified diff with:

```bash
rtk git add -A
rtk git diff --cached --check
rtk git commit -m "refactor: close Explorer state ownership phase one"
```

