# Phase 3.3.2 Recent Parity and Production Sequencing Plan

## Design reference

`docs/superpowers/specs/2026-08-09-phase-3-3-2-recent-parity-sequencing-design.md`

## Implementation steps

1. Establish the red tests before production edits.
   - Replace direct `items`, `startSave()`, and generation-counter mutation in
     `tst_recent_persistence.cpp` with public `recordAccess()` calls.
   - Add assertions for serialized A->B and A->B->C saves, save failure,
     refresh only after accepted completion, stale load protection, rapid
     recording, and an event-loop completion.
   - Add native Recent tests for valid, malformed, missing, and zero timestamps;
     missing launch targets and malformed records; Finder stale-target behavior;
     and large mixed-source loading through `RecentController::load()`.
   - Run the focused tests and confirm the expected failures against the
     current implementation.

2. Correct timestamp and Finder compatibility with minimal native changes.
   - Add a timestamp parse result that distinguishes a missing/malformed value
     from an invalid record.
   - Apply filesystem-mtime fallback for recoverable launch timestamps and
     preserve valid desktop metadata.
   - Build Finder records from valid serialized identity/metadata when the
     target no longer exists, while retaining rejection of unusable records.
   - Leave launch/XBEL target validation and existing desktop qualification
     intact.

3. Bound and qualify source loading.
   - Pass the configured limit into launch-history loading.
   - Process newest-first, deduplicate accepted launch paths before the limit,
     and stop after the bounded number of retained source candidates.
   - Add a per-load desktop resolution cache without introducing a worker,
     process-based listing, or new synchronization architecture.
   - Preserve merge tie handling, source precedence, sorting, and model roles.

4. Make the persistence tests production-shaped.
   - Extend the test app with a recent-path predicate and observable refresh
     count while keeping the real `RecentState.qml` implementation.
   - Drive all state changes via `recordAccess()` and let the process double
     emit the normal completion signal.
   - Assert that only one save is active, the newest pending snapshot wins, a
     failed older save cannot refresh or roll back state, stale loads are
     ignored, and post-load behavior survives an event-loop turn.

5. Run full validation and inspect scope.
   - Configure fresh `build-phase332-debug` and `build-phase332-release`
     directories, build, and run all CTest tests.
   - Run the complete Python and Rust suites, QML lint with the recorded 995
     warning baseline, `git diff --check`, and offscreen native smoke.
   - Inspect the exact phase diff and preserve all unrelated staged Orbit paths.

6. Deliver one focused commit and source-only archive.
   - Stage only the phase-owned docs, native Recent implementation/tests, and
     any necessary phase documentation.
   - Commit exactly:
     `fix(explorer): align recent parity and production sequencing`
   - Rebuild and verify the source-only zip without compiled folders or binary
     artifacts.

## Self-review

- The plan does not migrate Recent persistence or any new QML `Process` domain.
- It tests public production behavior instead of manufacturing internal state.
- It keeps Finder, launch-history, and XBEL validation rules distinct.
- It bounds accepted launch candidates and repeated desktop resolution without
  changing visible ordering or adding a new worker framework.
- It includes clean-build, lint, smoke, repository-safety, commit, and archive
  checks required by the request.
