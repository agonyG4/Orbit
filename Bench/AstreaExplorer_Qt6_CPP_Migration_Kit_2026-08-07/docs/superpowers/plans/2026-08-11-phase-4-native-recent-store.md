# Phase 4 Native Recent Store and Asynchronous Source Loading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move Recent recording, persistence, source loading, and projection refresh from transitional QML/Python orchestration into a generation-safe native Qt implementation without changing unrelated Explorer domains.

**Architecture:** `RecentStore` will own Recent records, source parsing, retention, deduplication, atomic persistence, and background work. `RecentController` will own the current projection and translate store completions into `DirectoryEntry` vectors on the GUI thread. `NavigationController` will treat `recent://` as an asynchronous request kind, while `AppState.qml` delegates native recording/loading only when the native capability is active and retains the legacy `RecentState.qml` path as fallback.

**Tech Stack:** C++17, Qt 6.11 Core/Gui/Qml/Quick/Test, `QThreadPool`, `QSaveFile`, `QMetaObject` queued delivery, QML compatibility adapters, Python helper regression tests, existing Rust backend tests.

## Global Constraints

- The compatibility bridge remains the public `AppState.qml` contract.
- Recent projection, recording, persistence, and source loading become native-authoritative after this phase.
- All potentially expensive Recent source work must execute outside the GUI thread.
- Model mutation must remain on the model-owning GUI thread.
- Do not use unbounded `file.readAll().split('\\n')` for potentially large Recent history sources.
- Use an injectable dispatcher/executor for deterministic completion-order tests; do not use sleeps or timing-dependent assertions.
- Use `QSaveFile` or an equivalent Qt-native atomic-save mechanism.
- Preserve `finder-recents.json`, launch-history, XBEL, timestamp, Finder stale-entry, desktop-role, and restart compatibility contracts.
- RecentState.qml remains available only for legacy fallback and must not run Recent persistence processes in native runtime.
- Do not migrate paste/conflicts, file operations, archive/AppImage, trash, previews/thumbnails, devices/network, portal fallback, launch/open-with, helper actions, or scroll state.
- Do not modify Explorer visual layout, spacing, colors, typography, animation behavior, icons, delegates, or view structure.
- Preserve unrelated staged Orbit changes exactly; stage only Phase 4 files for the final commit.
- The single commit message is `feat(explorer): migrate Recent storage and loading to native C++`.

---

## File Map

### New files

- `source/AstreaOS/src/Apps/Explorer/native/src/services/recent_store.h/.cpp`: native Recent storage state, source parsing, background dispatch, generation guards, atomic persistence, and serialization.
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_recent_store.cpp`: focused store tests, injectable dispatcher, persistence/restart/format tests, thread ownership, stress fixtures, and load/save generation tests.

### Modified files

- `native/CMakeLists.txt`: compile/link `RecentStore`, add the store test, and link any required Qt Core target only.
- `native/src/controllers/recent_controller.h/.cpp`: consume `RecentStore`, expose asynchronous load/record APIs, retain projection/merge/model-facing behavior, and remove synchronous source parsing.
- `native/src/controllers/navigation_controller.h/.cpp`: add the Recent request kind, asynchronous completion slots, cancellation/invalidation, immediate current-projection publication, and stale-result guards.
- `native/src/controllers/app_state_facade.h/.cpp`: expose `loadRecent()` and `recordRecentAccess()` through the native state boundary and construct complete records from the current model when available.
- `native/src/models/directory_model.h/.cpp`: provide a C++ lookup for the current `DirectoryEntry` by path so desktop identity/roles survive native recording.
- `native/src/controllers/open_with_controller.h/.cpp`: extend the existing lookup path with one per-load desktop catalog/index rather than creating a second catalog subsystem.
- `native/src/explorer_application.cpp`: construct and wire `RecentStore`/`RecentController` with the existing Recent source paths and application lifetime.
- `Apps/Explorer/compatibility/NativeAppStateAdapter.qml`: expose native Recent methods without exposing store internals.
- `Apps/Explorer/AppState.qml`: route Recent startup/recording through native APIs when the native bridge is active, preserving legacy fallback behavior.
- `Apps/Explorer/state/RecentState.qml`: guard legacy load/save/record paths so no Recent `Process` is active or invoked in native runtime.
- `native/tests/tst_recent_controller.cpp`: switch source tests to store/controller production APIs and preserve all Phase 3 parity cases.
- `native/tests/tst_navigation_controller.cpp`: qualify asynchronous `recent://` load, stale completion, navigation-away, and immediate projection behavior.
- `native/tests/tst_app_state_facade.cpp` and/or `tst_app_state_compatibility.cpp`: qualify the native public recording/loading surface.
- `Apps/Explorer/tests/test_explorer_qml.py`: assert public AppState native routing and legacy fallback without bypassing the bridge.
- `native/docs/APPSTATE_COMPATIBILITY.md` and `native/docs/TRANSITIONAL_QML_STATE.md`: document actual Recent ownership and the remaining legacy-only role of `RecentState.qml`.

## Interfaces

The implementation will keep all cross-thread data immutable and value-based:

```cpp
struct RecentSourcePaths {
    QString finderPath;
    QString launchHistoryPath;
    QString xbelPath;
    int limit = 60;
};

struct RecentRecord {
    DirectoryEntry entry;
    qint64 lastAccessed = 0;
    QString source;
};

class RecentStore final : public QObject {
    Q_OBJECT
public:
    using Dispatch = std::function<void(std::function<void()>)>;

    explicit RecentStore(
        RecentSourcePaths paths,
        QObject *parent = nullptr,
        Dispatch dispatch = {});

    quint64 load();
    void cancelLoad(quint64 requestId);
    void recordAccess(const RecentRecord &record);
    QVector<RecentRecord> records() const;
    quint64 persistenceGeneration() const;

signals:
    void loadReady(
        quint64 requestId,
        const QVector<RecentRecord> &records,
        quintptr workerThreadId);
    void loadFailed(quint64 requestId, const QString &message);
    void saveFinished(
        quint64 generation,
        bool success,
        const QString &message,
        quintptr workerThreadId);
    void recordsChanged();
};
```

`RecentController` will expose production-facing operations:

```cpp
BackendRequestId loadAsync(const RecentSourcePaths &paths);
void cancelLoad(BackendRequestId requestId);
void recordAccess(const DirectoryEntry &entry);
QVector<DirectoryEntry> currentEntries() const;
```

It will emit `recentReady(requestId, entries)`, `recentFailed(requestId, message)`, and `projectionChanged()` only on its owning GUI thread.

### Task 1: Add failing RecentStore and dispatcher tests

**Files:**
- Create: `native/tests/tst_recent_store.cpp`
- Modify: `native/CMakeLists.txt`
- Modify: `native/src/services/recent_store.h` only when the wished-for interface is needed to compile the test

**Interfaces:**
- Consumes: existing `DirectoryEntry`, Phase 3 timestamp/Finder behavior, existing Recent source paths.
- Produces: compile-time test expectations for `RecentStore`, `RecentRecord`, the injectable `Dispatch`, and store signals used by later controller tests.

- [ ] **Step 1: Write the failing store tests first.** Cover empty state; persisted finder file records; desktop and mixed launch records; malformed lines/records; valid/malformed/missing/zero/negative timestamps; Finder missing target; deduplication/order/limit; record file and desktop metadata; immediate in-memory update before save completion; and `load A`, `load B`, complete B, complete A using a manual dispatch queue.

  The manual dispatcher must retain each submitted `std::function<void()>` in order and expose `run(index)` to complete a chosen job explicitly. Assertions must compare the published request ID and final paths, not elapsed time.

- [ ] **Step 2: Add atomic-save and restart tests before implementation.** Use a temporary directory and a valid existing `finder-recents.json`; record a newer item, run the save job, verify the file contains a complete JSON array, create a fresh store against that path, load it, and verify the newer item survives. Add a save failure case using a path whose parent cannot be created or whose target is not writable, then assert records remain unchanged and a failure signal is emitted.

- [ ] **Step 3: Add the thread ownership test before implementation.** Use the production dispatcher, start a source load, capture the worker thread ID in `loadReady`, assert it differs from the store owner thread, and assert the queued completion is observed on the store owner thread.

- [ ] **Step 4: Configure only the new test target and run it.**

  Run:

  ```bash
  cmake -S source/AstreaOS/src/Apps/Explorer/native -B build-phase4-red -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-phase4-red --target tst_recent_store
  ctest --test-dir build-phase4-red -R '^recent_store$' --output-on-failure
  ```

  Expected result: the test target does not compile or the new tests fail because `RecentStore` and its APIs do not yet exist. Fix test-only compilation errors until the failure is caused by the missing production behavior.

### Task 2: Implement RecentStore persistence, records, and bounded source parsing

**Files:**
- Create: `native/src/services/recent_store.h`
- Create: `native/src/services/recent_store.cpp`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/tst_recent_store.cpp`

**Interfaces:**
- Consumes: Task 1 tests and `DirectoryEntry`/`RecentSourcePaths` contracts.
- Produces: GUI-owned authoritative records, `load()`/`cancelLoad()`/`recordAccess()`, generation-safe background jobs, and atomic finder persistence for `RecentController`.

- [ ] **Step 1: Add the shared value types and Qt metatype declarations.** Move `RecentRecord` and `RecentSourcePaths` to the store header, keep the namespace `Astrea::Explorer::Native::Backend`, and register `RecentRecord`/`QVector<RecentRecord>` for queued delivery. Update `recent_controller.h` to include the store header rather than redefining the types.

- [ ] **Step 2: Implement the production dispatch path.** If no dispatcher is injected, submit each immutable job to `QThreadPool::globalInstance()`. The worker must only read source files, parse/normalize values, resolve desktop metadata, deduplicate/sort bounded candidates, or write a `QSaveFile`. Deliver completion to the store object with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` and include the worker thread ID for qualification.

- [ ] **Step 3: Implement streaming launch-history parsing.** Read newest-first with bounded reverse chunks. Keep only the current reverse line plus a fixed maximum record size; skip oversized/malformed lines. Parse only accepted `status == "ok"` file/desktop objects, retain timestamp fallback behavior, deduplicate accepted absolute paths, and stop after `limit` unique accepted paths.

- [ ] **Step 4: Implement bounded Finder and XBEL source handling.** Parse Finder array objects one object at a time with an incremental brace/string scanner and a maximum object buffer; preserve non-empty serialized stale identities without stat calls. Keep only the bounded highest-priority candidates needed for the final limit. Continue using `QXmlStreamReader` for XBEL and bounded candidate retention while preserving newest-access ordering.

- [ ] **Step 5: Implement native serialization and atomic saves.** Serialize only the persisted Finder-compatible records to the existing JSON array shape, retaining recognized identity, role, URL, timestamp, source, and desktop metadata fields. Ensure the parent directory exists, write a complete compact document to `QSaveFile`, flush/commit it, and report failure without modifying the authoritative in-memory vector.

- [ ] **Step 6: Implement record coalescing and generations.** On `recordAccess`, replace the existing path with the new record, put it at the newest position through access timestamp sorting, enforce `limit`, emit `recordsChanged()` immediately, and schedule a save of the newest snapshot. A save completion may only report status; it must never replace records. If multiple records arrive while a save runs, retain only the newest pending snapshot and start it after the active save finishes.

- [ ] **Step 7: Run the focused store tests and refactor only after green.**

  Run:

  ```bash
  cmake --build build-phase4-red --target tst_recent_store
  ctest --test-dir build-phase4-red -R '^recent_store$' --output-on-failure
  ```

  Expected result: all Task 1 tests pass, including explicit malformed timestamp, Finder stale target, atomic save, restart, thread, and load-order assertions.

### Task 3: Refactor RecentController and desktop lookup around the store

**Files:**
- Modify: `native/src/controllers/recent_controller.h`
- Modify: `native/src/controllers/recent_controller.cpp`
- Modify: `native/src/controllers/open_with_controller.h`
- Modify: `native/src/controllers/open_with_controller.cpp`
- Modify: `native/tests/tst_recent_controller.cpp`
- Modify: `native/CMakeLists.txt` only if target linkage changes

**Interfaces:**
- Consumes: `RecentStore` records and store completion signals.
- Produces: asynchronous `RecentController::loadAsync`, `cancelLoad`, `recordAccess`, `currentEntries`, `recentReady`, `recentFailed`, and `projectionChanged`.

- [ ] **Step 1: Rewrite controller tests against production APIs.** Replace synchronous `controller.load(paths)` source parsing calls with a store/controller fixture using the manual dispatcher. Assert controller projection merges newest duplicate records, preserves access timestamps in `fileModified`, retains desktop roles, preserves Finder stale targets, and publishes only the accepted request.

- [ ] **Step 2: Run the controller tests to establish the red state.**

  Run:

  ```bash
  cmake --build build-phase4-red --target tst_recent_controller
  ctest --test-dir build-phase4-red -R '^recent_controller$' --output-on-failure
  ```

  Expected result: the new async API is missing or the tests fail because the controller is still synchronous.

- [ ] **Step 3: Add one per-load indexed desktop catalog to the existing OpenWithController path.** Introduce a value-based catalog builder/lookup helper in `OpenWithController` that scans application roots once per source load and maps desktop IDs and file names to parsed `OpenWithApplication` values. Keep direct `.desktop` paths fast, preserve existing `resolveDesktopEntry()` callers, and do not add a second catalog subsystem.

- [ ] **Step 4: Make RecentController a GUI-thread projection owner.** It must never parse files itself. It requests store loads, rejects results whose request ID is no longer current, merges accepted source records with current in-memory records, converts them into `DirectoryEntry` values, and emits projection/results from its own thread.

- [ ] **Step 5: Implement immediate native recording.** Convert a model-enriched `DirectoryEntry` to a Finder-source `RecentRecord`, call `RecentStore::recordAccess`, rebuild the projection immediately, and emit `projectionChanged()` without waiting for persistence.

- [ ] **Step 6: Run controller and store tests together.**

  Run:

  ```bash
  cmake --build build-phase4-red --target tst_recent_store tst_recent_controller
  ctest --test-dir build-phase4-red -R '^(recent_store|recent_controller)$' --output-on-failure
  ```

### Task 4: Integrate asynchronous Recent navigation and model publication

**Files:**
- Modify: `native/src/controllers/navigation_controller.h`
- Modify: `native/src/controllers/navigation_controller.cpp`
- Modify: `native/tests/tst_navigation_controller.cpp`
- Modify: `native/tests/tst_recent_controller.cpp` only for integration fixture support

**Interfaces:**
- Consumes: `RecentController::loadAsync`, `cancelLoad`, `currentEntries`, and signals from Task 3.
- Produces: `RequestKind::Recent`, pending request identity, asynchronous `recent://` model publication, and stale navigation safety.

- [ ] **Step 1: Add failing navigation tests.** Use `FakeRustBackendClient`, a `DirectoryModel`, a real `RecentController` with a manual store dispatcher, and a watcher. Test `load A → load B → complete B → complete A`; assert only B reaches the model. Test navigation to `recent://`, then another directory before Recent completes; complete Recent and assert the directory model remains on the other directory. Test a record update while viewing `recent://`; assert the new item appears before any save job completes and `DirectoryModel::countChanged` is observed on the model thread.

- [ ] **Step 2: Run the navigation tests and verify the expected red failures.**

  Run:

  ```bash
  cmake --build build-phase4-red --target tst_navigation_controller
  ctest --test-dir build-phase4-red -R '^navigation_controller$' --output-on-failure
  ```

- [ ] **Step 3: Add Recent request state.** Extend `RequestKind`, store the Recent request ID/generation/path in `m_pendingRequests`, and connect the controller's ready/failed/projection signals with queued delivery. `startList("recent://")` must apply the last known projection, set loading, issue `loadAsync`, and return the Recent request ID without parsing sources synchronously.

- [ ] **Step 4: Add stale cancellation and completion guards.** `cancelActiveRequest()` must invalidate Recent requests through `RecentController::cancelLoad`. The Recent completion slot must require active request ID, navigation generation, current path, and Recent request identity before applying the result. A failure must preserve last-known projection and set a diagnostic error instead of clearing valid data.

- [ ] **Step 5: Publish immediate in-memory projection updates.** When the controller reports `projectionChanged()` and the current path is `recent://`, apply the current projection on the GUI thread without waiting for disk persistence. Do not cancel an active source request merely because memory changed; its accepted completion must merge source data with newer in-memory records.

- [ ] **Step 6: Run the navigation, store, and existing native test set.**

  Run:

  ```bash
  cmake --build build-phase4-red
  ctest --test-dir build-phase4-red --output-on-failure
  ```

### Task 5: Expose native Recent APIs and disable native-runtime QML ownership

**Files:**
- Modify: `native/src/controllers/app_state_facade.h`
- Modify: `native/src/controllers/app_state_facade.cpp`
- Modify: `native/src/models/directory_model.h`
- Modify: `native/src/models/directory_model.cpp`
- Modify: `native/src/explorer_application.cpp`
- Modify: `Apps/Explorer/compatibility/NativeAppStateAdapter.qml`
- Modify: `Apps/Explorer/AppState.qml`
- Modify: `Apps/Explorer/state/RecentState.qml`
- Modify: `native/tests/tst_app_state_facade.cpp`
- Modify: `native/tests/tst_app_state_compatibility.cpp`
- Modify: `Apps/Explorer/tests/test_explorer_qml.py`

**Interfaces:**
- Consumes: `RecentController::recordAccess/loadAsync`, `DirectoryModel::entryForPath`, and existing native capability boundary.
- Produces: `NativeAppState.recordRecentAccess(path, isDir, fileUrl)` and `NativeAppState.loadRecent()` with legacy-compatible public `AppState` delegation.

- [ ] **Step 1: Add failing public-API tests.** Assert `AppStateFacade::recordRecentAccess` rejects empty/recent/trash/dialog paths, enriches existing model entries including desktop name/icon/kind, and updates Recent memory immediately. Assert `loadRecent()` is callable and `recent://` projection flows through the public facade. Update Python/QML assertions to require native routing under `nativeNavigationActive`, legacy `recent.recordAccess` otherwise, and no native-runtime invocation of `RecentState` save/load processes.

- [ ] **Step 2: Run the facade/QML tests to verify red behavior.**

  Run:

  ```bash
  cmake --build build-phase4-red --target tst_app_state_facade tst_app_state_compatibility
  ctest --test-dir build-phase4-red -R '^(app_state_facade|app_state_compatibility)$' --output-on-failure
  python3 -m unittest discover -s source/AstreaOS/src/Apps/Explorer/tests -p 'test_explorer_qml.py'
  ```

- [ ] **Step 3: Wire RecentController and store into ExplorerApplication.** Construct the store with the existing home-based Finder/launch/XBEL paths and limit, pass the controller into NavigationController and AppStateFacade, and preserve object lifetimes under `QGuiApplication`.

- [ ] **Step 4: Add the facade methods and model lookup.** `recordRecentAccess` must copy the current model entry when available, override the URL when supplied, use a filesystem-derived fallback record otherwise, and delegate to RecentController. `loadRecent` must request an asynchronous source load without creating any QML process.

- [ ] **Step 5: Route the public QML contract.** In native mode, `AppState.qml` calls `nativeAppState.loadRecent()` during deferred startup and `nativeAppState.recordRecentAccess()` from `recordRecentItem`. In fallback mode it continues to call `RecentState.qml`. Keep `recentModelItems()` meaningful only for legacy NavigationState.

- [ ] **Step 6: Guard RecentState.qml for fallback-only ownership.** Preserve its existing behavior when `nativeNavigationActive` is false. In native mode, `load`, `recordAccess`, `persist`, `startSave`, and process completion handlers must return without work; `loadProc` and `saveProc` must remain stopped and must not be assigned native commands. Leave unrelated QML `Process` objects untouched.

- [ ] **Step 7: Update ownership documentation and run integration tests.** Record that RecentController/RecentStore are native-authoritative, that RecentState is legacy fallback only, and that all model publication is GUI-thread-owned.

### Task 6: Full validation, smoke qualification, and focused commit

**Files:**
- Modify: only Phase 4 implementation, test, documentation, and plan files listed above.
- No generated files, build directories, archives, or unrelated staged paths.

- [ ] **Step 1: Capture validation baselines before the final build.** Record the existing QML lint command/scope and warning count, current `git status --short`, and all available Qt/Python/Rust tool versions in the final report. Use separate clean Debug and Release build directories outside the source tree if possible.

- [ ] **Step 2: Run clean Debug and Release builds plus all CTest tests.**

  Run:

  ```bash
  cmake -S source/AstreaOS/src/Apps/Explorer/native -B build-phase4-debug -DCMAKE_BUILD_TYPE=Debug
  cmake --build build-phase4-debug
  ctest --test-dir build-phase4-debug --output-on-failure
  cmake -S source/AstreaOS/src/Apps/Explorer/native -B build-phase4-release -DCMAKE_BUILD_TYPE=Release
  cmake --build build-phase4-release
  ctest --test-dir build-phase4-release --output-on-failure
  ```

- [ ] **Step 3: Run Python, Rust, QML lint, and source checks.** Run the repository's existing Python test command, the Explorer Rust test command discovered from the source tree, the same QML lint scope before/after, and:

  ```bash
  git diff --check
  rg -n 'Process|recordAccess|persist|history.jsonl|merged-recents' source/AstreaOS/src/Apps/Explorer/state/RecentState.qml source/AstreaOS/src/Apps/Explorer/AppState.qml source/AstreaOS/src/Apps/Explorer/compatibility/NativeAppStateAdapter.qml
  ```

  Manually confirm all matching Recent process paths are guarded as legacy-only and that no unrelated process domain changed.

- [ ] **Step 4: Attempt runtime smoke.** Run the native bootstrap/self-test and the real Explorer startup path with the repository's available offscreen environment. Attempt Recent navigation, immediate native recording, desktop recording, restart/reload, and repeated refresh. If `quickshell-ioplugin` is unavailable, report that real `Main.qml` was reached, automated qualification separately, and the visual-smoke limitation separately.

- [ ] **Step 5: Review scope before staging.** Inspect `git status --short`, `git diff --stat`, and `git diff --name-only`. Add only the Phase 4 plan, source, tests, QML compatibility, and documentation files. Verify no unrelated staged or unstaged user changes are reset, unstaged, reformatted, or included.

- [ ] **Step 6: Inspect the exact staged diff and commit once.**

  ```bash
  git diff --cached --check
  git diff --cached --stat
  git diff --cached --name-only
  git commit -m "feat(explorer): migrate Recent storage and loading to native C++"
  git rev-parse HEAD
  ```

- [ ] **Step 7: Re-run post-commit status and report only verified facts.** Confirm the focused commit hash, exact changed files, unrelated repository changes preserved, test counts/results, lint counts before/after, smoke status, and any external blockers.

## Self-review

- Source parsing, persistence, generation handling, model-thread ownership, QML routing, desktop lookup, timestamp/Finder compatibility, restart behavior, failure semantics, stress coverage, docs, lint, builds, smoke, and repository safety each have an explicit task.
- No task introduces another migration domain or changes the visual QML structure.
- `RecentStore` owns storage and source work; `RecentController` owns projection; `NavigationController` owns navigation request identity; `DirectoryModel` is only mutated from the GUI thread.
- All production code is preceded by focused failing tests in the task order.
- No intermediate commit is created so the user-requested single focused commit remains the only Phase 4 commit.
