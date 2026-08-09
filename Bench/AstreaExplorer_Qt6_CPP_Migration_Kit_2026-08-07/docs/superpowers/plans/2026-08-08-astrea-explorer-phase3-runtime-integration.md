# Astrea Explorer Phase 3 Native Runtime Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline execution was selected). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the native Qt executable load the existing Explorer `Main.qml` through a validated `ASTREA_ROOT`/installed/development resolver and expose the tested native state graph without changing the visual interface.

**Architecture:** Add a pure `ExplorerRuntimeResolver` that normalizes a valid Astrea runtime root and all dependent paths. `ExplorerApplication` owns the resolved runtime, typed backend transport, models, controllers, and services, registers the native facade, and loads the existing local QML file. The facade preserves the current core QML names while delegating navigation, selection, model, settings, clipboard, and backend error behavior to focused native owners; transitional QML remains documented and available as fallback.

**Tech Stack:** Qt 6.11, C++17, CMake/CTest, QML/Qt Quick, Qt Test, existing Rust `explorer_backend`, existing Explorer QML, and Python structural tests.

## Global Constraints

- Work only under `/home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07` and its already committed Phase 3 documentation.
- Preserve all unrelated staged, modified, deleted, and untracked state in `/home/agony/GitHub/Orbit`; stage files by exact path and never use broad `git add`.
- Do not reset, clean, amend, squash, or rewrite existing history.
- Never override an explicitly set `ASTREA_ROOT`; an explicit invalid value is a hard startup error.
- An invalid installed root must fall through to development discovery.
- Do not hardcode `/home/agony` or any repository-specific absolute path in source.
- Load the existing `Apps/Explorer/Main.qml`; do not create a replacement visual entry point.
- Preserve QML geometry, styling, layout, delegates, animations, theme imports, translations, assets, and the legacy launcher/portal fallback.
- Do not remove `AppState.qml`, `PortalDialog.qml`, `explorer_helper.py`, Quickshell imports, or the old launcher in this phase.
- Keep `QML -> facade/models -> controllers -> services -> typed backend -> transport -> Rust` dependency direction.
- Production code must not use `QProcess::waitForFinished()` for normal runtime operations.

---

## Existing file map

- `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.*`: application lifecycle, current bootstrap-only load, singleton registration, and self-test.
- `source/AstreaOS/src/Apps/Explorer/native/src/backend/one_shot_cli_transport.*`: asynchronous one-shot Rust backend transport with a configurable backend program.
- `source/AstreaOS/src/Apps/Explorer/native/src/models/directory_model.*`: QAbstractListModel with legacy role names and generation filtering.
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/navigation_controller.*`: navigation, history, tabs, search, watcher, model updates, and load errors.
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/selection_controller.*`: stable-path selection state and model-reset reconciliation.
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.*`: current native QML compatibility surface for core navigation/selection/settings properties.
- `source/AstreaOS/src/Apps/Explorer/native/src/services/settings_service.*`: persisted Explorer settings.
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/{file_operations,device,preview,recent,open_with,portal}_controller.*`: existing focused behavior owners to wire only where a current QML contract already has a native equivalent.
- `source/AstreaOS/src/Apps/Explorer/Main.qml`: visual entry point to load unchanged except for proven behavior-only runtime bindings.
- `source/AstreaOS/src/Apps/Explorer/AppState.qml` and `state/*.qml`: legacy transitional state to retain and audit, not delete.
- `source/AstreaOS/src/Apps/Explorer/native/tests/`: existing Qt Test suite and native bootstrap test target.

## Runtime interfaces

The new resolver must expose a small value type with these semantics:

```cpp
struct ExplorerRuntimePaths {
    QString root;
    QString explorerDirectory;
    QString explorerMainQml;
    QString backendProgram;
    QString helperProgram;
    QString launcherProgram;
    QString windowsRunnerProgram;
    QStringList importPaths;
    QStringList diagnostics;
    bool valid = false;
};

class ExplorerRuntimeResolver final {
public:
    static ExplorerRuntimePaths resolve(
        const QString &executableDirectory,
        const QString &homeDirectory,
        const QProcessEnvironment &environment);
};
```

`resolve()` tests an explicit environment root first, then
`homeDirectory/.local/share/Astrea`, then every executable-directory ancestor.
`ASTREA_ROOT` being present in `environment` is distinct from it being empty.
The validator accepts only a direct runtime root containing the six required
Explorer/QML paths from the design spec. Optional helper/launcher/backend
paths are reported as empty when not present; the backend path uses the first
existing candidate under `Core/bridge/apps/explorer_backend` or
`Core/bridge/explorer_backend`.

## Task 1: Add resolver tests and implementation

**Files:**

- Create: `source/AstreaOS/src/Apps/Explorer/native/src/runtime/explorer_runtime_paths.h`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/runtime/explorer_runtime_paths.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_runtime_paths.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

**Interfaces:** `ExplorerRuntimePaths` and `ExplorerRuntimeResolver::resolve()` as defined above. The resolver has no `QCoreApplication` dependency and accepts all environment/anchor inputs explicitly so tests can use `QTemporaryDir` fixtures.

- [ ] **Step 1: Write the failing resolver tests.** Create a temporary runtime fixture with `Apps/Explorer/Main.qml`, `Apps/Explorer/qmldir`, `Apps/Explorer/Theme.qml`, `Core/components/Theme.qml`, `Features/Files`, and `System/i18n/I18n.qml`. Add one test per behavior: explicit root wins; explicit empty root is rejected without fallback; invalid explicit root is rejected without fallback; invalid installed root falls through; valid installed root beats development; development ancestor is found; invalid candidates report diagnostics; backend/helper/launcher paths are rooted consistently.
- [ ] **Step 2: Register the test target and run it to verify RED.** Add `runtime/explorer_runtime_paths.*` to the native library that owns application support, add `tst_runtime_paths`, link Qt Core/Test, and register `runtime_paths`. Configure the existing native build if available and run `ctest -R runtime_paths --output-on-failure`. Expected result: compile failure because the resolver files do not exist.
- [ ] **Step 3: Implement candidate validation and precedence.** Use `QFileInfo`/`QDir` only; normalize candidates with `QDir::cleanPath`. Treat `QProcessEnvironment::contains("ASTREA_ROOT")` as explicit even when its value is empty. Continue after invalid installed candidates, stop after invalid explicit candidates, and walk executable ancestors for development candidates. Do not use literal user-home or repository paths.
- [ ] **Step 4: Run the focused resolver test GREEN.** Reconfigure/build the target and run `ctest -R runtime_paths --output-on-failure`; verify every resolver test passes.
- [ ] **Step 5: Commit the isolated support slice.** Stage only the resolver files, test, and CMake lines and commit `feat(explorer): add runtime root resolver`.

## Task 2: Wire resolved paths into the backend and application loader

**Files:**

- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/backend/one_shot_cli_transport.*` only if a rooted backend option or diagnostic is needed
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_native_bootstrap.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_real_main_qml.cpp`

**Interfaces:** `ExplorerApplication::loadExplorerQml(QQmlApplicationEngine &, const ExplorerRuntimePaths &)`, `ExplorerApplication::loadBootstrap(...)` for the explicit fallback test path, and a self-test that returns zero only when the real Explorer root is created without captured QML errors.

- [ ] **Step 1: Add failing application tests.** Extend the native startup test to run the executable with `QT_QPA_PLATFORM=offscreen`, a temporary valid `ASTREA_ROOT`, and `--self-test`, asserting zero exit and output containing the resolved `Main.qml` root. Add a direct `tst_real_main_qml` that creates a `QQmlApplicationEngine`, registers a test `AppState`, loads the fixture’s real `Main.qml`, captures `QQmlApplicationEngine::warnings`, and fails on any warning/error or empty root list.
- [ ] **Step 2: Run the startup tests RED.** Run the focused CTest targets. Expected result: the binary still loads `NativeBootstrap.qml`, or the integration fixture fails because the native loader has not been connected to `Main.qml`.
- [ ] **Step 3: Implement root-aware loading.** Resolve paths before constructing the engine. If resolution is invalid, print diagnostics and return nonzero. When `ASTREA_ROOT` is absent, set it to the resolved root; when present, leave it untouched. Add the resolved import paths to the engine, connect the engine warning signal before loading, and call `engine.load(QUrl::fromLocalFile(paths.explorerMainQml))`. Keep `NativeBootstrap.qml` behind an explicit `--bootstrap` argument for fallback diagnostics. Pass `paths.backendProgram` to `OneShotCliTransport` when available.
- [ ] **Step 4: Update self-test semantics and run GREEN.** Remove the bootstrap-only `ready` requirement from the default self-test; require a real root object and no captured QML warnings/errors. Run the native startup and integration tests with the valid fixture root and verify that an invalid explicit root does not silently use the installed/development candidates.
- [ ] **Step 5: Commit the loader slice.** Stage only application, transport, CMake, and focused startup test changes and commit `feat(explorer): load real Explorer QML from native runtime`.

## Task 3: Expand and wire the native AppState contract

**Files:**

- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.h/.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.*`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_app_state_facade.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_selection_controller.cpp` only for shared facade assertions
- Create: `source/AstreaOS/src/Apps/Explorer/native/docs/APPSTATE_COMPATIBILITY.md`

**Interfaces:** Preserve existing native names and add only QML-observed compatibility members. Core properties are `fileModel`, `currentPath`, `history`, `historyIdx`, `tabs`, `activeTabIndex`, `loadingDir`, `loadError`, `remoteDirectoryActive`, `searchActive`, `searchQuery`, `selectedFile`, `selectedFiles`, `lastSelectedIndex`, `fileModelRevision`, `fileModelFilling`, `showPreview`, `viewMode`, `sortField`, `sortAsc`, `showHidden`, `foldersFirst`, `groupingEnabled`, `zoomLevel`, `autoMountDeviceIdsJson`, `sidebarFavoritesJson`, and `sidebarHiddenDefaultFavoritesJson`. Core invokables are navigation/history/tab/refresh methods, search methods, and selection methods already present in QML.

- [ ] **Step 1: Generate the contract inventory and add failing facade assertions.** Extract every `AppState.<member>` from Explorer QML, record its kind and current native owner in `APPSTATE_COMPATIBILITY.md`, and add QTest assertions for the core properties, model roles, navigation signals, selection signals, settings setters, and backend error property. Add a QML component smoke test that reads the registered native singleton.
- [ ] **Step 2: Run the focused facade test RED.** Run `ctest -R 'app_state_facade|selection_controller' --output-on-failure`; ensure each new assertion fails for a missing property or missing signal before implementation.
- [ ] **Step 3: Implement only required core projections.** Add `tabs` and breadcrumb data as QVariant-compatible read-only properties if the existing QML binds them. Delegate navigation and selection calls to their controllers. Connect model reset, navigation loading/error, selection changes, and settings changes to the exact NOTIFY signals. Do not move filesystem/backend behavior into the facade.
- [ ] **Step 4: Wire the long-lived graph.** Construct the rooted backend client, directory model, watcher, settings service, clipboard service, launch service, file-operation service, and existing focused controllers in `ExplorerApplication`; pass the existing dependencies explicitly. Register the facade as the only application-state singleton/context object exposed to QML. Keep unsupported legacy-only members documented rather than fabricating a second state machine.
- [ ] **Step 5: Run facade, model, navigation, selection, and backend failure tests GREEN.** Verify a model reset is QML-visible, navigation changes update `currentPath`/history/loading/error, selection survives a stable-path model refresh, and a fake backend failure updates `loadError`.
- [ ] **Step 6: Commit the state graph slice.** Stage only the facade, application graph, compatibility document, CMake, and native tests and commit `feat(explorer): wire native Explorer state facade`.

## Task 4: Resolve the real QML compatibility boundary

**Files:**

- Modify: `source/AstreaOS/src/Apps/Explorer/Main.qml` only if required to bind the registered native facade without visual changes
- Modify: Explorer behavior-only QML files proven necessary by the integration test
- Create: `source/AstreaOS/src/Apps/Explorer/native/docs/TRANSITIONAL_QML_STATE.md`
- Modify: `source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py` only for structural assertions required by native loading

- [ ] **Step 1: Add failing structural assertions.** Assert that the native path loads `Main.qml`, that no native production path loads only `NativeBootstrap.qml`, and record every remaining `Process {}`, Quickshell I/O, filesystem command, backend call, duplicated state machine, and synchronization timer with its owner/category in `TRANSITIONAL_QML_STATE.md`.
- [ ] **Step 2: Run the QML structural test RED where native bindings are missing.** Run the existing Explorer Python QML tests and the real-QML CTest target; capture exact unresolved properties, methods, imports, and component creation errors.
- [ ] **Step 3: Apply the smallest behavior-only bridge.** Prefer the C++ facade/context registration and existing local imports. If a QML alias/import is technically required, add only the import or call-site delegation needed to select native state; preserve all visual values and keep the legacy `AppState.qml`/fallback launcher unchanged.
- [ ] **Step 4: Run Python QML tests, native QML integration, and `qmllint` GREEN when available.** Treat missing toolchain binaries as NOT RUN and keep the exact command/result for the final report.
- [ ] **Step 5: Commit the compatibility audit and minimal QML bridge.** Use `refactor(explorer): document transitional QML ownership` only for the focused audit/bridge changes.

## Task 5: Run the full validation matrix and create the requested focused commit

**Files:**

- Modify only files already changed by Tasks 1–4 if validation exposes a real defect.
- Do not stage any unrelated root path.

- [ ] **Step 1: Configure and build Debug.** Use a build directory outside the source tree when possible, e.g. `cmake -S source/AstreaOS/src/Apps/Explorer/native -B /tmp/astrea-explorer-phase3-debug -DCMAKE_BUILD_TYPE=Debug`, then `cmake --build ...`.
- [ ] **Step 2: Run all CTest targets.** Run `ctest --test-dir ... --output-on-failure`, including resolver, startup, facade, model, navigation, selection, backend, and existing controller/parity tests. Report tests that require unavailable Rust artifacts separately.
- [ ] **Step 3: Configure and build Release.** Configure a separate Release directory and build the native target; do not reuse Debug artifacts.
- [ ] **Step 4: Run the existing Python, Rust, and QML checks.** Run Explorer Python tests, Rust `cargo test` for `source/AstreaOS/src/Core/bridge/apps/explorer` when Cargo exists, and `qmllint` over the native-loaded QML tree when available.
- [ ] **Step 5: Inspect the final diff and index boundary.** Run `git diff --check`, `git status --short`, `git diff --cached --name-only`, and verify every staged path is under the migration kit’s Phase 3 files. Confirm no `/home/agony/GitHub/Orbit/About`, sibling app, or other unrelated staged path is included.
- [ ] **Step 6: Commit exactly the implementation scope.** Stage exact implementation/test/documentation paths and commit `feat(explorer): connect native Qt runtime to Explorer UI`.
- [ ] **Step 7: Verify the commit and report evidence.** Run `git show --stat --oneline HEAD`, record Debug/Release/test/lint status as PASS/FAIL/NOT RUN, and confirm the legacy launcher/fallback files remain present.
