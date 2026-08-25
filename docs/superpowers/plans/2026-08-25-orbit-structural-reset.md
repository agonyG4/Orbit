# Orbit Structural Reset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote the native Explorer closure into a clean Orbit monorepo, retire the migration kit, and preserve Explorer behavior and the qualified Astrea runtime installation contract.

**Architecture:** The repository root owns CMake and the Rust workspace. Explorer owns its QML, C++, backend, tests, and docs under `apps/explorer`; shared QML lives under `shared/qml/Astrea`; Rust services live under `services`. CMake stages a development `runtime/Astrea` tree while installation continues to map to the existing Astrea runtime paths.

**Tech Stack:** CMake 3.21+, Qt 6.11, C++17, QML/Qt Quick, Rust edition 2024 with Cargo workspace resolver 3, Python unittest/static gates, Unix install/runtime qualification.

## Global Constraints

- Preserve Explorer UI, visual fidelity, behavior, CLI, backend JSON/protocol contracts, D-Bus lifecycle, launch behavior, persistence, and file-operation semantics.
- Preserve Rust package names `explorer_backend`, `astrea-launch`, and `astrea-filechooser-portal` and their dependency versions.
- Preserve installed destinations under `<prefix>/share/Astrea/{Apps/Explorer,Core/components,Features/Files,System/i18n,Core/bridge/apps,bin,System/services}`.
- Keep C++17 and Qt 6.11 as the minimum policy used by the current checkout.
- Do not retain `Bench/`, `Old/`, `source/AstreaOS/`, `apps/explorer/native/`, generated output, or active-source QML compatibility symlinks.
- Do not use destructive repository-wide reset/clean operations; preserve unrelated work.
- Use `git mv` for tracked source moves and `apply_patch` for file edits.

---

### Task 1: Create the canonical root workspace skeleton

**Files:**
- Create: `CMakeLists.txt`
- Create: `Cargo.toml`
- Create: `CMakePresets.json`
- Create: `cmake/OrbitRust.cmake`
- Modify: `.gitignore`

**Interfaces:**
- Produces root CMake targets and `debug`/`release` presets that later tasks wire to `apps/explorer`.
- Produces a Cargo workspace whose active members are `apps/explorer/backend`, `services/launch`, and `services/filechooser-portal`.

- [ ] **Step 1: Add the root CMake policy.**

```cmake
cmake_minimum_required(VERSION 3.21)
project(Orbit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.11)
enable_testing()

include(GNUInstallDirs)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/OrbitRust.cmake)
add_subdirectory(apps/explorer)
```

- [ ] **Step 2: Add the root Cargo workspace.**

```toml
[workspace]
resolver = "3"
members = [
    "apps/explorer/backend",
    "services/launch",
    "services/filechooser-portal",
]
```

- [ ] **Step 3: Add reusable debug/release presets.**

Define `debug` and `release` configure presets with binary directories `${sourceDir}/build/debug` and `${sourceDir}/build/release`, matching build/test presets named `debug` and `release`.

- [ ] **Step 4: Add the focused Rust helper interface.**

Implement `orbit_add_rust_package(TARGET PACKAGE MANIFEST_DIR ARTIFACT)` so it selects `debug` or `release`, sets `CARGO_TARGET_DIR` to `${CMAKE_CURRENT_BINARY_DIR}/cargo-target`, runs `cargo build --workspace --locked --package ${PACKAGE}`, and exposes the artifact as a custom command dependency. Keep the helper limited to package selection, profile, target directory, artifact path, dependencies, and command execution.

- [ ] **Step 5: Extend ignore rules for generated Orbit trees.**

Add `build/`, `build/debug/`, `build/release/`, `runtime/`, and Cargo output patterns without removing the existing cache/compiler protections.

- [ ] **Step 6: Verify the skeleton parses.**

Run: `cmake -S . -B /tmp/orbit-root-skeleton -DCMAKE_BUILD_TYPE=Debug`

Expected: configuration may fail only because `apps/explorer/CMakeLists.txt` is not present yet; syntax and workspace files are otherwise readable.

- [ ] **Step 7: Commit the root skeleton.**

```bash
rtk git add CMakeLists.txt Cargo.toml CMakePresets.json cmake/OrbitRust.cmake .gitignore
rtk git commit -m "build: add Orbit root build and Rust workspace"
```

### Task 2: Promote the active source with history-preserving moves

**Files:**
- Create: `apps/explorer/qml/` from `Bench/.../source/AstreaOS/src/Apps/Explorer/`
- Create: `apps/explorer/src/` from `Bench/.../source/AstreaOS/src/Apps/Explorer/native/src/`
- Create: `apps/explorer/tests/cpp/` from `Bench/.../source/AstreaOS/src/Apps/Explorer/native/tests/`
- Create: `apps/explorer/tests/qml/test_explorer_qml.py` from `Bench/.../source/AstreaOS/src/Apps/Explorer/tests/test_explorer_qml.py`
- Create: `apps/explorer/docs/` from relevant files in `Bench/.../source/AstreaOS/src/Apps/Explorer/native/docs/`
- Create: `apps/explorer/backend/` from `Bench/.../source/AstreaOS/src/Core/bridge/apps/explorer/`
- Create: `shared/qml/Astrea/Components/` from `Bench/.../source/AstreaOS/src/Core/components/`
- Create: `shared/qml/Astrea/Files/` from `Bench/.../source/AstreaOS/src/Features/Files/`
- Create: `shared/qml/Astrea/I18n/` from `Bench/.../source/AstreaOS/src/System/i18n/`
- Create: `services/launch/` from `Bench/.../source/AstreaOS/src/System/launch/`
- Create: `services/filechooser-portal/` from `Bench/.../source/AstreaOS/src/System/portal/`
- Create: `services/session/astrea-services.sh` from `Bench/.../source/AstreaOS/src/System/services/astrea-services.sh`

**Interfaces:**
- Produces the final source ownership paths used by Tasks 3–7.
- Keeps C++ internal directories `backend`, `controllers`, `models`, `runtime`, and `services` unchanged.

- [ ] **Step 1: Create destination parents without creating compatibility aliases.**

```bash
mkdir -p apps/explorer/{qml,src,tests/{cpp,qml,parity},docs,backend} shared/qml/Astrea services/{launch,filechooser-portal,session} cmake scripts
```

- [ ] **Step 2: Move Explorer QML and local tests.**

Use `git mv` for QML/state/components/compatibility files into `apps/explorer/qml`, `native/src` into `apps/explorer/src`, and `native/tests` into `apps/explorer/tests/cpp`. Move `test_explorer_qml.py` to `apps/explorer/tests/qml/`.

- [ ] **Step 3: Move Explorer documentation that remains present-tense useful.**

Move `ICON_THEME_INTEGRATION.md`, `OPERATION_PROGRESS_ARCHITECTURE.md`, and `APPSTATE_COMPATIBILITY.md` into `apps/explorer/docs`. Do not carry `FINAL_NATIVE_MIGRATION_JOURNAL.md`, `FINAL_NATIVE_MIGRATION_QUALIFICATION.md`, or `TRANSITIONAL_QML_STATE.md` unchanged.

- [ ] **Step 4: Move the Rust and shared-QML closures.**

Move the backend crate, launch crate, portal crate, shared modules, and session script to their destination paths. Keep the crate source and tests unchanged at this stage.

- [ ] **Step 5: Move `NativeBootstrap.qml` into the Explorer QML hierarchy.**

Place it at `apps/explorer/qml/native/NativeBootstrap.qml`; this directory is a QML grouping only and is not a source-layout distinction.

- [ ] **Step 6: Move parity fixtures with the C++ tests.**

Place `native/tests/parity` at `apps/explorer/tests/parity` and update only path references required by the new test layout.

- [ ] **Step 7: Verify no moved file is missing.**

Run: `rtk git status --short` and `rtk find apps/explorer -type f`, then compare counts against the pre-move inventory. Expected: all active source files have one destination and no active source symlinks have been created.

- [ ] **Step 8: Commit the source promotion.**

```bash
rtk git add apps shared services
rtk git commit -m "refactor: promote Orbit native application sources"
```

### Task 3: Build Explorer from the promoted paths

**Files:**
- Create: `apps/explorer/CMakeLists.txt` from the former native CMake target definition
- Modify: `apps/explorer/src/explorer_application.cpp`
- Modify: `apps/explorer/src/explorer_application.h`
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.cpp`
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.h`
- Modify: `apps/explorer/tests/cpp/CMakeLists.txt`
- Modify: all moved C++ tests that contain source-root path macros

**Interfaces:**
- Consumes: root CMake settings and `orbit_add_rust_package`.
- Produces: `astrea-explorer`, existing native static libraries, CTest targets, and required Rust artifact dependencies.

- [ ] **Step 1: Copy the former target graph into `apps/explorer/CMakeLists.txt`.**

Preserve target names, source ownership libraries, Qt links, test names, compile definitions, and test commands. Replace all source-root calculations with `${CMAKE_CURRENT_SOURCE_DIR}/src`, `${CMAKE_CURRENT_SOURCE_DIR}/qml`, `${CMAKE_CURRENT_SOURCE_DIR}/backend`, `${CMAKE_SOURCE_DIR}/shared/qml`, and `${CMAKE_SOURCE_DIR}/services`.

- [ ] **Step 2: Replace three repeated Cargo commands with the helper.**

Register `explorer_backend`, `astrea-launch`, and `astrea-filechooser-portal` using the root workspace manifest and set artifacts under `${CMAKE_CURRENT_BINARY_DIR}/cargo-target/$<CONFIG>`.

- [ ] **Step 3: Register the Explorer QML module from `qml/native/NativeBootstrap.qml`.**

Keep the existing `Astrea.Explorer.Native` URI and file behavior so `NativeAppStateAdapter.qml` remains contract-compatible.

- [ ] **Step 4: Update test compile definitions.**

Use `ASTREA_EXPLORER_BIN` for the current target, `ASTREA_EXPLORER_NATIVE_SOURCE_ROOT` for `apps/explorer/src`, and a generated runtime root variable for tests that load QML. Remove every `source/AstreaOS` and `Apps/Explorer/native` string from active build/test files.

- [ ] **Step 5: Configure and compile the first promoted Debug build.**

Run: `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build/debug --target astrea-explorer -j2`

Expected: configuration and target compilation succeed before runtime staging is fully enabled; any failure is fixed in the target graph, not by restoring the old source tree.

- [ ] **Step 6: Commit the promoted build graph.**

```bash
rtk git add apps/explorer
rtk git commit -m "build: compile Explorer from canonical sources"
```

### Task 4: Convert shared QML into real modules

**Files:**
- Modify: `shared/qml/Astrea/Components/qmldir`
- Modify: `shared/qml/Astrea/Files/qmldir`
- Modify: `shared/qml/Astrea/I18n/qmldir`
- Modify: all active `apps/explorer/qml/**/*.qml` imports
- Modify: `apps/explorer/tests/qml/test_explorer_qml.py`
- Modify: `apps/explorer/CMakeLists.txt`

**Interfaces:**
- Produces: importable modules `Astrea.Components`, `Astrea.Files`, and `Astrea.I18n` with preserved public wrappers and drag/drop behavior.

- [ ] **Step 1: Change module identifiers.**

Set the first line of the module files to `module Astrea.Components`, `module Astrea.Files`, and `module Astrea.I18n`. Preserve all existing QML public wrapper entries and add a `DragDropSupport` JavaScript module entry in `Astrea.Files` so active code can import it without a path alias.

- [ ] **Step 2: Replace symlink-relative imports.**

Change `AstreaComponents` imports to `import Astrea.Components 1.0 as UI` or `as Components`, `AstreaFiles` imports to `import Astrea.Files 1.0 as AstreaFiles`, and `AstreaI18n` imports to `import Astrea.I18n 1.0 as AstreaI18n`. Replace direct `DragDropSupport.js` imports with the module's exported script namespace while preserving all calls and aliases.

- [ ] **Step 3: Add shared module import paths to the engine and build.**

Add `${CMAKE_SOURCE_DIR}/shared/qml` to development QML import paths and stage/install the modules into the stable runtime directories. Do not add any symlink or Quickshell path.

- [ ] **Step 4: Update QML tests to use canonical paths.**

Replace fixture reads through `AstreaFiles` symlinks with `shared/qml/Astrea/Files/DragDropSupport.js` and update root constants to `apps/explorer/qml`.

- [ ] **Step 5: Run the Python/QML contract suite.**

Run: `rtk pytest apps/explorer/tests/qml -q`

Expected: all moved tests pass and no test refers to `Bench/` or `source/AstreaOS`.

- [ ] **Step 6: Run a QML import/load check.**

Run the canonical Debug CTest `native_bootstrap`, `real_main_qml`, and `qml_warning_lifetime` tests after the staging target exists. Expected: no import warnings, missing-module errors, or dangling symlink errors.

- [ ] **Step 7: Commit the module conversion.**

```bash
rtk git add apps/explorer shared/qml
rtk git commit -m "refactor: replace source QML symlinks with modules"
```

### Task 5: Implement deterministic development runtime staging

**Files:**
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.h`
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.cpp`
- Modify: `apps/explorer/tests/cpp/tst_runtime_paths.cpp`
- Modify: `apps/explorer/src/explorer_application.cpp`
- Modify: `apps/explorer/CMakeLists.txt`

**Interfaces:**
- Consumes: canonical source locations and CMake staging target.
- Produces: `build/<config>/runtime/Astrea` and deterministic `ExplorerRuntimeResolver` behavior.

- [ ] **Step 1: Write the failing development-staging resolver test.**

Add a test that creates an executable path below a fixture `build/debug/bin`, passes `ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT`, and asserts that the resolver chooses the generated `runtime/Astrea` root when no explicit or installed root is valid.

- [ ] **Step 2: Run only the new test and verify RED.**

Run the configured `runtime_paths` test with the old implementation. Expected: the new development-root assertion fails because the resolver only searches the historical source ancestry.

- [ ] **Step 3: Add explicit development-root support.**

Implement the exact resolution order from the design. Preserve the existing explicit-root rejection semantics, installed-prefix detection, user-local detection, capability flags, diagnostics, and import paths. Remove traversal that only recognizes `source/AstreaOS/src`.

- [ ] **Step 4: Create the CMake staging target.**

Generate `runtime/Astrea/Apps/Explorer`, `Core/components`, `Features/Files`, `System/i18n`, `Core/bridge/apps`, `bin`, and `System/services` by copying source-owned files and built artifacts. Set `ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT` for development executable/test environments. Keep generated output below `build/`.

- [ ] **Step 5: Run the runtime tests and verify GREEN.**

Run the focused runtime test, then the complete `runtime_paths`, `native_bootstrap`, `real_main_qml`, and `app_state_compatibility` tests. Expected: all pass using staged runtime files and no source-tree fallback.

- [ ] **Step 6: Verify installed mapping remains stable.**

Inspect `cmake --install build/debug --prefix /tmp/orbit-prefix` and assert the required `share/Astrea` paths match the baseline clean-install contract.

- [ ] **Step 7: Commit runtime separation.**

```bash
rtk git add apps/explorer
rtk git commit -m "build: stage Orbit development runtime"
```

### Task 6: Replace migration gates with permanent Orbit gates

**Files:**
- Create: `scripts/verify_orbit_source_gate.py`
- Move/replace: `Bench/.../scripts/verify_explorer_clean_install.py` -> `scripts/verify_explorer_clean_install.py`
- Move: `Bench/.../scripts/audit_quickshell_dependencies.py` -> `scripts/audit_qml_dependencies.py` if still useful
- Modify: `scripts/verify_explorer_clean_install.py`
- Create: `scripts/test_verify_orbit_source_gate.py`
- Modify: `apps/explorer/tests/qml/test_explorer_qml.py`

**Interfaces:**
- Produces: deterministic source architecture and clean-install gates that run from the repository root.

- [ ] **Step 1: Write source-gate tests for required invariants.**

Use temporary fixture roots to assert the gate rejects a `Bench/` directory, tracked-like `build/`/`target/` output, a production symlink, a Quickshell import, and missing required directories; assert it accepts the current canonical fixture.

- [ ] **Step 2: Run the gate tests and verify RED.**

Run: `rtk pytest scripts/test_verify_orbit_source_gate.py -q`

Expected: the tests fail because `verify_orbit_source_gate.py` does not exist yet.

- [ ] **Step 3: Implement the targeted Orbit gate.**

Check only the architectural invariants in the brief: absence of migration/source paths, generated output, active source symlinks, Quickshell/process markers, required root directories, Rust workspace members, canonical Explorer source, modules, services, and source/runtime path leaks. Keep diagnostics deterministic and avoid a broad heuristic linter.

- [ ] **Step 4: Rewrite clean-install paths and checks.**

Build with `cmake --preset debug` or a temporary root build from `-S REPOSITORY_ROOT`; clear `ASTREA_ROOT` and all import overrides; isolate HOME/XDG directories; preserve required installed file/executable checks; reject build/cache leakage; run normal and `--portal` self-tests.

- [ ] **Step 5: Run the new gates.**

Run: `python3 scripts/verify_orbit_source_gate.py` and `python3 scripts/verify_explorer_clean_install.py`. Expected: source gate PASS and clean install PASS.

- [ ] **Step 6: Commit permanent gates.**

```bash
rtk git add scripts apps/explorer/tests/qml
rtk git commit -m "test: add permanent Orbit source and install gates"
```

### Task 7: Write current Orbit documentation

**Files:**
- Modify: `README.md`
- Create: `docs/ARCHITECTURE.md`
- Create: `docs/BUILDING.md`
- Create: `docs/TESTING.md`
- Modify: `apps/explorer/docs/APPSTATE_COMPATIBILITY.md`
- Modify: `apps/explorer/docs/ICON_THEME_INTEGRATION.md`
- Modify: `apps/explorer/docs/OPERATION_PROGRESS_ARCHITECTURE.md`

- [ ] **Step 1: Write the repository README.**

Describe Orbit, current Explorer ownership, `old/`, root build commands, root Rust commands, canonical tests, and the top-level tree.

- [ ] **Step 2: Write architecture documentation.**

Document QML/C++/Rust ownership, module boundaries, dependency direction, runtime staging, and explicit separation between source paths and installed Astrea paths.

- [ ] **Step 3: Write building documentation.**

Document prerequisites, presets, Debug/Release reuse, installation, Cargo workspace usage, and the clean-install command.

- [ ] **Step 4: Write testing documentation.**

Document CTest, Rust formatting/check/clippy/test, Python/QML tests, source gate, clean-install gate, and expected qualification workflow.

- [ ] **Step 5: Rewrite Explorer docs in present tense.**

Remove migration-kit framing, retain only current ownership/compatibility information, and ensure every command/path exists in the promoted tree.

- [ ] **Step 6: Commit current documentation.**

```bash
rtk git add README.md docs apps/explorer/docs
rtk git commit -m "docs: describe the Orbit native applications repository"
```

### Task 8: Retire migration material and clean legacy source

**Files:**
- Delete: `Bench/`
- Rename: `Old/` -> `old/`
- Modify: `.gitignore`
- Remove: tracked generated/cache output under `old/`
- Remove: tracked generated/cache output under moved active source

- [ ] **Step 1: Audit promoted reachability before deletion.**

Search CMake, Cargo, QML imports, C++ string references, Rust code, Python tests, shell scripts, and install rules for every non-promoted file under `source/AstreaOS`. Classify retained files as app/shared/service/test/doc and record deletion reasons in the final report.

- [ ] **Step 2: Remove explicitly generated files.**

Remove tracked `build-*`, `target/`, `.qt/`, `.rcc/`, `CMakeFiles/`, `CMakeCache.txt`, `CTestTestfile.cmake`, `__pycache__`, `.pytest_cache`, `.pyc`, object files, and precompiled `src/bin/astrea-launch`. Do not remove user source or assets from `old/`.

- [ ] **Step 3: Remove obsolete migration documentation and scaffolding.**

Delete migration manifests, `source/original/Explorer.zip`, `references/` that only document the kit, numbered migration docs/plans/specs, stale audit/snapshot scripts, config/Quickshell material not reachable from promoted production source, and the entire remaining `Bench/` tree.

- [ ] **Step 4: Rename the legacy archive safely.**

Use `git mv Old old` or a two-step case-only move if required. Preserve legacy source files and assets; update only paths needed for source cleanliness and remove any symlink that would become dangling. Do not migrate legacy applications into active owners.

- [ ] **Step 5: Verify repository hygiene.**

Run: `rtk find . -type l`, `rtk rg -n 'Bench/|source/AstreaOS|src/Apps/Explorer|Apps/Explorer/native|Migration_Kit' --glob '!old/**' .`, and `rtk git ls-files` filtered for generated patterns. Expected: no migration source paths or dangling active symlinks, and no tracked generated output.

- [ ] **Step 6: Commit retirement.**

```bash
rtk git add -A
rtk git commit -m "chore: retire Explorer migration kit"
```

### Task 9: Full qualification and final review

**Files:**
- Modify only files required by fresh verification failures.

- [ ] **Step 1: Run formatting and Rust validation.**

```bash
rtk cargo fmt --all --check
rtk cargo check --workspace --locked
rtk cargo clippy --workspace --all-targets --locked -- -D warnings
rtk cargo test --workspace --locked
```

Expected: all commands exit 0. Pre-existing warnings or environment failures are recorded with exact output and not silently suppressed.

- [ ] **Step 2: Run fresh Debug qualification.**

```bash
cmake --preset debug
cmake --build --preset debug -j2
ctest --preset debug --output-on-failure
```

Expected: all configured CTest targets pass from the fresh root build.

- [ ] **Step 3: Run fresh Release qualification.**

```bash
cmake --preset release
cmake --build --preset release -j2
ctest --preset release --output-on-failure
```

- [ ] **Step 4: Run all Python/QML and repository gates.**

```bash
rtk pytest apps/explorer/tests -q
rtk pytest shared/qml -q
python3 scripts/verify_orbit_source_gate.py
python3 scripts/verify_explorer_clean_install.py
```

- [ ] **Step 5: Perform self-review searches.**

Search active source and docs for stale paths, Quickshell/process markers, source-tree target output, missing modules, and install-path drift. Inspect the full diff for behavioral churn and unexpected files.

- [ ] **Step 6: Run final hygiene checks.**

```bash
rtk git diff --check
rtk git status --short
rtk git log -8 --oneline --decorate
```

- [ ] **Step 7: Create any final focused fix commit.**

Use a narrowly scoped message such as `fix: correct Orbit runtime staging qualification` only when a fresh verification failure requires it. Do not amend earlier commits unless the repository state proves the commit boundary is incorrect.

- [ ] **Step 8: Report exact results.**

Include final tree, ownership changes, deletions, compatibility boundaries, source/runtime separation, CMake/Rust/QML/runtime changes, every test result, blocked environment checks, final status, commit hashes, and intentionally deferred debt.
