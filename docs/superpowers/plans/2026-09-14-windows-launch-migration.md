# Windows Launch Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `astrea-launch --windows` the sole Windows dispatch authority while preserving ordinary file and `.desktop` launching.

**Architecture:** Add a typed Windows request and pure planning helpers to `services/launch`. The planner reads Gaming/Compatibility JSON on each Windows request, validates PE/MSI targets, selects UMU/Wine according to the explicit policy, composes argv/environment without a shell, and returns safe Windows metadata. Explorer always dispatches Windows files through detached `astrea-launch`; the retired helper path is removed from runtime discovery and public facade state.

**Tech Stack:** Rust 2024, `serde_json`, `std::process::Command`, Qt 6/C++, CMake/CTest.

## Global Constraints

- Keep build outputs in the existing repository `build/debug` tree.
- Do not use subagents.
- Preserve the shared Astrea Windows prefix and backward-compatible Gaming JSON fields.
- Explicit Proton requires UMU; never execute direct `proton run` and never silently fall back to Wine.
- Auto prefers UMU and falls back to Wine; explicit Wine requires Wine.
- Read Gaming/Compatibility JSON fresh for every Windows launch.
- Parse custom prefixes into argv tokens without a shell.
- Keep arbitrary environment values out of launch history.
- Use the neutral/default UMU identity unless a real identity is already provided; never create path-hash IDs.
- Explorer must not wait on the Windows application lifetime.

### Task 1: Establish Rust Windows planning contracts

**Files:**
- Modify: `services/launch/tests/launcher_tests.rs`
- Modify: `services/launch/src/lib.rs`

Add failing tests for request parsing/serialization, PE and MSI validation, compatibility normalization, shell-free token parsing, runner policy, shared-prefix paths, metadata-only history, and environment propagation to direct/systemd argv builders. Run the focused Rust tests to observe the expected failures, then implement the smallest pure/testable contracts.

### Task 2: Integrate Windows planning with the daemon and CLI

**Files:**
- Modify: `services/launch/src/lib.rs`
- Modify: `services/launch/src/main.rs`
- Modify: `services/launch/tests/cli_contract.rs`

Route `--windows <path>` through the daemon protocol and direct fallback. Keep config loading per request, launch the planned command with detached process semantics, record only safe metadata, and add an integration test that proves compatibility JSON changes are observed by successive launches.

### Task 3: Remove the retired Explorer runner authority

**Files:**
- Modify: `apps/explorer/src/services/launch_service.h`
- Modify: `apps/explorer/src/services/launch_service.cpp`
- Modify: `apps/explorer/src/controllers/app_state_facade.h`
- Modify: `apps/explorer/src/controllers/app_state_facade.cpp`
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.h`
- Modify: `apps/explorer/src/runtime/explorer_runtime_paths.cpp`
- Modify: `apps/explorer/src/explorer_application.cpp`
- Modify: `apps/explorer/tests/cpp/tst_launch_service.cpp`
- Modify: `apps/explorer/tests/cpp/tst_app_state_facade.cpp`
- Modify: `apps/explorer/tests/cpp/tst_recent_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_runtime_paths.cpp`

Make `LaunchService::windowsLaunch` build a detached `astrea-launch --windows` spec, remove retired runtime-path and facade properties, and retain ordinary file/desktop behavior. Update tests to assert the single authority and no lifetime wait.

### Task 4: Verify, audit, and commit

**Files:**
- Modify: `docs/superpowers/plans/2026-09-14-windows-launch-migration.md`

Run focused Rust and C++ tests, build everything in `build/debug`, scan the final diff for retired runner references, direct Proton execution, environment leakage, shell construction, daemon config caching, and Explorer lifetime waits, then commit the complete change.
