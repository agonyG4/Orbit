# Async Directory Metrics for Explorer Properties Implementation Plan

> **For the implementer:** Execute this plan task-by-task in the current checkout. Keep `build/debug` as the only build tree and use focused tests after each boundary.

**Goal:** Add on-demand recursive folder metrics for Explorer Properties without affecting ordinary listing responsiveness, and expose correct aggregate metrics for multi-selection.

**Architecture:** Add a Rust `directory-metrics` operation with incremental no-follow traversal and coalesced progress. Add a dedicated persistent worker/client pair in `ExplorerApplication`, a typed `DirectoryMetricsService`, and AppState request-id state consumed by both FileContextMenu and Sidebar Properties. Keep fast utility Properties metadata separate and preserve unknown folder sizes in list/preview views.

**Tech stack:** Rust 2024 backend, Qt 6 C++ native boundary, QML, CMake/CTest, Python source-level QML tests.

### Task 1: Lock the Rust metrics contract with failing tests

Files:
- Add `apps/explorer/backend/src/directory_metrics.rs` tests before implementation.
- Update `apps/explorer/backend/src/main.rs` only as needed to register the test module/operation.

Tests:
- empty, regular file, nested, hidden, and empty nested directories;
- symlink-to-file, symlink-to-directory, cyclic symlink, duplicate/overlapping roots;
- multi-root aggregation, disappearing/injected-unreadable entries, cancellation;
- checked byte overflow, 10,000-entry fixture, and bounded progress emission.

Run the backend directory test target and confirm the new tests fail because the operation does not yet exist.

### Task 2: Implement bounded incremental Rust traversal and wire the CLI operation

Files:
- Add `apps/explorer/backend/src/directory_metrics.rs`.
- Update `apps/explorer/backend/src/main.rs`.
- Update `apps/explorer/backend/src/utility.rs` to mark directory basic size as unknown (`sizeKnown=false`) while retaining immediate metadata and child count.
- Reuse the existing remote-listing predicate from `entries.rs` without changing listing traversal.

Implement a stack-based walk using `symlink_metadata`, checked byte accumulation, Unix device/inode directory identities, cancellation marker checks, deterministic test fault hooks, bounded progress, structured partial/failed/cancelled records, and no full-file reads or path collection. The CLI should accept `directory-metrics --json <request>` and emit progress lines plus one terminal result.

Run all focused Rust backend tests, including the existing listing/utility tests, and verify ordinary directory entries still carry unknown/zero listing size.

### Task 3: Add typed native transport/client protocol support

Files:
- Update `apps/explorer/src/backend/backend_types.h`.
- Update `apps/explorer/src/backend/rust_backend_client.h/.cpp`.
- Update `apps/explorer/src/backend/fake_backend_client.h/.cpp`.

Add request/progress/result types, client encoding/decoding, streamed progress handling, terminal result handling, cancellation/error mapping, and fake-client helpers. Keep environment/history/archive/Windows boundaries unchanged.

Tests:
- request JSON carries all paths;
- progress, success, partial, failed, and cancelled records decode into typed values;
- malformed protocol is rejected;
- no unrelated operation starts recursive metrics.

Run the focused backend client tests.

### Task 4: Add the dedicated DirectoryMetricsService and worker lane

Files:
- Add `apps/explorer/src/services/directory_metrics_service.h/.cpp`.
- Update `apps/explorer/CMakeLists.txt`.
- Update `apps/explorer/src/explorer_application.cpp`.

Make the service replace/cancel one active request, filter stale request ids, and expose typed progress/finished/failed signals. Create an independent persistent transport/client pair with no ordinary 30-second timeout. Add native service tests for two targets, cancellation, stale signals, multi-path requests, and result forwarding. Add a deterministic two-worker transport regression with a slow metrics fixture and a fast interactive request.

Run the service and transport tests.

### Task 5: Expose native AppState state and preserve basic Properties immediacy

Files:
- Update `apps/explorer/src/controllers/app_state_facade.h/.cpp`.
- Update `apps/explorer/qml/AppState.qml`.
- Update AppState tests.

Inject the metrics service, expose request-id/running/state/bytes/counts/unreadable/scanned/error properties, and add request/cancel invokables. Reset state for each new request and ignore late signals. Keep single-file utility Properties immediate and make directory basic metadata return `sizeKnown=false`. Add AppState tests for complete, partial, cancelled, failed, stale, and multi-path state transitions.

### Task 6: Update both Properties surfaces and i18n without touching listing/preview behavior

Files:
- Update `apps/explorer/qml/components/common/FileContextMenu.qml`.
- Update `apps/explorer/qml/components/layout/Sidebar.qml`.
- Update `shared/qml/Astrea/I18n/en_US.json` and `pt_BR.json`.
- Update `apps/explorer/tests/qml/test_explorer_qml.py` with source/runtime coverage.

Start metrics for single directories and all multi-selections, never request only the first selected path, cancel on close/replacement, and ignore local stale ids. Show localized calculating/live counts, exact success, and “at least” partial size with warning. Leave regular files immediate, Preview Panel directory size unknown, and FileListView directory cells as `—`.

Run QML/source tests and i18n validation.

### Task 7: Full verification and review gates

Run focused Rust, client/service/AppState, QML, and CTest suites first, then the complete configured Rust workspace, CTest, QML, i18n validation, source gate, and `git diff --check`. Inspect the final diff and source for recursive listing, interactive-worker traversal, symlink following, stale-result overwrite, unbounded path collection, fake percentages, ordinary file/desktop regressions, retired Windows runtime references, direct Proton execution, environment/history leaks, shell construction, and unrelated Archive/Windows changes. Record synthetic fixture timing, progress-event count, and cancellation latency; report real provider availability separately.

