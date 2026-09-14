# Windows Launch Closure Implementation Plan

> **For inline execution:** Execute the tasks in this session with test-first checkpoints. Do not create a worktree, subagent, or alternate build directory.

**Goal:** Complete the native Windows launch integration without changing the approved `astrea-launch` architecture.

**Architecture:** Keep Rust responsible for Windows planning, bounded PE inspection, runner policy, wrapper argv, environment construction, and launch metadata. Add a Qt-native controller that owns only the short-lived `astrea-launch --windows` process, then project its result through `AppStateFacade` and the existing status bar. Ordinary file and desktop launches continue through the existing detached `LaunchService` path.

**Tech Stack:** Rust/Cargo, Qt 6 C++, QProcess, QJsonDocument, Qt Test, CMake, existing `build/debug` tree.

## Global Constraints

- Preserve `LaunchRequest::Windows`, UMU-only Proton policy, Wine/Auto semantics, shared prefix, neutral UMU identity, fresh Gaming config, environment separation, metadata history, and retired-runner removal.
- Do not use `sh -c`, shell interpolation, or shell-based custom-prefix construction.
- `astrea-launch --windows` waits only for planning/dispatch acceptance; Explorer never owns the Windows application lifetime.
- PE validation reads a fixed-size header only; it never allocates proportional to executable size.
- Use the existing configured `build/debug` directory for all compilation and tests.
- No new worktree, alternate build tree, or subagent.

### Task 1: Harden Rust PE validation and Windows planner contracts

**Files:**
- Modify: `services/launch/src/lib.rs`
- Test: `services/launch/tests/launcher_tests.rs`
- Test: `services/launch/tests/cli_contract.rs`

- [ ] Add failing tests for bounded PE validation, malformed offsets, sparse large files, historical wrapper ordering, MSI runner argv, and doctor capability output.
- [ ] Run the focused Rust tests and confirm they fail for the missing bounded reader/order/diagnostic behavior.
- [ ] Implement bounded `File` header reads with checked offset and length validation, then restore `custom_prefix` inside GameMode/Gamescope/MangoHud outer composition.
- [ ] Add non-secret Windows capability lines to `doctor` without reading or printing Gaming config values.
- [ ] Run all `services/launch` Rust targets and confirm green.

### Task 2: Add the native asynchronous Windows launch controller

**Files:**
- Create: `apps/explorer/src/services/windows_launch_controller.h`
- Create: `apps/explorer/src/services/windows_launch_controller.cpp`
- Modify: `apps/explorer/CMakeLists.txt`
- Create: `apps/explorer/tests/cpp/tst_windows_launch_controller.cpp`

- [ ] Add failing Qt tests for valid success JSON, UMU/Wine/PE failures, malformed JSON, failed process start, bounded output, and a fake CLI that exits while a separate child remains alive.
- [ ] Run the focused controller target and confirm it fails because the controller does not exist.
- [ ] Implement bounded stdout/stderr capture, asynchronous process completion, Windows-record validation, runner/machine/warnings projection, and actionable errors.
- [ ] Ensure the controller never calls `waitForFinished()` and never follows the child application after the CLI exits.
- [ ] Build and run the controller target.

### Task 3: Project controller state through AppState and preserve routing

**Files:**
- Modify: `apps/explorer/src/controllers/app_state_facade.h`
- Modify: `apps/explorer/src/controllers/app_state_facade.cpp`
- Modify: `apps/explorer/src/explorer_application.cpp`
- Modify: `apps/explorer/qml/AppState.qml`
- Modify: `apps/explorer/qml/components/layout/StatusBar.qml`
- Modify: `apps/explorer/tests/cpp/tst_app_state_facade.cpp`

- [ ] Add failing AppState contract/routing tests for `.exe/.EXE/.msi/.MSI`, native properties, and unchanged ordinary file/desktop launch dispatch.
- [ ] Run the focused AppState test and confirm the new properties/controller routing are absent.
- [ ] Add the controller dependency, connect its state signal, expose running/status/error/runner/machine/warnings properties, and route only Windows targets through it.
- [ ] Add the accepted-launch and error text to the existing status bar; do not create a new notification subsystem.
- [ ] Build and run AppState, LaunchService, RuntimePaths, and QML tests.

### Task 4: Final verification and commit

**Files:**
- Verify: all modified source and test files above.
- Modify if needed: `docs/superpowers/plans/2026-09-14-windows-launch-closure.md`

- [ ] Run focused Rust tests, focused Qt tests, complete `build/debug` CMake build, complete CTest, Rust workspace tests where configured, QML tests, source gates, and `git diff --check`.
- [ ] Inspect specifically for whole-file PE reads, direct Proton execution, shell custom-prefix construction, environment/history leakage, stale Gaming config, Explorer application-lifetime waits, and retired runtime references.
- [ ] Commit the closure with a focused message and verify the working tree is clean.
