# Explorer Archive 2.0 Design

## Goal

Replace Explorer's synchronous, utility-shaped archive path with a dedicated long-running archive operation architecture that supports multi-source creation, explicit format/provider selection, honest progress, cancellation, safe publication, password continuations, capability-driven UI, and responsive navigation.

## Current implementation trace and verified defects

The current path is:

```text
FileContextMenu.qml
  -> AppState.qml / FileOperationsState.qml
  -> AppStateFacade
  -> ArchiveController
  -> FilesystemService
  -> IRustBackendClient::utility
  -> interactive PersistentWorkerTransport
  -> persistent worker (`serve`)
  -> backend utility/archive dispatch
  -> apps/explorer/backend/src/archive.rs
```

The graph trace and source inspection show that `FileContextMenu.qml` exposes a hardcoded compression submenu and gates creation with `itemIsDir`. `ArchiveController` owns fake archive progress and invokes `FilesystemService::archiveCompress()` / `archiveExtract()`. `FilesystemService` encodes both operations as generic utility requests. The shared Rust client decodes them as `UtilityResult`, so structured archive states are not available to the controller. `archive.rs` uses blocking `Command::output()`, always invokes `bsdtar` for creation, does not consume its advertised format argument, stages with a filename ending in `.tmp-<pid>`, and performs no real progress streaming. Extraction uses a PID-only staging directory and relies on provider output text for failures. `OperationProgressPresenter.qml` deliberately replaces archive values with an indeterminate/one-item presentation. The extraction popup offers Merge while `ArchiveController::isSupportedConflictPolicy()` rejects it.

The repository already has a persistent worker protocol with bounded output capture, cooperative cancellation markers, and a dedicated JSONL file-operation protocol. The archive replacement will reuse the transport mechanics and security principles, but will not reuse `UtilityRequest`, `FileOperationRequest`, or the interactive worker lane.

## Chosen architecture

### Dedicated archive lane

`ExplorerApplication` will construct a second `PersistentWorkerTransport` configured with the same backend executable and `requestTimeoutMs <= 0`, then a dedicated `RustBackendClient` and `ArchiveOperationService` on that lane. The existing transport/client remains responsible for navigation, search, previews, devices, light utilities, and File Operations. The archive service serializes archive jobs by accepting one active request and rejects or queues additional jobs deterministically; the initial product behavior is one heavy archive job at a time.

```text
interactive transport -> existing RustBackendClient -> navigation/search/file operations
archive transport      -> archive RustBackendClient -> ArchiveOperationService -> ArchiveController
```

The archive transport gets no wall-clock timeout, while positive timeout behavior remains unchanged for all other transports. Cancellation continues to use the request identity and the existing worker cancellation path.

### Dedicated protocol

Add typed C++ request/result models:

- `ArchiveOperationRequest`: operation kind (`create`, `extract`, `capabilities`), source list, archive/destination paths, format ID, compression profile, password, and conflict policy.
- `ArchiveOperationProgress`: phase, done/total item counts, optional byte counts, monotonic progress/percent, current path/name, and status text.
- `ArchiveOperationResult`: request identity, operation kind, terminal state, structured error code/reason, destination, and capability payload.

The wire request uses a dedicated request kind such as `archive-operation`; arguments are encoded by the archive client, not by `FilesystemService`. Rust emits throttled JSONL events with `event`, `operation`, `phase`, `doneCount`, `totalCount`, `bytesDone`, `bytesTotal`, `progress`, `percent`, `currentPath`, `currentName`, and `statusText`. Unknown byte totals are omitted or represented as unknown; item/phase progress remains honest and monotonic.

Terminal states are machine-readable: `success`, `cancelled`, `password-required`, `bad-password`, `destination-conflict`, `unsupported-format`, `provider-unavailable`, `unsafe-member`, `invalid-source`, and `provider-failed`. C++ does not inspect stderr text to choose controller state.

`ArchiveOperationService` owns dispatch, the active identity, progress forwarding, cancellation, stale result filtering, and cleanup. `ArchiveController` consumes only this service and owns logical workflow state and continuations.

## Rust archive engine

`archive.rs` will be replaced by an operation-oriented implementation with these internal boundaries:

1. capability discovery and provider planning;
2. source scanning and top-level-name validation;
3. archive inspection/member safety validation;
4. controlled provider process execution;
5. staging and atomic publication;
6. password/conflict continuation result construction.

Provider plans are authoritative and include format ID, canonical extension, provider executable, creation/extraction arguments, supported profiles, and password support. `bsdtar` is used for tar-family paths and safe inspection where supported; `7z` is used for ZIP/7Z and encrypted extraction where available; RAR creation is never advertised without a tested functional provider. Provider processes are spawned with bounded stdout/stderr readers, deterministic locale, cancellation polling, kill-and-wait cleanup, and no password logging.

Creation supports ZIP, 7Z, TAR, TAR.GZ, TAR.XZ, and TAR.ZST when capability discovery finds a working provider. Extraction supports ZIP, TAR, TGZ/TAR.GZ, TBZ2/TAR.BZ2, TXZ/TAR.XZ, TZST/TAR.ZST, 7Z, and RAR where providers permit. Product profiles are `fast`, `balanced`, and `maximum`; Rust maps them to tested provider arguments. TAR exposes no compression-level control.

Creation receives an ordered source list. Each source is validated as a regular file or directory, symlink sources are rejected unless an explicit safe-preservation policy is proven, and the archive-relative root is its basename. Duplicate top-level basenames fail before provider execution. Directory recursion remains entirely in Rust/provider code; QML does not enumerate contents and one provider process handles the complete request.

Each operation creates a unique private sibling staging directory under the destination parent, with a real canonical output filename such as `payload.tar.zst`. It verifies provider success and performs a basic list/readability check before resolving final destination conflict policy and publishing. `keep-both` chooses a unique final name, `overwrite` replaces only after successful staging/verification, and failed/cancelled operations remove staging while leaving an existing final archive untouched.

Extraction remains fully staged. It inspects and validates every member with path-component-aware checks for absolute paths, `..` traversal, and link-based escapes. Symbolic and hard links remain rejected unless a separate proof and regression set makes safe preservation possible. The destination is published only after extraction and validation. Archive Merge is removed from the UI for this closure; implemented extraction policies are `keep-both` and `overwrite`.

Password-required is a real terminal continuation result. The controller preserves archive, destination, conflict policy, and operation kind; password submission retries the logical operation. Wrong passwords return `bad-password` and keep the prompt available. Passwords are never placed in status/error telemetry or logs. Provider limitations around password transport are documented in code/tests when argv is unavoidable.

## Controller and QML contract

`ArchiveController` is refactored around operation kind, request identity, running state, phase, progress, counts, bytes, current item, status, destination, password/conflict continuation, terminal state, and structured error. Existing `archiveExtraction*` properties remain compatibility aliases. A multi-source create API replaces `startFolderCompression`; a temporary wrapper may remain only for migrated tests/callers.

Context-menu source semantics are selection-aware: if the clicked item is selected and the selection has multiple items, the complete current selection is used; otherwise only the clicked item is used. The hardcoded format list, RAR executable probe, archive-extension authority, and nested compression submenu are removed. `Compress…` opens a dialog with archive name, capability-provided format, profile where supported, and a source summary. Default names are the single source basename or `Archive` for multiple sources, with canonical extension updates that preserve an edited base name.

Extraction exposes `Extract Here`, `Extract to "<archive-name>/"`, and `Extract…` within the existing destination model. Native capability checks decide extractability; extension checks may remain only as a cheap hint. The existing password popup becomes a real state projection. Conflict UI shows only keep-both and overwrite for extraction, matching controller support. Create conflicts use explicit archive-file policies and do not reuse extraction Merge semantics.

`OperationProgressPresenter` consumes real archive phase/progress/count/byte values and no longer forces archive progress to zero or fabricates one-item completion. It coalesces naturally through the service's throttled progress cadence.

## Testing and qualification

Rust tests cover plan authority for ZIP/7Z/TAR/TAR.GZ/TAR.XZ/TAR.ZST, unique staging, canonical suffixes, atomic publication, cancellation/failure cleanup, unchanged failed replacements, mixed-source member roots, duplicate basenames, unsafe extraction members, link rejection, malformed providers, and capability combinations. Provider integrations are capability-gated and explicitly report skips.

C++ tests cover archive request encoding, JSONL progress/result decoding, structured continuation states, stale filtering, transport timeout-disabled mode, ArchiveOperationService lifecycle, ArchiveController continuations, selection semantics, and presenter values. Existing interactive transport behavior receives focused regressions for positive timeout and cancellation. QML source/runtime tests cover dialog/capability/profile/action behavior and no fake RAR.

Final verification reuses `build/debug` and includes focused Rust/C++/QML tests, the complete Explorer CTest and Rust suites, `python3 scripts/verify_orbit_source_gate.py`, and `git diff --check`. Runtime qualification records provider availability, ZIP and TAR.ZST create/extract times, navigation responsiveness during an archive, cancellation latency, and memory growth. Optional provider tests are skipped explicitly when the executable is unavailable.

## Constraints and non-goals

- No new worktree or alternate build directory.
- No Windows `.exe` compatibility work.
- No general File Operations 2.0 rewrite.
- No Preview Pipeline, icon, ModelAdapter, drag MIME, or MacTahoe changes.
- No large Rust codec dependency graph; external mature providers remain behind Rust adapters.
- No batch extraction requirement for multiple selected archives in this closure.
