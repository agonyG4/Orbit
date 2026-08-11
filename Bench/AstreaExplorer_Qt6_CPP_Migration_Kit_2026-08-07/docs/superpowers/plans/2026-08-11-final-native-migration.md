# Astrea Explorer Complete Native Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the native Qt 6/C++ Explorer the only production runtime while preserving the existing QML UI and retaining Rust for filesystem-heavy and security-sensitive operations.

**Architecture:** Keep `AppState.qml` as the public bridge. Native C++ owns application state, controllers, models, typed services, async generations, process lifecycle, clipboard, portal mode, and backend transport. Rust owns filesystem semantics, destructive operations, archives, trash, traversal, thumbnails, devices, and AppImage work. Production Explorer QML contains presentation and UI-only transient state only.

**Tech Stack:** Qt 6.11/C++17, QML/Qt Quick, CMake/CTest, Rust 2024, existing Rust backend, XDG Desktop Entry/MIME/Trash contracts, and the existing portal interfaces.

## Global Constraints

- Preserve the existing QML visual tree and visual values.
- Keep `AppState.qml` as the single public compatibility contract.
- Mutate Qt models only on their owning GUI thread.
- Give every supersedable async operation an explicit request/generation identity.
- Use argv-safe typed `QProcess` services; never shell-concatenate commands.
- Do not use Quickshell, `Quickshell.Io`, QML `Process {}`, `explorer_helper.py`, `wl-copy`, `wl-paste`, `/usr/bin/qs`, or machine-specific repository paths in production Explorer.
- Preserve unrelated pre-existing worktree state and stage only focused paths for each commit.
- Use clean Debug and Release build directories for final qualification.

## Work packages

### Task 1: Native low-risk filesystem and integration services

Files: add focused C++ services/controllers under `native/src/services` and
`native/src/controllers`; extend Rust Explorer backend modules and tests; wire
them into `AppStateFacade` and `ExplorerApplication`.

- [ ] Add failing tests for safe child-name validation, create-folder,
  collision-aware rename, bounded executable/path suggestions, file metadata,
  XDG desktop shortcut creation, and explicit error codes.
- [ ] Add typed Rust commands for those filesystem operations and retain
  containment/symlink/error semantics from `explorer_helper.py`.
- [ ] Add C++ request-generation wrappers and facade methods; reject stale
  completions and keep GUI model updates on the GUI thread.
- [ ] Replace Toolbar, Sidebar, and FileContextMenu helper calls with facade
  calls while preserving visible text and geometry.
- [ ] Run focused C++/Rust/QML tests and commit the slice.

### Task 2: Devices and network ownership

Files: `DeviceController`, new network service/controller, `DeviceNetworkState.qml`,
facade/adapter tests, and Rust/typed external integration where required.

- [ ] Add failing tests for device refresh generations, mount/unmount/remount,
  auto-mount state, network probe/connect cancellation, and stale results.
- [ ] Reuse the existing native `DeviceController` and Rust device backend.
- [ ] Implement network probing/mounting behind a typed C++ service with argv
  lists, bounded output, timeout, cancellation, and mapped errors.
- [ ] Route native-mode DeviceNetworkState through the facade and remove its
  process nodes and Quickshell import.
- [ ] Run focused tests and commit the slice.

### Task 3: Open With, default associations, and launch

Files: `OpenWithController`, `LaunchService`, desktop catalog service,
`OpenWithMenu.qml`, `PreviewState.qml`, facade, Rust/native tests.

- [ ] Add failing tests for MIME matching, desktop visibility, deterministic
  ordering, default association read/write, selected-app target argv, and
  stale catalog requests.
- [ ] Build one indexed desktop catalog reusable by Recent, Open With, and
  launching; preserve Desktop Entry Exec field-code semantics through the
  centralized launch layer.
- [ ] Route normal file, executable, desktop, Windows, shell-script, and
  external launch actions through typed native services.
- [ ] Remove Open With and Preview process nodes/imports and commit.

### Task 4: Preview and thumbnail pipeline

Files: `PreviewController`, backend client/types, `PreviewState.qml`,
`DirectoryModel`, facade, and tests.

- [ ] Add failing tests for preview generations, remote policy, targeted role
  updates, thumbnail warm ranges, cancellation, and rapid navigation.
- [ ] Extend the native controller to schedule backend thumbnail warming without
  GUI-thread blocking and reject stale metadata/results.
- [ ] Route PreviewState to native properties/methods and remove all process
  ownership while preserving preview presentation.
- [ ] Run focused tests and commit.

### Task 5: Clipboard, paste, and conflict workflow

Files: `ClipboardService`, `FileOperationsController`, backend Rust file-op
and conflict modules, `FileOperationsState.qml`, facade, tests.

- [ ] Add failing tests for Qt MIME URI lists, cut/copy transitions, image paste
  naming/atomic writes, conflict planning, cancellation, stale terminal events,
  and model refresh.
- [ ] Read actual `QClipboard` MIME data in native code; remove wl-copy/wl-paste
  and helper clipboard calls.
- [ ] Move conflict planning into Rust using the same path semantics as file-op
  execution; expose typed progress/conflict/result events.
- [ ] Route all paste state and conflict UI through native facade properties and
  remove FileOperationsState process nodes/imports.
- [ ] Run destructive-operation tests and commit.

### Task 6: Trash, archives, AppImage, and wallpaper

Files: new Rust filesystem/security modules and tests, typed native services,
`FileOperationsState.qml`, `FileContextMenu.qml`, facade, launch/integration
configuration.

- [ ] Add failing Rust tests for `.trashinfo`, collisions, restore conflicts,
  malformed metadata, symlinks, empty trash, archive traversal/absolute-path/
  symlink escapes, password/conflict/rollback/cancellation, and AppImage staged
  publication.
- [ ] Port helper trash and archive semantics to Rust; emit structured JSON
  events rather than human-readable progress parsing.
- [ ] Add typed C++ orchestration for password/conflict UI, cancellation,
  progress, result mapping, and model refresh.
- [ ] Route wallpaper through the Astrea desktop integration service and remove
  machine-specific command construction.
- [ ] Remove remaining file-operation/context-menu process nodes and commit.

### Task 7: Native FileChooser portal

Files: `native/src/main.cpp`, `ExplorerApplication`, `PortalController`,
`PortalDialog.qml`, CMake, Rust/Python portal launcher, portal tests.

- [ ] Add a failing native `--portal` integration test covering open, multiple,
  folder, save, filters, current folder/name, accept/cancel, timeout, and
  result-file failure.
- [ ] Load the existing FileChooser presentation from the native executable,
  resolve environment contracts in C++, and atomically write the result file.
- [ ] Update the production portal launcher to invoke `astrea-explorer
  --portal` with explicit argv; remove `/usr/bin/qs` and direct QML execution.
- [ ] Remove PortalDialog Quickshell/process dependencies and commit.

### Task 8: Eliminate production QML process ownership and helper dependency

Files: all listed production QML states/components, `LegacyAppStateAdapter.qml`,
`AppState.qml`, runtime resolver, audit script, helper ownership journal.

- [ ] Replace environment access with native runtime properties and remove all
  production Quickshell imports and QML Process objects.
- [ ] Add/update the deterministic static gate to fail on Process, Quickshell,
  helper, Python, wl-copy/wl-paste, qs, and `/home/agony` production matches.
- [ ] Prove helper production caller count is zero, preserve only parity tests /
  fixtures, then remove `explorer_helper.py` from production source.
- [ ] Remove obsolete transitional state and legacy adapter code only after the
  native facade contract tests pass.
- [ ] Run the audit and commit the canonical-runtime slice.

### Task 9: Persistent Rust backend worker

Files: Rust backend `serve` mode/protocol, C++ transport abstraction, tests,
application wiring, fallback diagnostics.

- [ ] Add protocol tests for handshake/version, request IDs, bounded frames,
  terminal status, progress, cancellation, crash, restart limits, and shutdown.
- [ ] Implement event-driven framed JSONL worker transport behind the existing
  backend abstraction without blocking the GUI thread.
- [ ] Make the persistent worker canonical for normal Explorer operations;
  retain one-shot transport only as explicit diagnostic/fallback mode.
- [ ] Run all backend/native tests and commit.

### Task 10: Launcher, packaging, qualification, and archive

Files: system launcher/config, install/package metadata, migration journal,
dependency audit, tests/scripts, source-only archive.

- [ ] Replace the system file-manager launcher with installed `astrea-explorer`.
- [ ] Verify packaging includes executable, QML, translations, themes/assets,
  backend binary, portal integration, and desktop entry where applicable.
- [ ] Run clean Debug/Release builds, all CTest/Python/Rust/portal tests, QML
  lint, static dependency audit, real native/quickshell-independent smoke
  where available, and `git diff --check`.
- [ ] Append evidence and unresolved environment limitations to the journal;
  do not claim a gate passed without command output.
- [ ] Create the final source-only migration archive after qualification.
- [ ] Use focused commits for each green work package.

