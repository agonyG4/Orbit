# Explorer Archive 2.0 Safety, Lifecycle, and Performance Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close Archive 2.0's remaining P0/P1 extraction-safety, destination-semantics, lifecycle, password-transport, provider-truthfulness, and performance gaps while preserving the dedicated archive architecture.

**Architecture:** Extend the existing typed archive request with explicit extraction destination mode and logical workflow state. Keep every extraction fully staged, publish into an existing container member-by-member through a rollback transaction, and use deterministic provider plans/parsers. Extend the existing persistent worker JSON protocol with a bounded optional stdin payload so archive secrets do not reach the Astrea operation argv.

**Tech Stack:** Rust 2024 with serde/serde_json and external bsdtar/7z providers; C++17/Qt 6.11; QML; Python unittest/QML source tests; CMake preset `debug` and the existing `build/debug` directory.

## Global Constraints

- Use the current checkout; do not create a worktree or alternate build directory.
- Use the existing `build/debug` and configured Rust target directory.
- Do not restore the generic Utility archive path or move normal worker traffic to the archive lane.
- Stage archive members privately before provider extraction and validate before publication.
- For `into-directory`, never call `remove_dir_all`, rename, replace, or recursively delete the destination container.
- Keep passwords out of logs, status/error telemetry, and Astrea operation-child argv; isolate any unavoidable 7z provider `-p` use.
- Run each focused test red before its production implementation and use `rtk` for shell/test commands.
- Commit each independently testable task.

## File map

- `apps/explorer/backend/src/archive.rs`: extraction modes, member publication transaction, 7z records, canonical suffixes, source preparation, provider plans/capabilities, profiles/threads, tests.
- `apps/explorer/backend/src/worker.rs`: bounded optional child stdin payload and cleanup.
- `apps/explorer/src/backend/backend_types.h`: destination mode, logical state, directional capability fields, transport payload types.
- `apps/explorer/src/backend/rust_backend_client.{h,cpp}`: archive JSON without password, payload transport, decoding.
- `apps/explorer/src/backend/persistent_worker_transport.{h,cpp}`: optional bounded payload through worker stdin and timeout behavior.
- `apps/explorer/src/controllers/archive_controller.{h,cpp}` and `app_state_facade.{h,cpp}`: workflow state and terminal-signal semantics.
- `apps/explorer/qml/components/common/FileContextMenu.qml`: distinct extraction actions, canonical stem, safe archive names, capability profiles.
- `apps/explorer/qml/state/FileOperationsState.qml`, `AppState.qml`, `Main.qml`, `OperationProgressPresenter.qml`: state projection and presentation.
- `apps/explorer/tests/cpp/*`, `apps/explorer/tests/qml/test_explorer_qml.py`: protocol, transport, controller, presenter, and action regressions.

---

### Task 1: Add failing safety, mode, and workflow-contract tests

**Files:**
- Modify: `apps/explorer/tests/cpp/tst_backend_client.cpp`
- Modify: `apps/explorer/tests/cpp/tst_persistent_worker_transport.cpp`
- Modify: `apps/explorer/tests/cpp/tst_archive_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_operation_progress_presenter.cpp`
- Modify: `apps/explorer/tests/qml/test_explorer_qml.py`
- Modify: `apps/explorer/backend/src/archive.rs` test module only for new pure tests

**Interfaces:**
- Tests define `ArchiveOperationRequest.destinationMode` values `new-directory` and `into-directory`.
- Tests define `ArchiveWorkflowState` values `running`, `waiting-password`, `waiting-conflict`, `success`, `cancelled`, and `failed`.
- Tests require `ArchiveCapability.createProvider`, `extractProvider`, `createPasswordSupported`, `extractPasswordSupported`, and provider-specific profile data.

- [ ] **Step 1: Write the Rust red tests.** Add tests for an existing destination containing `unrelated.txt` plus staged `archive-file.txt`; run `into-directory` with `keep-both` and `overwrite`; assert the container and unrelated file survive. Add tests for directory merge, canonical aliases (`.zip`, `.7z`, `.rar`, `.tar`, `.tar.gz`, `.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst`, `.tzst`), compound keep-both names, unsafe archive names, and the invariant that the into-directory publication path contains no container deletion operation.
- [ ] **Step 2: Write the 7z parser red tests.** Fixtures must cover regular file, directory, encrypted regular file, symbolic link, hard link, and encrypted symbolic link. Assert `encrypted` and link fields are independent and links are rejected before extraction.
- [ ] **Step 3: Write source-preparation red tests.** Add a same-parent multi-source seam asserting `materialize_sources` is not called, a cross-parent seam asserting bounded preparation/cancellation, and a deterministic cancellation helper that cancels while preparation is active and leaves no `.astrea-archive-*` input clone or final archive.
- [ ] **Step 4: Write C++/QML red tests.** Assert the three QML actions call distinct APIs/modes; password-required and conflict are waiting states without `operationFinished`; password/conflict cancellation ends as `cancelled`; cancelled archive requests project to the shared Cancelled presentation; archive progress snapshots preserve real counts; and an archive request's operation argv does not contain the password.
- [ ] **Step 5: Run focused tests and record expected failures.**

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive -- --nocapture
rtk ctest --test-dir build/debug -R 'backend_client|persistent_worker_transport|archive_controller|operation_progress_presenter' --output-on-failure
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
```

Expected: the new tests fail because the current implementation replaces an into-directory destination, aliases the QML actions, parses 7z link attributes as encryption, copies every 7z source, and emits terminal failure/completion for continuation states.

### Task 2: Implement explicit extraction destination semantics and transactional publication

**Files:**
- Modify: `apps/explorer/backend/src/archive.rs`
- Modify: `apps/explorer/src/backend/backend_types.h`
- Modify: `apps/explorer/src/backend/rust_backend_client.cpp`
- Modify: `apps/explorer/src/controllers/archive_controller.{h,cpp}`
- Modify: `apps/explorer/src/controllers/app_state_facade.{h,cpp}`
- Test: files from Task 1

**Interfaces:**
- `OperationRequest.destination_mode: String` accepts only `new-directory` and `into-directory`.
- `extract_archive` stages into a private sibling and calls `publish_extraction(stage, destination, mode, policy, cancellation)`.
- `ArchiveController::startArchiveExtraction` creates `new-directory`; `startArchiveExtractionHere` and `startArchiveExtractionTo` create `into-directory`.

- [ ] **Step 1: Add request/type fields and decoder coverage.** Serialize/deserialize `destinationMode`, default legacy requests to `new-directory` only where the old API explicitly means a child folder, and reject missing/invalid mode for new extraction requests. Preserve request identity and compatibility aliases.
- [ ] **Step 2: Implement a member-wise publication plan.** Enumerate only staged top-level members, validate every relative component, and represent operations as incoming path, destination path, conflict kind, and chosen keep-both name. For `new-directory`, publish the staged child as a replaceable target. For `into-directory`, treat the destination only as a container and never pass it to a recursive removal helper.
- [ ] **Step 3: Implement safe keep-both.** Reserve unique incoming top-level names against the destination and other incoming names; for a directory conflict, choose one renamed root and leave all existing siblings untouched.
- [ ] **Step 4: Implement overwrite transaction.** Create a mode-0700 `.astrea-extract-backup-<request>` sibling. Move only conflicting regular files or conflicting top-level entries into backup; recursively merge incoming directories and back up only colliding descendants. Replace regular files with rename/atomic moves. Never delete an existing directory tree as a shortcut.
- [ ] **Step 5: Add rollback and cancellation handling.** On any publish error or cancellation, remove newly published incoming paths where safe, restore backups in reverse order, preserve unrelated destination entries, and remove staging/backup. On success remove backup only after all paths are verified. If a recursive conflict cannot be safely represented, return a structured failure before destructive work and keep Replace unavailable for that case.
- [ ] **Step 6: Run the Rust and controller safety tests green.**

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::into_directory -- --nocapture
rtk ctest --test-dir build/debug -R 'archive_controller|backend_client' --output-on-failure
```

- [ ] **Step 7: Commit.**

```bash
rtk git add apps/explorer/backend/src/archive.rs apps/explorer/src/backend/backend_types.h apps/explorer/src/backend/rust_backend_client.cpp apps/explorer/src/controllers/archive_controller.{h,cpp} apps/explorer/src/controllers/app_state_facade.{h,cpp} apps/explorer/tests
rtk git commit -m "fix: publish archive members safely into existing directories"
```

### Task 3: Implement deterministic 7z inspection and cancellable source preparation

**Files:**
- Modify: `apps/explorer/backend/src/archive.rs`
- Modify: `apps/explorer/backend/src/worker.rs` only if the preparation test needs worker cancellation plumbing
- Test: `apps/explorer/backend/src/archive.rs`

**Interfaces:**
- `SevenZipMemberRecord { path, encrypted, symbolic_link, hard_link, attributes }` is the sole parsed representation of a 7z `-slt` member.
- `parse_seven_zip_slt_records(input: &str) -> Result<Vec<SevenZipMemberRecord>, ArchiveError>` is pure and deterministic.
- `prepare_sources(plan, stage, sources, cancellation, emitter) -> Result<PreparedSources, ArchiveError>` selects direct or bounded fallback input.

- [ ] **Step 1: Implement the pure `-slt` record parser.** Split records on blank lines, parse `Path`, `Encrypted`, `Symbolic Link`, `Hard Link`, and `Attributes` independently, ignore the archive summary record, reject malformed member records, and never infer encryption from an `L` attribute.
- [ ] **Step 2: Make link rejection pre-extraction.** In the 7z inspection path, parse all records, validate every path, return `unsafe-member` for any symbolic/hard link representation, and only return password-required for encrypted regular members without a password. Keep post-extraction tree validation.
- [ ] **Step 3: Add direct same-parent planning.** If all selected source parents match, set `cwd` to that parent and pass only source basenames to 7z. Add the common safe-root path when it preserves the selected top-level layout. Do not call `materialize_sources` for either direct case.
- [ ] **Step 4: Bound cross-parent fallback.** Emit `preparing`, create the private `input` root only for a genuine cross-parent mismatch, copy files in fixed-size chunks with cancellation checks between reads/writes, recurse with `symlink_metadata`, preserve promised permissions/times, and remove partial input on cancellation or error.
- [ ] **Step 5: Run red/green security and preparation tests.**

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::seven_zip -- --nocapture
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::same_parent -- --nocapture
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::cancellation_during_preparation -- --nocapture
```

- [ ] **Step 6: Commit.**

```bash
rtk git add apps/explorer/backend/src/archive.rs apps/explorer/backend/src/worker.rs
rtk git commit -m "fix: reject 7z links before extraction and avoid unnecessary source clones"
```

### Task 4: Make provider plans and capabilities truthful

**Files:**
- Modify: `apps/explorer/backend/src/archive.rs`
- Modify: `apps/explorer/src/backend/backend_types.h`
- Modify: `apps/explorer/src/backend/rust_backend_client.cpp`
- Modify: `apps/explorer/src/controllers/archive_controller.cpp`
- Modify: `apps/explorer/qml/components/common/FileContextMenu.qml`
- Test: Rust archive tests and `apps/explorer/tests/qml/test_explorer_qml.py`

**Interfaces:**
- `CreatePlan` contains provider-specific arguments, supported profiles, and optional thread settings.
- `Capability` reports `create_provider`, `extract_provider`, `create_password_supported`, `extract_password_supported`, `profiles`, and `threads` truthfully.

- [ ] **Step 1: Change ZIP provider priority and profile mapping.** Prefer 7z ZIP creation when available and map Fast/Balanced/Maximum to distinct `-mx` levels. For bsdtar ZIP, return an empty profile list and one deterministic command plan; never advertise ignored levels.
- [ ] **Step 2: Add deterministic provider probes.** Probe only supported help/version output for bsdtar XZ/Zstandard thread options; store booleans in provider availability and add `--options=xz:threads=N` / `--options=zstd:threads=N` only when confirmed. Keep TAR without profiles and do not advertise theoretical 7z extraction formats without a successful deterministic probe/matrix entry.
- [ ] **Step 3: Split directional capability fields end-to-end.** Update Rust JSON, C++ decoding, QVariant maps, and QML profile binding. Ensure 7z-only extraction capabilities are exposed when 7z is available and bsdtar is absent, while unsupported formats remain absent.
- [ ] **Step 4: Add argument-plan and provider-matrix tests.** For every advertised profile assert a distinct meaningful provider setting; for no-level providers assert empty profiles; assert ZIP/7Z format authority remains distinct; assert TAR.ZST aliases and 7z-only extraction behavior.
- [ ] **Step 5: Run focused tests and commit.**

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::capability -- --nocapture
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::profile -- --nocapture
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
rtk git add apps/explorer/backend/src/archive.rs apps/explorer/src/backend/backend_types.h apps/explorer/src/backend/rust_backend_client.cpp apps/explorer/src/controllers/archive_controller.cpp apps/explorer/qml/components/common/FileContextMenu.qml apps/explorer/tests
rtk git commit -m "fix: derive archive capabilities from provider behavior"
```

### Task 5: Close canonical naming, action semantics, and input safety

**Files:**
- Modify: `apps/explorer/qml/components/common/FileContextMenu.qml`
- Modify: `apps/explorer/qml/AppState.qml`
- Modify: `apps/explorer/qml/state/FileOperationsState.qml`
- Modify: `apps/explorer/src/controllers/archive_controller.cpp`
- Modify: `apps/explorer/tests/qml/test_explorer_qml.py`
- Modify: `apps/explorer/backend/src/archive.rs` tests/helpers

**Interfaces:**
- `canonical_archive_stem(name)` matches supported suffixes longest-first.
- `FileContextMenu.runExtractHere()` calls `startArchiveExtractionHere(itemPath, AppState.currentPath)`.
- `FileContextMenu.runExtractTo()` calls `startArchiveExtractionTo(itemPath, selectedFolder)` with `destinationMode = into-directory`.
- `FileContextMenu.runExtractToNamedFolder()` calls `startArchiveExtraction(itemPath, canonicalArchiveStem(itemPath))` with `destinationMode = new-directory`.

- [ ] **Step 1: Add the canonical stem helper and tests.** Cover every required alias, case-insensitively, and ensure `.tar.zst`/`.tzst` are stripped before shorter suffixes. Use the same helper for compression defaults and keep-both target naming.
- [ ] **Step 2: Sanitize archive-name input.** Reject empty names, separators, absolute paths, `.`/`..`, and path components containing parent traversal. Keep the output rooted in the current Explorer directory and show a deterministic validation error instead of treating the field as a path.
- [ ] **Step 3: Split all three QML actions.** Replace the shared `runExtract`; make Extract Here use the current folder container, Extract to `<stem>/` use a new child destination, and Extract… use the picked existing folder. Add distinct source assertions and runtime seam coverage.
- [ ] **Step 4: Fix compound keep-both naming.** Preserve `.tar.gz`, `.tar.xz`, `.tar.zst`, `.tar.bz2`, `.zip`, and `.7z` as suffixes so `Archive.tar.zst` becomes `Archive (2).tar.zst`.
- [ ] **Step 5: Run QML and Rust naming tests and commit.**

```bash
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::canonical -- --nocapture
rtk git add apps/explorer/qml apps/explorer/src/controllers/archive_controller.cpp apps/explorer/backend/src/archive.rs apps/explorer/tests/qml/test_explorer_qml.py
rtk git commit -m "fix: give archive actions distinct destinations and safe names"
```

### Task 6: Complete logical workflow lifecycle and presenter behavior

**Files:**
- Modify: `apps/explorer/src/controllers/archive_controller.{h,cpp}`
- Modify: `apps/explorer/src/controllers/app_state_facade.{h,cpp}`
- Modify: `apps/explorer/qml/state/FileOperationsState.qml`
- Modify: `apps/explorer/qml/AppState.qml`
- Modify: `apps/explorer/qml/components/common/OperationProgressPresenter.qml`
- Modify: `apps/explorer/qml/Main.qml`
- Modify: `apps/explorer/tests/cpp/tst_archive_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_operation_progress_presenter.cpp`

**Interfaces:**
- `ArchiveController::workflowState()` returns the explicit logical state.
- `currentArchiveOperationSnapshot()` includes `state`, `phase`, counts, bytes, continuation flags, and structured error.
- `operationFinished` is emitted only for `success`, `cancelled`, and unrecoverable `failed`.

- [ ] **Step 1: Add the explicit controller state.** Store logical state separately from request running state; map password-required/bad-password with retry available to waiting-password, destination-conflict to waiting-conflict, and cancellation to cancelled.
- [ ] **Step 2: Separate request completion from logical completion.** Clear the service request for continuation results but keep workflow context; do not emit `operationFinished` for password/conflict; emit it exactly once after successful retry, user cancellation, or unrecoverable failure.
- [ ] **Step 3: Make continuation cancellation terminal.** `cancelArchivePassword()` and `cancelArchiveConflict()` clear popups, clear transient secret/context, set state cancelled, and publish the normal Cancelled result/presentation. Active request cancellation must also map to cancelled rather than failed.
- [ ] **Step 4: Remove presenter heuristics.** Use the structured snapshot state, keep waiting states visible/suspended without Completed/Failed, copy real archive counts/progress, and use the existing Cancelled presentation for all archive cancellation routes.
- [ ] **Step 5: Run focused C++/QML tests and commit.**

```bash
rtk ctest --test-dir build/debug -R 'archive_controller|app_state_facade|operation_progress_presenter' --output-on-failure
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
rtk git add apps/explorer/src/controllers apps/explorer/qml apps/explorer/tests/cpp/tst_archive_controller.cpp apps/explorer/tests/cpp/tst_operation_progress_presenter.cpp apps/explorer/tests/qml/test_explorer_qml.py
rtk git commit -m "fix: preserve archive continuation and cancellation lifecycle"
```

### Task 7: Add bounded password transport and verify the integrated path

**Files:**
- Modify: `apps/explorer/src/backend/backend_transport.h`
- Modify: `apps/explorer/src/backend/persistent_worker_transport.{h,cpp}`
- Modify: `apps/explorer/src/backend/rust_backend_client.{h,cpp}`
- Modify: `apps/explorer/backend/src/worker.rs`
- Modify: `apps/explorer/backend/src/archive.rs`
- Modify: `apps/explorer/tests/cpp/tst_persistent_worker_transport.cpp`
- Modify: `apps/explorer/tests/cpp/tst_backend_client.cpp`
- Modify: `apps/explorer/tests/cpp/tst_archive_controller.cpp`

**Interfaces:**
- `BackendTransport::start(const QStringList &, const QByteArray &stdinPayload = {})` remains source-compatible for existing callers.
- Worker request JSON has a bounded optional `stdinPayload`; the payload is written only to the operation child's stdin and never appended to `arguments`.
- Archive JSON omits `password`; the archive operation reads one bounded secret body from stdin and clears it after parsing.

- [ ] **Step 1: Add transport red tests.** Assert a started archive request's `arguments` contain no password, the worker child receives the payload through stdin, payload size is bounded, and ordinary requests retain null stdin behavior.
- [ ] **Step 2: Implement payload transport.** Extend pending transport data, serialize a bounded JSON payload field, pipe stdin only when present, write/close it immediately after spawn, and reject oversize payloads without starting the operation. Preserve timeout-disabled archive semantics.
- [ ] **Step 3: Consume the secret in Rust.** Parse the request without a password field, read at most the configured limit from stdin, trim only the transport terminator, and use the in-memory value for inspection/provider arguments. Never include it in progress, errors, logs, or the Astrea child argv.
- [ ] **Step 4: Run transport/backend/controller tests and commit.**

```bash
rtk ctest --test-dir build/debug -R 'persistent_worker_transport|backend_client|archive_controller' --output-on-failure
rtk git add apps/explorer/src/backend apps/explorer/backend/src/worker.rs apps/explorer/backend/src/archive.rs apps/explorer/tests/cpp
rtk git commit -m "fix: keep archive passwords out of operation argv"
```

### Task 8: Full verification, provider qualification, and final review

**Files:**
- Modify: relevant Markdown files only for measured results and EOF whitespace cleanup.
- Do not create a build directory or alternate Rust target directory.

- [ ] **Step 1: Build using the existing configured directory.**

```bash
rtk run cmake --preset debug
rtk run cmake --build build/debug --parallel 2
```

- [ ] **Step 2: Run all required verification commands.**

```bash
rtk ctest --test-dir build/debug --output-on-failure
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
rtk run python3 scripts/verify_orbit_source_gate.py
rtk git diff --check
```

- [ ] **Step 3: Record provider availability and skips.** Run `command -v bsdtar`, `command -v 7z`, and `command -v rar`; report unavailable real-provider tests as skipped, never passed.
- [ ] **Step 4: Measure the required performance data.** Record same-parent source size, temporary bytes created before compression (must be zero clone bytes), elapsed time, TAR.ZST thread plan/elapsed comparison when reproducible, cancellation latency, residual staging bytes, and Explorer navigation responsiveness during a large job.
- [ ] **Step 5: Inspect the final anti-pattern checklist.** Search with `rtk rg` for destination-container deletion, shared Extract functions, `remove_dir_all` on user containers, unconditional 7z materialization, uninterruptible `fs::copy`, ignored ZIP profiles, link-as-encryption parsing, continuation terminal signals, password in `--json` arguments, ambiguous capabilities, compound suffix corruption, unsafe name path joining, or any generic Utility archive route.
- [ ] **Step 6: Fix the two Markdown EOF whitespace issues, review the diff/status, and commit the final qualification note if changed.**

```bash
rtk git status --short --branch
rtk git diff --stat
rtk git diff --check
rtk git log -12 --oneline
```

