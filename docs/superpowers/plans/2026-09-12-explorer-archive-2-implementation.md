# Explorer Archive 2.0 Implementation Plan

> For inline execution: Execute this plan in the current checkout with the executing-plans skill. Do not create a worktree, subagent, or alternate build directory. Each task ends with focused verification and a commit.

Goal: Replace the generic archive utility path with a dedicated, capability-driven Archive Operation protocol and usable Explorer workflow.

Architecture: Add typed archive request/progress/result models and archive methods/signals to the backend client, run them over a second long-job PersistentWorkerTransport, and expose them through ArchiveOperationService to a stateful ArchiveController. Replace archive.rs with provider planning, bounded process execution, structured JSONL, private staging, security validation, and atomic publication. Migrate QML to capability-driven compression/extraction actions while preserving compatibility aliases.

Tech Stack: C++17, Qt 6.11, Qt Test/QML source tests, Rust 2024, serde/serde_json, mature external providers (bsdtar, 7z, optional rar), CMake preset debug, Rust target build/debug/cargo-target.

## Global Constraints

- Use the existing checkout; do not create a worktree.
- Reuse build/debug; do not create another build tree or Rust target directory.
- Do not move File Operations onto the archive lane.
- Do not add a large Rust codec/archive dependency graph.
- Do not touch Preview Pipeline 2.0, icon/theme resolution, ModelAdapters, drag MIME, MacTahoe precedence, Windows .exe compatibility, or general File Operations 2.0.
- Archive cancellation must terminate/reap the provider and remove staging before reporting cancelled.
- Archive UI decisions must use structured states/capabilities, never provider stderr text.
- Write tests before production changes and run the focused test after each red/green cycle.

## File map

- apps/explorer/src/backend/backend_types.h: archive request/progress/result/capability value types and metatypes.
- apps/explorer/src/backend/persistent_worker_transport.{h,cpp}: explicit non-positive timeout semantics.
- apps/explorer/src/backend/rust_backend_client.{h,cpp}: archive request encoding and JSONL decoding.
- apps/explorer/src/backend/fake_backend_client.{h,cpp}: archive test controls.
- apps/explorer/src/services/archive_operation_service.{h,cpp}: single-active archive lane service.
- apps/explorer/src/controllers/archive_controller.{h,cpp}: archive workflow state and continuations.
- apps/explorer/src/explorer_application.cpp: dedicated archive transport/client/service wiring.
- apps/explorer/backend/src/archive.rs: archive engine, providers, staging, security, progress, cancellation, tests.
- apps/explorer/backend/src/main.rs and utility.rs: dedicated dispatch and removal of old archive utility dispatch.
- apps/explorer/qml/components/common/FileContextMenu.qml: selection-aware Compress… and extraction actions.
- apps/explorer/qml/components/common/OperationProgressPresenter.qml: real archive progress.
- apps/explorer/qml/state/FileOperationsState.qml, AppState.qml, Main.qml: compatibility aliases/dialogs.
- apps/explorer/tests/cpp/*: protocol, transport, service, controller, presenter regressions.
- apps/explorer/tests/qml/test_explorer_qml.py: source/runtime QML assertions.
- apps/explorer/CMakeLists.txt: new sources/tests and target links.

---

### Task 1: Establish the typed archive protocol and timeout contract

Files:
- Modify: apps/explorer/src/backend/backend_types.h
- Modify: apps/explorer/src/backend/rust_backend_client.{h,cpp}
- Modify: apps/explorer/src/backend/fake_backend_client.{h,cpp}
- Modify: apps/explorer/src/backend/persistent_worker_transport.{h,cpp}
- Test: apps/explorer/tests/cpp/tst_backend_client.cpp
- Test: apps/explorer/tests/cpp/tst_persistent_worker_transport.cpp
- Modify: apps/explorer/CMakeLists.txt

Interfaces:
- ArchiveOperationRequest { kind, sources, archivePath, destination, format, profile, password, conflictPolicy }.
- ArchiveCapability { id, label, extension, createSupported, extractSupported, profiles, passwordSupported, provider }.
- ArchiveOperationProgress { requestId, operation, phase, doneCount, totalCount, bytesDone, bytesTotal, progress, percent, currentPath, currentName, statusText }.
- ArchiveOperationResult { requestId, operation, state, errorCode, errorMessage, destination, capabilities, phase, counts, bytes }.
- IRustBackendClient::archiveOperation(const ArchiveOperationRequest&) plus archiveOperationProgress and archiveOperationReady signals.
- PersistentWorkerTransportOptions::requestTimeoutMs <= 0 means no request wall-clock timer for that transport instance.

- [ ] Step 1: Add failing transport regression. Add noTimeoutRequestSurvivesNormalBoundaryAndStillCancels() using the existing Python worker helper, set requestTimeoutMs = 0, wait past 300 ms, assert no timeout, then cancel and assert one cancelled failure.
- [ ] Step 2: Run the focused transport test and verify the new test fails because the current code starts a zero-duration QTimer.
- [ ] Step 3: Add failing client protocol tests. Cover archive request argument encoding, one progress JSON object, a terminal success result, password-required, destination-conflict, and stale completion filtering.
- [ ] Step 4: Run the focused backend-client test and verify it fails because the archive request kind/signals/decoders do not exist.
- [ ] Step 5: Implement the minimum typed structs, metatypes, client request kind, encoder, JSONL decoder, fake controls, and conditional timeout start. Decode event=progress and event=result; preserve bytesTotal = -1 for unknown values; map malformed payloads to decode_error.
- [ ] Step 6: Run focused tests.

    rtk ctest --test-dir build/debug -R 'backend_client|persistent_worker_transport' --output-on-failure

- [ ] Step 7: Commit.

    rtk git add apps/explorer/src/backend apps/explorer/tests/cpp/tst_backend_client.cpp apps/explorer/tests/cpp/tst_persistent_worker_transport.cpp apps/explorer/CMakeLists.txt
    rtk git commit -m "feat: add archive operation protocol"

### Task 2: Build the Rust provider plan and capability catalog

Files:
- Modify: apps/explorer/backend/src/archive.rs
- Modify: apps/explorer/backend/src/main.rs
- Modify: apps/explorer/backend/Cargo.toml
- Modify: apps/explorer/backend/Cargo.lock
- Test: apps/explorer/backend/src/archive.rs unit tests

Interfaces:
- pub fn run_operation(args: &[String]) -> Result<(), String> accepts a JSON request argument and writes JSONL events.
- discover_capabilities() -> Vec<Capability> probes functional provider availability without exposing untested RAR create.
- plan_create(format: FormatId, profile: Profile, provider: ProviderAvailability) -> Result<CreatePlan, ArchiveError> is the format authority.
- plan_extract(archive: &Path, password: Option<&str>) -> Result<ExtractPlan, ArchiveError> distinguishes readable from creatable formats.

- [ ] Step 1: Write failing plan tests for ZIP, 7Z, TAR, TAR.GZ, TAR.XZ, and TAR.ZST asserting the returned provider/format/filter/extension plan; assert an unavailable provider returns provider-unavailable; assert no RAR create capability without a functional RAR provider.
- [ ] Step 2: Run the Rust archive tests and verify they fail because the new plan types/functions are absent.
- [ ] Step 3: Implement stable format/profile/provider enums and explicit plan mappings. Use bsdtar --format=zip, bsdtar --format=ustar plus -z/-J/--zstd for tar filters where supported, and 7z -t7z for 7Z. Map profiles to tested levels; TAR returns no profile list.
- [ ] Step 4: Implement capability discovery with Command::new(provider).arg("--version").output() only for short probes, validate executable success, and return JSON capability objects with create/extract distinction.
- [ ] Step 5: Run the focused Rust tests.

    rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive::tests::create_plan -- --nocapture

- [ ] Step 6: Commit.

    rtk git add apps/explorer/backend/Cargo.toml apps/explorer/backend/Cargo.lock apps/explorer/backend/src/archive.rs apps/explorer/backend/src/main.rs
    rtk git commit -m "feat: add archive provider planning"

### Task 3: Implement bounded provider execution, staging, source semantics, and cancellation

Files:
- Modify: apps/explorer/backend/src/archive.rs
- Test: apps/explorer/backend/src/archive.rs unit tests
- Test: capability-gated Rust provider integration tests in apps/explorer/backend/src/archive.rs

Interfaces:
- create_archive(request, emit, cancellation) -> Result<ArchiveResult, ArchiveError>.
- extract_archive(request, emit, cancellation) -> Result<ArchiveResult, ArchiveError>.
- run_provider(plan, cwd, args, cancellation) -> ProviderOutcome spawns, drains bounded stdout/stderr, polls cancellation, kills/waits the provider, and returns structured status.
- create_stage(parent, request_id) -> Result<Stage, ArchiveError> creates a unique mode-0700 sibling directory and canonical output path.
- validate_member(member: &str) -> Result<(), ArchiveError> rejects absolute, parent-traversal, and link-dangerous members.

- [ ] Step 1: Add failing staging/source tests for unique request staging, canonical output suffixes, duplicate top-level basenames, mixed report.txt + photos + notes member roots, no absolute fixture prefix, and symlink source rejection.
- [ ] Step 2: Run the focused tests and verify they fail against the old single-source/suffix-driven implementation.
- [ ] Step 3: Add the request parser and source scan. Parse ordered sources from JSON, reject missing/invalid paths and symlink roots, use basenames as archive roots, compute item/byte totals when available, and emit throttled scanning progress.
- [ ] Step 4: Add unique private staging and atomic publication. Use a request identity plus monotonic/time entropy, write the real canonical archive filename inside staging, never touch the final destination until provider/list verification succeeds, implement keep-both/overwrite publication, and remove staging on every error path.
- [ ] Step 5: Add the controlled provider runner. Set LC_ALL=C, use spawn() with piped streams and bounded reader threads, poll ASTREA_CANCEL_FILE, terminate provider process/group, wait(), and return cancelled without retaining unbounded output. Keep password out of logs; document the 7z argv limitation in a code comment/test.
- [ ] Step 6: Implement explicit create invocation. Use one provider process per create request. For bsdtar, pass explicit format/filter flags and -C parent basename pairs. For 7Z, use a request input-root materialization only when needed to preserve basenames across different source parents; reject symlink traversal during materialization.
- [ ] Step 7: Add failing extraction safety tests for ../escape, nested/../../escape, absolute Unix paths, link rejection, malformed provider output, staging cleanup, and existing destination preservation after failed replacement.
- [ ] Step 8: Implement inspect -> validate -> password/provider resolution -> staged extract -> verify -> publish. Use bsdtar -tf/-tvf or 7z l with bounded capture; represent password-required/bad-password structurally; keep extraction policies to keep-both/overwrite; validate the resolved target with path components, not string prefixes.
- [ ] Step 9: Add deterministic cancellation helper-provider tests that stay active until ASTREA_CANCEL_FILE is observed; assert provider termination/reaping, staging removal, unchanged final destination, and cancelled result.
- [ ] Step 10: Add capability-gated real-provider tests for available create/list/extract paths and explicitly print skipped providers. Include TAR.ZST only when bsdtar advertises the needed support.
- [ ] Step 11: Run the Rust archive suite.

    rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml archive -- --nocapture

- [ ] Step 12: Commit.

    rtk git add apps/explorer/backend/src/archive.rs
    rtk git commit -m "feat: implement staged archive providers"

### Task 4: Add the dedicated ArchiveOperationService and worker lane

Files:
- Create: apps/explorer/src/services/archive_operation_service.{h,cpp}
- Modify: apps/explorer/src/explorer_application.cpp
- Modify: apps/explorer/src/explorer_application.h only if lifetime helpers are required
- Modify: apps/explorer/CMakeLists.txt
- Test: apps/explorer/tests/cpp/tst_archive_operation_service.cpp

Interfaces:
- ArchiveOperationService(IRustBackendClient*, QObject*).
- BackendRequestId start(const ArchiveOperationRequest&) accepts one job and records the request identity.
- void cancel(BackendRequestId) forwards only the active identity.
- Signals: progress(id, ArchiveOperationProgress), finished(id, ArchiveOperationResult), failed(id, BackendError).

- [ ] Step 1: Add failing service tests for one active job, progress forwarding, completion, cancellation, stale progress/result rejection, state cleanup, and immediate next-job start.
- [ ] Step 2: Run the new service test and verify it fails because the service/files are absent.
- [ ] Step 3: Implement the service with an active-id set containing at most one request. Remove the ID before terminal signal emission, reject second starts with a structured busy result or queue one explicit next job, and ignore all stale signals.
- [ ] Step 4: Wire the lane in ExplorerApplication. Construct PersistentWorkerTransport archiveTransport with the same backend program and requestTimeoutMs = 0, construct a separate RustBackendClient archiveBackendClient, construct ArchiveOperationService archiveOperationService, and pass it to ArchiveController. Leave the existing transport/client construction unchanged.
- [ ] Step 5: Run focused service/transport tests and commit.

    rtk ctest --test-dir build/debug -R 'archive_operation_service|persistent_worker_transport' --output-on-failure
    rtk git add apps/explorer/src/services/archive_operation_service.h apps/explorer/src/services/archive_operation_service.cpp apps/explorer/src/explorer_application.cpp apps/explorer/src/explorer_application.h apps/explorer/CMakeLists.txt apps/explorer/tests/cpp/tst_archive_operation_service.cpp
    rtk git commit -m "feat: isolate archive jobs on a dedicated worker lane"

### Task 5: Refactor ArchiveController around structured workflow results

Files:
- Modify: apps/explorer/src/controllers/archive_controller.{h,cpp}
- Modify: apps/explorer/src/controllers/app_state_facade.{h,cpp}
- Modify: apps/explorer/tests/cpp/tst_archive_controller.cpp
- Modify: apps/explorer/tests/cpp/tst_app_state_compatibility.cpp if aliases change

Interfaces:
- startArchiveCreate(const QStringList &sources, const QString &archivePath, const QString &format, const QString &profile, const QString &conflictPolicy).
- startArchiveExtraction(const QString &path, const QString &folderName, const QString &destination = QString()).
- submitArchivePassword, submitArchiveConflict, cancelArchivePassword, cancelArchiveConflict, and cancelArchiveOperation retain the logical identity.
- Compatibility wrapper startFolderCompression(path, format) delegates to a one-source create request until all callers migrate.

- [ ] Step 1: Replace test-only continuation arrangements with failing service-result tests. Drive password-required, bad-password, valid-password success, destination-conflict, keep-both/overwrite continuation, stale completion, cancellation, and real progress through FakeRustBackendClient archive signals.
- [ ] Step 2: Run the focused controller test and verify it fails because the current controller listens to FilesystemService::operationFinished and rejects/does not receive structured states.
- [ ] Step 3: Implement controller state. Store operation kind, active request, phase, real progress/percent/counts/bytes/current item/status, destination, terminal state, structured error, and continuation context. Clear the active request before publishing terminal state to preserve re-entrant next-job behavior.
- [ ] Step 4: Map structured results exactly. password-required shows the password prompt without generic failure; bad-password restores the prompt with passwordError; destination-conflict shows only supported policies; cancelled is terminal without an error; all other error states become structured failure text.
- [ ] Step 5: Remove FilesystemService archive connections and delete archive methods after all callers migrate. Keep generic utility completion for remaining light utilities only.
- [ ] Step 6: Run focused controller/facade tests and commit.

    rtk ctest --test-dir build/debug -R 'archive_controller|app_state_compatibility|app_state_facade' --output-on-failure
    rtk git add apps/explorer/src/controllers apps/explorer/src/services/filesystem_service.* apps/explorer/tests/cpp/tst_archive_controller.cpp apps/explorer/tests/cpp/tst_app_state_compatibility.cpp
    rtk git commit -m "feat: migrate archive controller to structured jobs"

### Task 6: Migrate QML to capabilities, selection-aware Compress…, and safe extraction actions

Files:
- Modify: apps/explorer/qml/components/common/FileContextMenu.qml
- Modify: apps/explorer/qml/state/FileOperationsState.qml
- Modify: apps/explorer/qml/AppState.qml
- Modify: apps/explorer/qml/Main.qml
- Modify: apps/explorer/tests/qml/test_explorer_qml.py
- Add/update: QML dialog component under apps/explorer/qml/components/common/ if keeping the dialog separate improves testability

Interfaces:
- QML reads archiveCapabilities from native state; each capability is backend-provided.
- QML calls startArchiveCreate(sources, baseName, formatId, profile, policy).
- QML calls startArchiveExtraction(path, folderName, destination) and exposes Extract Here, Extract to…, and Extract… within the existing folder destination model.

- [ ] Step 1: Add failing QML source tests for regular-file compression, folder compression, selected multi-source and clicked-unselected semantics, mixed sources, capability-driven format list, TAR.ZST, absent RAR create, profile visibility/TAR hiding, extraction actions, real password/conflict state, and real progress fields.
- [ ] Step 2: Run the QML tests and verify the expected failures against itemIsDir, compressionFormats, rarAvailable, regex authority, and fake presenter values.
- [ ] Step 3: Replace the nested submenu with a Compress… dialog. Use AppState.isPathSelected and selectedPaths to build source lists; use clicked path alone if it is not in a multi-selection; default to basename or Archive; update canonical extension only when the base name is not user-edited or already has a supported archive suffix.
- [ ] Step 4: Bind formats/profiles to backend capability properties. Remove checkExecutable("rar"), the hardcoded array, and RAR-specific QML checks. Disable compression only when no create capability or invalid target context exists.
- [ ] Step 5: Implement extraction actions and remove Merge. Use backend extractSupported capability as authority with extension as a hint; preserve current-folder and archive-name destinations; hide Merge rather than exposing an inert action.
- [ ] Step 6: Project new archive state through FileOperationsState/AppState while preserving existing archiveExtraction* aliases. Never expose password values through native state after submission; keep the password only in the transient QML field.
- [ ] Step 7: Run QML tests and commit.

    rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
    rtk git add apps/explorer/qml apps/explorer/tests/qml/test_explorer_qml.py
    rtk git commit -m "feat: add capability-driven archive actions"

### Task 7: Make common progress presenter consume real archive values

Files:
- Modify: apps/explorer/qml/components/common/OperationProgressPresenter.qml
- Modify: apps/explorer/tests/cpp/tst_operation_progress_presenter.cpp
- Modify: apps/explorer/tests/qml/test_explorer_qml.py if presenter source assertions are needed

Interfaces:
- Archive snapshots provide phase, progress, percent, doneCount, totalCount, bytesDone, bytesTotal, currentName, status, state, and error.

- [ ] Step 1: Add failing presenter tests that assert a running archive snapshot with 37%/37 items is rendered as 37%/37 items and a terminal result preserves meaningful counts; assert unknown byte totals stay indeterminate only for bytes, not item progress.
- [ ] Step 2: Run the presenter test and verify it fails because _copyLive and _copyTerminal force archive zero/one-item values.
- [ ] Step 3: Remove archive special cases. Copy common progress/count/remaining values, use phase-aware fallback titles, and derive terminal state from structured state rather than provider/error text.
- [ ] Step 4: Run focused presenter and QML tests, then commit.

    rtk ctest --test-dir build/debug -R operation_progress_presenter --output-on-failure
    rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
    rtk git add apps/explorer/qml/components/common/OperationProgressPresenter.qml apps/explorer/tests/cpp/tst_operation_progress_presenter.cpp apps/explorer/tests/qml/test_explorer_qml.py
    rtk git commit -m "fix: present real archive progress"

### Task 8: Remove obsolete archive utility plumbing and update build/test coverage

Files:
- Modify: apps/explorer/backend/src/utility.rs
- Modify: apps/explorer/backend/src/main.rs
- Reduce: apps/explorer/src/services/filesystem_service.{h,cpp} archive methods only
- Modify: apps/explorer/tests/cpp/tst_backend_client.cpp
- Modify: apps/explorer/tests/cpp/tst_archive_controller.cpp
- Modify: apps/explorer/CMakeLists.txt
- Modify: apps/explorer/tests/qml/test_explorer_qml.py

- [ ] Step 1: Search the source for legacy archive symbols with rtk rg and add migration assertions that no production caller uses archiveCompress, archiveExtract, archive-compress, or archive-extract utility dispatch.
- [ ] Step 2: Remove only dead archive utility dispatch and compatibility tests after the new path is green. Keep utility for remaining operations.
- [ ] Step 3: Add backend/client/service/controller test targets to CMake and ensure fake clients compile with the new interface.
- [ ] Step 4: Run focused backend and C++ suites.

    rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml
    rtk ctest --test-dir build/debug -R 'backend_client|archive_operation_service|archive_controller|persistent_worker_transport|operation_progress_presenter' --output-on-failure
    rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py

- [ ] Step 5: Commit.

    rtk git add apps/explorer/backend/src/main.rs apps/explorer/backend/src/utility.rs apps/explorer/src apps/explorer/tests apps/explorer/CMakeLists.txt
    rtk git commit -m "refactor: remove legacy archive utility path"

### Task 9: Build, full verification, and runtime qualification

Files:
- Modify: docs/superpowers/specs/2026-09-12-explorer-archive-2-design.md only if measured provider/limitation notes need an evidence update.
- Add: no new build directory; runtime logs/results may be stored outside the repository or summarized in the final report.

- [ ] Step 1: Configure/build using the existing debug preset.

    rtk run "cmake --preset debug"
    rtk run "cmake --build build/debug --parallel 2"

- [ ] Step 2: Run the complete verification commands.

    rtk ctest --test-dir build/debug --output-on-failure
    rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml
    rtk python3 scripts/verify_orbit_source_gate.py
    rtk git diff --check

- [ ] Step 3: Record runtime provider availability. At minimum run command -v bsdtar, command -v 7z, and command -v rar; report unavailable optional providers as skips.
- [ ] Step 4: Exercise the actual Explorer where providers are available. Measure ZIP create, TAR.ZST create, ZIP/TAR.ZST extract, 7Z create/extract, cancellation latency, and memory growth. During a large create, navigate/list/scroll/select another directory and record idle-vs-running responsiveness. Exercise existing destination, password, traversal rejection, and mixed-selection cases.
- [ ] Step 5: Inspect the final source for the explicit anti-pattern checklist: ignored format, .tmp-* suffix authority, folder-only gating, hardcoded QML format list/RAR probing, generic utility routing, fake archive progress, shared worker lane, global timeout changes, provider child leaks, unbounded capture, partial final output, staging collisions, unsafe paths/links, visible Merge without implementation, text-only password/conflict states, QML recursion, one process per source, and unrelated scope changes.
- [ ] Step 6: Review status/diff, commit any final documentation-only qualification update, and create the final implementation report with exact commands/results, skips, measurements, and known limitations.

    rtk git status --short --branch
    rtk git diff --stat
    rtk git log -12 --oneline

