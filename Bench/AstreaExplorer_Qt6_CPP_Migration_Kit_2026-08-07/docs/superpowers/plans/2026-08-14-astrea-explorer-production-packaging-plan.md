# Astrea Explorer Production Packaging and Clean-Install Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a clean CMake configure/build/install produce a self-contained native Explorer runtime with correct XDG MIME/desktop identity, nonblocking backend startup, asynchronous portal lifecycle, and isolated qualification evidence.

**Architecture:** CMake builds the existing Explorer and both Rust Cargo binaries into the build tree, installs them plus the tracked launch provider into one `<prefix>/share/Astrea` root, and makes required artifacts hard dependencies. C++ owns runtime capability validation, desktop identity, MIME persistence, and an event-driven worker; Rust owns per-request portal state and async dialog processes. Existing QML presentation and native ownership remain unchanged.

**Tech Stack:** CMake 3.21, Qt 6.11/C++17, Qt Test/CTest, Rust edition 2024, Tokio, zbus 5, Python `unittest`, Bash qualification harness.

## Global Constraints

- Do not migrate additional QML domains or redesign Explorer UI.
- Do not reintroduce Quickshell or production `Process {}` nodes.
- The tracked `source/AstreaOS/src/bin/astrea-launch` is the required existing launch provider; no `System/launch` source crate exists in this checkout.
- Production runtime root is `${CMAKE_INSTALL_PREFIX}/share/Astrea`, overridden at runtime by `ASTREA_ROOT`.
- Rust backend and portal must be built by the canonical CMake build; a manually prebuilt or stale binary is not an accepted install dependency.
- Required components fail configure/build/install clearly when their source or artifact is missing.
- `mimeapps.list` is parsed and atomically written by `MimeAppsService`, never by `QSettings`.
- Portal child stdout, stderr, and result data are bounded; every child is terminated/reaped on cancellation and shutdown.
- Existing dirty generated files and the deleted source-only zip are unrelated changes and must remain untouched.

---

### Task 1: Make CMake own the production artifact closure

**Files:**
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Modify: `source/AstreaOS/src/System/services/astrea-services.sh`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_runtime_paths.cpp`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_native_bootstrap.cpp`

**Interfaces:**
- Produces `astrea_explorer_rust_components`, `ASTREA_EXPLORER_BACKEND_ARTIFACT`, and `ASTREA_EXPLORER_PORTAL_ARTIFACT` for native build/install logic.
- Installs `bin/astrea-launch`, `bin/astrea-filechooser-portal`, and `Core/bridge/apps/explorer_backend` under `share/Astrea`.
- Keeps `ExplorerRuntimePaths::backendProgram` and `launcherProgram` pointed at those installed paths.

- [ ] **Step 1: Write the artifact-closure regression test.** Add a CTest/Python assertion that configures a fresh build without `-DASTREA_EXPLORER_BACKEND_PROGRAM`, then checks the install manifest for the backend, portal, and launch provider.

```python
required = {
    "share/Astrea/Core/bridge/apps/explorer_backend",
    "share/Astrea/bin/astrea-filechooser-portal",
    "share/Astrea/bin/astrea-launch",
}
assert required <= installed_relative_paths
```

- [ ] **Step 2: Run the focused packaging assertion before implementation.** Run `python3 -m unittest` for the new harness test and confirm it fails because the current install only includes a supplied backend and no canonical portal/launch install target.

- [ ] **Step 3: Add deterministic Cargo custom commands.** In `native/CMakeLists.txt`, define the Explorer and portal manifest/source roots, use a build-tree-specific `CARGO_TARGET_DIR`, select `--release` for `Release`/`RelWithDebInfo` and no profile flag for `Debug`, declare manifest/lock/source dependencies, and make `astrea-explorer` depend on the resulting artifacts. Remove `ASTREA_EXPLORER_BACKEND_PROGRAM`.

- [ ] **Step 4: Install all required artifacts.** Install the CMake executable to `bin`, copy the Cargo outputs to their canonical runtime locations, install the tracked executable source `src/bin/astrea-launch` to runtime `bin`, and use the built backend artifact for shadow-parity tests.

- [ ] **Step 5: Make service metadata ordering strict.** Update `astrea-services.sh` so `install_portal_binary` builds/copies the canonical portal before `write_portal_files`; missing launch or portal source/artifacts returns nonzero and does not write/enable metadata. Preserve optional-service fallback semantics for unrelated services.

- [ ] **Step 6: Run the focused CMake configure/build/install test.** Use a fresh temporary build and install prefix, without `ASTREA_EXPLORER_BACKEND_PROGRAM`, then assert executable permissions and all required paths. Commit as `build(explorer): make native runtime dependencies reproducible`.

### Task 2: Split runtime capabilities and harden launch/desktop identity

**Files:**
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/runtime/explorer_runtime_paths.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/runtime/explorer_runtime_paths.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/services/launch_service.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/services/launch_service.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.cpp`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_runtime_paths.cpp`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_launch_service.cpp`
- Test: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_open_with_controller.cpp`

**Interfaces:**
- `ExplorerRuntimePaths` exposes `resourceRootValid`, `backendAvailable`, `launchAvailable`, `windowsRunnerAvailable`, `normalRuntimeReady`, and `portalRuntimeReady` (or equivalent typed state).
- `DesktopCatalog` keys use application-root-relative desktop-file IDs, including nested IDs such as `foo/bar.desktop`.
- `LaunchService` rejects a missing required program before calling `QProcess::startDetached` and returns an actionable typed error.

- [ ] **Step 1: Add failing capability tests.** Cover resource-only roots, normal-mode readiness with backend/launch present, portal-mode readiness without Windows runner, explicit-root diagnostics, and missing launch/backend errors.

- [ ] **Step 2: Add failing desktop-ID/preference tests.** Create temporary XDG user/system application roots containing `foo.desktop` and `foo/bar.desktop`; assert nested IDs and user-root precedence.

- [ ] **Step 3: Implement capability validation.** Keep QML/resource checks separate from executable checks, derive required runtime state without development-path fallback hiding missing install artifacts, and make application startup choose normal vs portal requirements before constructing controllers.

- [ ] **Step 4: Implement shared desktop identity.** Add a focused utility/service that computes the ID from the relative path below each `applications` root, parses desktop entries, and inserts the first effective XDG entry only. Reuse it from `OpenWithController` and default lookup.

- [ ] **Step 5: Harden launch specs and process start.** Preserve argv boundaries, require existing executable providers, and return `missing_launcher`/`invalid_launch_spec`/`launch_failed` errors instead of empty commands.

- [ ] **Step 6: Run CTest targets `runtime_paths`, `launch_service`, and `open_with_controller`; commit as `fix(explorer): validate runtime capabilities and desktop identity`.

### Task 3: Introduce `MimeAppsService` with freedesktop persistence

**Files:**
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.h`
- Create: `source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_mime_apps_service.cpp`

**Interfaces:**
- `MimeAppsService(QString configPath = {})` resolves the writable XDG config path.
- `QStringList defaultsForMime(const QString &mime) const` reads `[Default Applications]` semicolon lists.
- `QStringList associationsForMime(const QString &mime) const` reads `[Added Associations]` semicolon lists.
- `bool setDefault(const QString &mime, const QString &desktopId)` acquires `QLockFile`, re-reads, prepends/deduplicates the canonical ID, updates associations, and atomically commits with `QSaveFile`.

- [ ] **Step 1: Write failing parser/round-trip tests.** Cover `text/plain`, `application/pdf`, multiple defaults, fallback preservation, Added Associations, unrelated sections, malformed/empty files, and a new service instance reading a prior write.

- [ ] **Step 2: Write failing concurrency and path tests.** Set temporary `XDG_CONFIG_HOME`, verify the chosen path, and hold the lock in a second process/object to assert deterministic failure rather than clobbering.

- [ ] **Step 3: Implement a line-preserving INI model.** Parse sections and key/value lines without `QSettings`; normalize only the targeted semicolon-list key, preserve unrelated lines, and treat malformed lines as opaque content.

- [ ] **Step 4: Implement locked atomic update.** Ensure the parent directory exists, acquire `QLockFile`, re-read current content, update default/association lists with canonical `.desktop` IDs, write through `QSaveFile`, and release the lock on every path.

- [ ] **Step 5: Wire all consumers.** Remove `QSettings` MIME reads/writes from `OpenWithController` and `AppStateFacade`; use `MimeAppsService` for discovery, selected-default state, and default updates.

- [ ] **Step 6: Run `tst_mime_apps_service`, `tst_open_with_controller`, and `tst_app_state_facade`; commit as `fix(explorer): implement freedesktop MIME associations`.

### Task 4: Make persistent worker startup event-driven

**Files:**
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/backend/persistent_worker_transport.h`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/backend/persistent_worker_transport.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_backend_client.cpp`
- Create: `source/AstreaOS/src/Apps/Explorer/native/tests/tst_persistent_worker_transport.cpp`

**Interfaces:**
- Internal states are `Stopped`, `Starting`, `Running`, `Failed`, and `Stopping`.
- `start()` allocates an ID, queues the request, starts `QProcess` asynchronously, and returns immediately.
- Startup timeout is a `QTimer`; queued requests flush on `QProcess::started`; failure/cancellation emits exactly one terminal event.

- [ ] **Step 1: Add failing startup tests.** Use a fake executable that delays startup and assert `start()` returns promptly, two requests queue, startup timeout fails both once, and cancellation while starting produces only `cancelled`.

- [ ] **Step 2: Add failing restart/shutdown tests.** Exercise child exit, bounded restart, request timeout, and destructor cleanup with no `waitForStarted(2000)` call.

- [ ] **Step 3: Implement the state machine.** Add pending request arguments, startup timer, bounded restart counter, and `flushPendingRequests()`; connect `started`, `errorOccurred`, `finished`, and timers without blocking calls.

- [ ] **Step 4: Preserve protocol behavior.** Keep JSONL parsing, line bounds, per-request timers, cancellation frames, and exactly-once terminal signals while adapting failure handling to queued requests.

- [ ] **Step 5: Run backend/client/transport tests and a real `serve` smoke; commit as `fix(explorer): make backend startup nonblocking`.

### Task 5: Rebuild the portal around async per-handle request ownership

**Files:**
- Modify: `source/AstreaOS/src/System/portal/Cargo.toml`
- Modify: `source/AstreaOS/src/System/portal/src/lib.rs`
- Modify: `source/AstreaOS/src/System/portal/src/main.rs`
- Create or modify: `source/AstreaOS/src/System/portal/tests/portal_lifecycle.rs`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.cpp`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.cpp`

**Interfaces:**
- `PortalRequestRegistry` keys active requests by `OwnedObjectPath` and enforces a fixed active-request limit.
- Every request exports `org.freedesktop.impl.portal.Request` at its supplied path with idempotent `Close()`.
- `DialogRunner::run(...)` uses `tokio::process::Command`, bounded output/result reads, and cancellation/timeout selection; it always reaps the child.

- [ ] **Step 1: Add failing lifecycle tests.** Cover supplied-handle export/removal, `Close()` before start and during execution, close-vs-success races, timeout, crash, multiple concurrent handles, active-request rejection, oversized output/result, OpenFile, SaveFile, and SaveFiles.

- [ ] **Step 2: Extend Tokio features and introduce bounded runner types.** Add the minimum `sync`/`io-util`/`macros` features needed for cancellation and async bounded reads; keep the existing result schema and compatibility environment variables.

- [ ] **Step 3: Implement async dialog execution.** Spawn `--portal` with `tokio::process::Command`, await child completion/result once, bound stdout/stderr/result bytes, and select completion, cancellation, and timeout.

- [ ] **Step 4: Implement request object lifecycle.** Store `app_id`, `parent_window`, `handle`, title, and options; export a Request interface at the exact handle; make `Close()` cancel and terminate the child; remove the object after terminal response.

- [ ] **Step 5: Convert `OpenFile`, `SaveFile`, and `SaveFiles` to async zbus methods.** Keep D-Bus executor responsiveness, preserve result/cancel semantics, and route every outcome through one exactly-once completion path.

- [ ] **Step 6: Run `cargo test --manifest-path source/AstreaOS/src/System/portal/Cargo.toml` and private-bus lifecycle tests; commit as `fix(portal): implement async request lifecycle`.

### Task 6: Add the clean-install harness, strengthen the migration gate, and update truthfulness docs

**Files:**
- Create: `scripts/qualify_clean_install.sh`
- Modify: `scripts/verify_native_migration_gate.py`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/docs/FINAL_NATIVE_MIGRATION_QUALIFICATION.md`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/docs/FINAL_NATIVE_MIGRATION_JOURNAL.md`
- Modify: `docs/10_BUILD_PACKAGING_INTEGRATION.md`
- Modify: `docs/09_FILECHOOSER_PORTAL_MIGRATION.md`
- Modify: `source/AstreaOS/src/Apps/Explorer/native/tests/CMakeLists.txt`

**Interfaces:**
- `qualify_clean_install.sh` accepts optional build type/prefix arguments, creates temporary HOME/XDG roots, performs clean configure/build/install, and exits nonzero on any missing artifact or smoke failure.
- The migration gate rejects source patterns for QSettings MIME writers, discarded FileChooser handles, blocking portal loops, `waitForStarted(2000)`, and optional-only required installs.

- [ ] **Step 1: Write the harness assertions first.** Verify executable permissions and the installed inventory: Explorer, backend, portal, launch provider, Explorer QML, AstreaFiles, AstreaI18n, AstreaComponents, translations, and portal metadata.

- [ ] **Step 2: Implement isolated runtime smoke.** Run installed `astrea-explorer --self-test` and `--portal --self-test` with `ASTREA_ROOT` set to the temporary runtime and no developer-specific PATH/import markers.

- [ ] **Step 3: Add MIME, launch, and portal qualification.** Create temporary desktop files, set/read defaults through native code, run actual launch-provider boundary checks, and exercise a private D-Bus Request/Close/second-request sequence where the environment supports it.

- [ ] **Step 4: Strengthen semantic gate checks.** Keep textual checks limited to forbidden dependency/install markers and use the unit/integration suites for MIME, portal, worker, and runtime semantics.

- [ ] **Step 5: Correct existing qualification claims.** Distinguish source-install/manual-dependency evidence from clean-install evidence, document the tracked launch provider and canonical artifact layout, document portal cancellation/bounds, and record only freshly verified PASS results.

- [ ] **Step 6: Run the complete validation matrix.** From fresh Debug and Release build directories, run CTest, Rust backend tests, portal tests, relevant Python tests, QML lint, migration gate, `git diff --check`, and the isolated harness. Commit as `test(explorer): qualify clean native installation`.

### Final verification checklist

- [ ] `rtk git diff --check` returns no output.
- [ ] `rtk git status --short` shows only intended Explorer/docs changes plus the pre-existing unrelated dirty files.
- [ ] Clean Debug and Release configure/build/install do not use `ASTREA_EXPLORER_BACKEND_PROGRAM`.
- [ ] Installed normal and portal self-tests pass with temporary HOME/XDG roots and `ASTREA_ROOT`.
- [ ] All changed tests pass with fresh output; no completion claim is made without the command output.

