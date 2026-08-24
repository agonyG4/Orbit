# Operation Progress Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep backend operation state truthful while giving Explorer's progress card an independent, testable lifecycle that preserves running, success, failure, partial-success, and cancelled states long enough to be seen.

**Architecture:** Add an Explorer-specific `OperationProgressPresenter.qml` that snapshots operation inputs, arbitrates live archive/file sources, owns minimum-running/terminal-hold/fade timing, and exposes a stable card surface. `Main.qml` will render that presenter while the existing generic `ProgressCard` remains visual-only. Native controllers will continue reporting real backend truth, with cancellation and archive startup/reset semantics corrected at their existing boundaries.

**Tech Stack:** Qt 6/QML, Qt Quick, Qt Test, C++17, existing CMake/CTest targets, Rust backend tests.

## Global Constraints

- Do not keep backend `running=true` after completion.
- Do not fake archive percentages or implement archive streaming in this task.
- Do not put presentation timers in Rust, filesystem services, or generic `ProgressCard`.
- Preserve real streamed copy/move progress, refresh/navigation behavior, and archive/file source priority.
- Preserve card layout, styling, spacing, and existing drag/selection/sidebar/icon behavior.
- Use `rtk` for shell commands and the existing build directory.
- Preserve unrelated dirty-worktree changes; do not reset, clean, or overwrite them.

### Task 1: Trace and capture current progress contracts

**Files:**
- Inspect: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/Main.qml`
- Inspect: `Apps/Explorer/state/FileOperationsState.qml`, `Apps/Explorer/AppState.qml`, compatibility adapter, operation controllers/services, backend types, Rust file/archive workers.
- Test: existing native controller and facade tests.

- [ ] **Step 1: Trace active file-operation and archive source paths.**

Record the exact properties/signals used by Main, the existing terminal fields, and whether archive progress is streamed or terminal-only.

- [ ] **Step 2: Confirm current coupling with focused source reads.**

Verify that card visibility currently depends directly on `fileOperationRunning`/`archiveExtractionRunning`, and verify the existing opacity/visible ordering.

- [ ] **Step 3: Do not modify production code in this task.**

Use the findings to define the fixture properties for the presenter tests below.

### Task 2: Add the failing presenter lifecycle test

**Files:**
- Create: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/native/tests/tst_operation_progress_presenter.cpp`
- Modify: `native/CMakeLists.txt`
- Test: presenter lifecycle target.

**Interfaces:**
- Consumes: a small QML presenter fixture with bindable operation properties.
- Produces: a testable presenter API with `phase`, `cardVisible`, `cardOpacity`, `terminal`, `failed`, snapshot fields, and configurable timing properties.

- [ ] **Step 1: Write executable failing QML tests before the presenter exists.**

Cover success terminal retention, fast completion minimum display, failure retention, cancellation, partial success, stale-timer preemption, source arbitration, and archive indeterminate semantics. Use short durations in the fixture and `QTRY_COMPARE`/`QTRY_VERIFY` instead of sleeps.

- [ ] **Step 2: Build and run the focused target.**

Run:

```bash
rtk run cmake --build <existing-build> --target tst_operation_progress_presenter -j2
rtk run ./tst_operation_progress_presenter
```

Expected: the target fails because the presenter component/API does not yet exist, demonstrating that the test exercises the missing lifecycle rather than a source-string convention.

### Task 3: Implement the Explorer-specific presentation coordinator

**Files:**
- Create: `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS/src/Apps/Explorer/components/common/OperationProgressPresenter.qml`
- Modify: `Main.qml`
- Modify if required: Explorer QML install/copy lists and `tests/test_explorer_qml.py` broad architecture checks.

**Interfaces:**
- Consumes: `appState` operation properties for file operations and archive operations.
- Produces: `cardVisible`, `cardOpacity`, `activeKind`, `title`, `detail`, `destination`, `progress`, `percent`, `completedItems`, `totalItems`, `remainingText`, `failed`, `terminal`, `terminalState`, and configurable `minimumRunningDisplayMs`, `successHoldMs`, `partialSuccessHoldMs`, `failureHoldMs`, `cancelledHoldMs`, `fadeOutMs`.

- [ ] **Step 1: Add explicit `hidden`, `running`, `terminal`, and `fading` phases.**

Use one generation token and lifecycle timers. A new live operation increments the generation, stops terminal/fade timers, snapshots the new source, and wins over retained terminal data.

- [ ] **Step 2: Implement source arbitration.**

Choose live archive over live file operation when both are running. If archive is terminal while file work remains live, immediately display the live file operation. Show retained terminal snapshots only when no source is live.

- [ ] **Step 3: Implement minimum-running and terminal retention.**

For fast operations, hold the running phase until the configured minimum, then show the terminal snapshot. Use defaults of 500ms minimum running, 1200ms success hold, 2200ms partial-success hold, 5000ms failure hold, 1500ms cancelled hold, and 160ms fade.

- [ ] **Step 4: Implement fade correctly.**

Keep `cardVisible` true while opacity animates to zero; only set the phase to hidden on animation completion. Ensure no-animation/reduced-motion behavior reaches hidden deterministically.

- [ ] **Step 5: Replace Main's direct running/opacity visibility logic.**

Instantiate the presenter, bind `ProgressCard` only to presenter fields, and preserve the existing card layout and generic card component.

- [ ] **Step 6: Run the focused presenter tests.**

Expected: all lifecycle tests pass without changing backend running state.

### Task 4: Correct native operation terminal and archive semantics

**Files:**
- Modify: `native/src/controllers/file_operations_controller.h/.cpp`
- Modify: `native/src/controllers/app_state_facade.h/.cpp`
- Modify: `native/src/services/file_operation_service.*` only if the existing typed terminal state requires a boundary fix.
- Modify: `native/tests/tst_file_operations_controller.cpp`
- Modify: `native/tests/tst_app_state_facade.cpp`

- [ ] **Step 1: Add a failing cancellation assertion.**

Extend the existing cancel test to require `operationState == "cancelled"` (or the exact typed terminal value used by the project) while `running == false`.

- [ ] **Step 2: Implement coherent cancellation state.**

Keep the existing backend cancel request, but set terminal cancellation state/status/error consistently before publishing the state. Do not delay `setRunning(false)`.

- [ ] **Step 3: Add and run terminal-state tests.**

Cover success, failure, partial success, cancellation, and new-operation reset/preemption at the native facade/controller boundary.

- [ ] **Step 4: Correct archive startup/reset/destination fields.**

At both extraction and compression start, clear all archive terminal/transient fields and set running archive counts to `0/0`, percent `0`, progress `0.0`. While running, expose planned destination; after success prefer the published result destination, otherwise fall back to the planned destination. Preserve final navigation destination behavior.

- [ ] **Step 5: Add archive indeterminate/reset tests.**

Verify extraction and compression start with unknown totals, terminal success becomes `1/1` and 100%, failure preserves error, and a second archive operation does not inherit the first operation's destination/count/error/status.

### Task 5: Integrate and validate all regression paths

**Files:**
- Modify only directly related source/tests discovered in Tasks 1–4.
- Add English documentation: `Apps/Explorer/native/docs/OPERATION_PROGRESS_LIFECYCLE.md`.

- [ ] **Step 1: Document the lifecycle boundary and archive limitation.**

State that backend running truth is independent from presentation lifetime, explain timing/source priority, and record that archive extraction/compression expose terminal results without continuous typed progress, so the UI uses indeterminate running presentation rather than fake percentages.

- [ ] **Step 2: Run focused C++/QML checks.**

Run presenter, file-operation controller, app-state facade, archive, and real Main QML tests.

- [ ] **Step 3: Run Explorer Python/QML regression tests.**

Run `rtk run python3 tests/test_explorer_qml.py` and confirm no source-level assertions still encode direct running-to-card visibility.

- [ ] **Step 4: Run full build and CTest.**

Run the existing full CMake build and full CTest suite with `--output-on-failure`.

- [ ] **Step 5: Run Rust tests and formatting.**

Run the Explorer Rust `cargo test` and `cargo fmt -- --check`; no Rust implementation changes are expected.

- [ ] **Step 6: Install and perform available runtime qualification.**

Install to `/home/agony/.local/share/AstreaNative`, run the installed startup/self-test, and report whether graphical extraction/copy/move/failure/cancel/second-operation validation was available. Do not claim manual visual qualification if it was not possible.

- [ ] **Step 7: Review and check the final diff.**

Confirm backend running truth, snapshot retention, active-operation preemption, stale-timer protection, indeterminate archive semantics, untouched generic card styling, and `git diff --check`. Preserve unrelated dirty files and do not commit unless explicitly requested.
