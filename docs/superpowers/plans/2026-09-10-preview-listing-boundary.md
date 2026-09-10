# Preview Listing Boundary Implementation Plan

> **For agentic workers:** Execute this plan inline task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove shared-thumbnail cache consumption from initial list/search enumeration and make the preview preference an explicit, consistent C++/Rust request contract.

**Architecture:** Initial list/search requests select `None` or `Direct`. `Direct` creates only a source-versioned URL for image/SVG entries from the existing extension capability check; videos remain empty. The existing exact-path `thumbnail-batch` path remains the sole production path for XDG/GIO cache validation, failure entries, generation, and immediate model hydration.

**Tech Stack:** Rust 2024, `png`/GIO/Rayon backend, Qt 6 C++ tests, CMake, and the existing `build/debug` directory.

## Global Constraints

- Preserve all concurrency, lifecycle, cache, source-version, DPR, exact-path, bound, permission, interoperability, and dependency fixes from commit `8b19959`.
- Do not redesign the thumbnail subsystem, `PreviewController`, QML, cache format, generator, or asynchronous `thumbnail-batch` lifecycle.
- Do not create a worktree, alternate build directory, or private Orbit thumbnail-cache destination.
- Initial listing/search must not call `cached_preview_url`, `read_valid_thumbnail`, `read_failure_entry`, `gio_thumbnail_snapshot`, ImageMagick, or FFmpeg.
- Keep full PNG payload validation through `png::Reader::next_frame()` in the asynchronous cache-consumption path.
- Use `None` when previews are disabled and `Direct` when previews are enabled for both list and search.
- Use `apply_patch` for edits, `rtk` for shell/test commands, and no subagents.
- Commit each independently testable task.

---

## File map

Modify:

- `apps/explorer/src/backend/backend_types.h` — add `SearchRequest.previews`.
- `apps/explorer/src/backend/rust_backend_client.cpp` — emit explicit `none`/`direct` mode arguments for list and search.
- `apps/explorer/src/controllers/navigation_controller.cpp` — copy `m_previews` into `SearchRequest`.
- `apps/explorer/backend/src/entries.rs` — replace cache-validating listing modes with `None`/`Direct`, make direct image URLs cache-free, and propagate search mode recursively.
- `apps/explorer/backend/src/thumbnails.rs` — remove the now-unused listing-facing `cached_preview_url` function while preserving async cache callers and all generator/cache behavior.
- `apps/explorer/backend/src/thumbnail_cache.rs` — add only a small `#[cfg(test)]` full-validation counter seam if needed by the listing regression test; do not alter validation semantics.
- `apps/explorer/backend/src/thumbnails.rs` — correct concurrent failure cleanup to inspect the versioned failure application directory.
- `apps/explorer/tests/cpp/tst_backend_client.cpp` — assert explicit list/search preview arguments.
- `apps/explorer/tests/cpp/tst_navigation_controller.cpp` — assert search forwards the current preview preference.

Tests are inline in the existing Rust modules; no new test binary or build tree is required.

## Task 1: Lock the C++ request contract with failing tests

**Files:**

- Modify: `apps/explorer/tests/cpp/tst_backend_client.cpp`
- Modify: `apps/explorer/tests/cpp/tst_navigation_controller.cpp`

**Interfaces:**

- `SearchRequest.previews` is a boolean matching `ListRequest.previews`.
- `RustBackendClient::search()` must receive enough information to emit
  `--preview-mode none` or `--preview-mode direct`.
- `NavigationController::startSearch()` must copy `m_previews` into the request.

- [ ] **Step 1: Add the C++ search preference assertions.**

In `BackendClientTest::forwardsLegacyCliArgumentsForListAndSearch`, set
`searchRequest.previews = false` and extend the expected search arguments with:

```cpp
QStringLiteral("--preview-mode"),
QStringLiteral("none")
```

Add a second request assertion in the same test (or a focused adjacent test)
with `searchRequest.previews = true` and the literal expected suffix:

```cpp
QStringLiteral("--preview-mode"),
QStringLiteral("direct")
```

This test must assert the actual `InMemoryTransport::startedRequests` arguments,
not only a mock invocation count.

In `NavigationControllerTest`, add a test that calls `navigation.setPreviews(false)`,
starts a search, and asserts `client.searchRequests().constLast().previews` is
`false`; repeat with `true` and assert `true`.

- [ ] **Step 2: Build and run the focused tests to verify the tests fail for the missing contract.**

Run:

```bash
rtk cmake --build build/debug --target tst_backend_client tst_navigation_controller -j2
build/debug/tst_backend_client forwardsLegacyCliArgumentsForListAndSearch
build/debug/tst_navigation_controller forwardsSearchPreviewPreference
```

Expected: compilation fails because `SearchRequest` has no `previews` field,
or the runtime assertions show search arguments lack an explicit mode. Do not
modify production code until this red result is observed.

- [ ] **Step 3: Commit the red-test checkpoint only if the repository workflow requires a test checkpoint.**

Do not commit a knowingly uncompilable tree. Keep the failing test changes in
the worktree for Task 2, then use the green commit at the end of Task 2.

## Task 2: Implement the explicit C++ list/search mode contract

**Files:**

- Modify: `apps/explorer/src/backend/backend_types.h`
- Modify: `apps/explorer/src/backend/rust_backend_client.cpp`
- Modify: `apps/explorer/src/controllers/navigation_controller.cpp`
- Test: `apps/explorer/tests/cpp/tst_backend_client.cpp`
- Test: `apps/explorer/tests/cpp/tst_navigation_controller.cpp`

**Interfaces:**

- `SearchRequest { ..., bool previews = true; }`.
- `RustBackendClient::listArguments()` appends `--preview-mode none` when
  `ListRequest.previews` is false and `--preview-mode direct` otherwise.
- `RustBackendClient::searchArguments()` appends the equivalent explicit mode
  based on `SearchRequest.previews`.
- `NavigationController::startSearch()` sets `request.previews = m_previews`.

- [ ] **Step 1: Add the field and minimal argument helper behavior.**

Add `bool previews = true;` after `foldersFirst` in `SearchRequest`. In
`rust_backend_client.cpp`, introduce no new policy abstraction; append the
literal mode pair in each existing argument builder:

```cpp
arguments.append(QStringLiteral("--preview-mode"));
arguments.append(request.previews ? QStringLiteral("direct") : QStringLiteral("none"));
```

Apply the same explicit append to `listArguments()` for both branches so an
enabled list cannot silently select an old Rust default.

- [ ] **Step 2: Forward the navigation setting.**

Set `request.previews = m_previews;` immediately after the other search listing
options in `NavigationController::startSearch()`.

- [ ] **Step 3: Run the focused C++ tests and confirm green.**

Run:

```bash
rtk cmake --build build/debug --target tst_backend_client tst_navigation_controller -j2
build/debug/tst_backend_client forwardsLegacyCliArgumentsForListAndSearch
build/debug/tst_navigation_controller forwardsSearchPreviewPreference
```

Expected: both tests pass and list/search argument vectors contain explicit
`none` or `direct` modes.

- [ ] **Step 4: Commit the C++ contract.**

```bash
rtk git diff --check
rtk git add apps/explorer/src/backend/backend_types.h apps/explorer/src/backend/rust_backend_client.cpp apps/explorer/src/controllers/navigation_controller.cpp apps/explorer/tests/cpp/tst_backend_client.cpp apps/explorer/tests/cpp/tst_navigation_controller.cpp
rtk git commit -m fix-explicit-search-preview-policy
```

## Task 3: Lock the Rust listing/search boundary with failing tests

**Files:**

- Modify: `apps/explorer/backend/src/entries.rs`
- Test: inline `#[cfg(test)]` module in `apps/explorer/backend/src/entries.rs`

**Interfaces:**

- `PreviewMode::{None, Direct}` only.
- `parse_preview_mode_arg()` accepts `none` and `direct`, rejects `cached` and
  `full`, and defaults internal callers to `Direct`.
- `preview_url_for_mode()` returns an empty string for `None` and calls a
  cache-free direct-image helper for `Direct`.
- `search_dir_recursive_with_preview()` passes its selected mode to every
  recursive invocation.

- [ ] **Step 1: Add explicit mode/parser tests.**

Replace the old parser test with literal expectations:

```rust
assert_eq!(parse_preview_mode_arg(&[]).unwrap(), PreviewMode::Direct);
assert_eq!(
    parse_preview_mode_arg(&["--preview-mode".into(), "none".into()]).unwrap(),
    PreviewMode::None
);
assert_eq!(
    parse_preview_mode_arg(&["--preview-mode=direct".into()]).unwrap(),
    PreviewMode::Direct
);
assert!(parse_preview_mode_arg(&["--preview-mode=cached".into()]).is_err());
assert!(parse_preview_mode_arg(&["--preview-mode=full".into()]).is_err());
```

- [ ] **Step 2: Add the direct-image/video and preview-disabled behavior tests.**

Create one temporary directory containing `photo.png`, `clip.mp4`, and a
deliberately malformed PNG at a cache-shaped path. Construct entries with
`PreviewMode::Direct` and `PreviewMode::None` and assert:

```rust
assert!(direct_image.preview_url.contains("sourceVersion="));
assert!(direct_image.preview_url.contains("cacheTier=direct"));
assert!(direct_video.preview_url.is_empty());
assert!(none_image.preview_url.is_empty());
assert!(none_video.preview_url.is_empty());
```

The assertions name the production break: reintroducing cache lookup into the
direct mode would make the image depend on the malformed shared thumbnail,
while changing direct mode to generator behavior would give the video a URL.

- [ ] **Step 3: Add recursive search mode tests.**

Use `search_dir_recursive_with_preview()` over a temporary root containing one
matching image and one matching video. Run it once with `PreviewMode::None` and
once with `PreviewMode::Direct`; assert all result URLs are empty in the first
run, while only the image has a direct identity in the second. This catches a
recursive call that silently reverts to a cache-enabled/default mode.

- [ ] **Step 4: Run the Rust tests and verify the intended red failures.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml entries::tests -- --nocapture
```

Expected: the new tests fail because the old enum still accepts/defaults to
`Cached`/`Full` and direct listing still consults `cached_preview_url()`.

## Task 4: Implement cache-free initial list/search enumeration

**Files:**

- Modify: `apps/explorer/backend/src/entries.rs`
- Modify: `apps/explorer/backend/src/thumbnails.rs`
- Modify: `apps/explorer/backend/src/thumbnail_cache.rs` only if the test seam in Task 5 is needed

**Interfaces:**

- `PreviewMode` contains exactly `None` and `Direct`.
- `thumbnails::preview_url()` is cache-free and returns a source-versioned
  direct identity only for image/SVG paths; videos return an empty string.
- `cached_preview_url()` has no remaining production caller and is deleted.
- `read_sorted_entries()` defaults to `PreviewMode::Direct`.
- `run_list()` and `run_search()` honor explicit `--preview-mode` values.

- [ ] **Step 1: Replace the enum and parser defaults.**

Change the enum and parser mapping to:

```rust
enum PreviewMode {
    None,
    Direct,
}

match value {
    "none" => Some(Self::None),
    "direct" => Some(Self::Direct),
    _ => None,
}
```

Initialize `parse_preview_mode_arg()` with `PreviewMode::Direct`. Change
`read_sorted_entries()` and the test-only `search_dir_recursive()` helper to
use `PreviewMode::Direct` instead of `Full`.

- [ ] **Step 2: Make the thumbnail preview helper cache-free.**

In `thumbnails.rs`, remove the first `cached_preview_url()` lookup from
`preview_url()`. Keep its existing image/SVG capability check and
`preview_url_identity()` construction, so direct URLs remain source-versioned.
Delete `cached_preview_url()` after confirming no non-test caller remains.
Do not change `process_item()`, `source_is_readable()`, GIO lookup, failure
handling, generation, or `read_valid_thumbnail()`.

- [ ] **Step 3: Update the mode dispatch.**

Change `preview_url_for_mode()` to:

```rust
match mode {
    PreviewMode::None => String::new(),
    PreviewMode::Direct => thumbnails::preview_url(path, is_dir, modified_ms, size),
}
```

Keep `search_dir_recursive_with_preview()` passing its `preview_mode` in the
recursive call, and make `run_search()` use the parsed mode already supplied
by `parse_preview_mode_arg()`.

- [ ] **Step 4: Run the Rust entry tests and confirm green.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml entries::tests -- --nocapture
```

Expected: explicit parser, direct image/video, disabled, and recursive search
tests pass. Existing entry/search behavior tests must remain green.

- [ ] **Step 5: Commit the listing boundary.**

```bash
rtk git diff --check
rtk git add apps/explorer/backend/src/entries.rs apps/explorer/backend/src/thumbnails.rs
rtk git commit -m fix-cache-free-initial-preview-listing
```

## Task 5: Add deterministic zero-validation instrumentation and regressions

**Files:**

- Modify: `apps/explorer/backend/src/thumbnail_cache.rs`
- Modify: `apps/explorer/backend/src/entries.rs`

**Interfaces:**

- Under `#[cfg(test)]`, `png_text()` increments a private atomic counter only
  for deterministic test observation.
- Test-only `reset_full_validation_count()` and
  `full_validation_count()` are `pub(crate)` and have no production build
  behavior.

- [ ] **Step 1: Add the failing validation-count test.**

Before changing the counter seam, add an entry test that resets the counter,
lists a temporary directory with an image and video using `PreviewMode::Direct`,
then asserts:

```rust
assert_eq!(crate::thumbnail_cache::full_validation_count(), 0);
```

Also create a valid cached PNG fixture and a deliberately truncated cached PNG
fixture in the fixture setup, proving the assertion is about skipped validation
rather than cache absence. Keep the cache fixture outside the source directory
and do not add any source readability probe.

- [ ] **Step 2: Run the focused test to verify it fails for the missing seam.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml initial_listing_does_not_validate_cached_png -- --nocapture
```

Expected: compilation fails because the test-only counter helpers do not exist.

- [ ] **Step 3: Add the minimal test-only counter.**

Add an `AtomicUsize` with `Ordering::Relaxed` behind `#[cfg(test)]`, increment
it at the start of `png_text()`, and expose only reset/read helpers with
`#[cfg(test)] pub(crate)`. Do not move or weaken the existing `next_frame()`
decode.

- [ ] **Step 4: Run the deterministic boundary test and the cache validation tests.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml initial_listing_does_not_validate_cached_png -- --nocapture
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnail_cache::tests thumbnails::tests -- --nocapture
```

Expected: the listing count is zero, while valid, truncated, metadata, GIO,
failure, permission, and generator tests continue to exercise full validation.

- [ ] **Step 5: Commit the deterministic regression seam.**

```bash
rtk git diff --check
rtk git add apps/explorer/backend/src/thumbnail_cache.rs apps/explorer/backend/src/entries.rs
rtk git commit -m test-initial-listing-cache-boundary
```

## Task 6: Correct concurrent failure fixture cleanup without touching runtime behavior

**Files:**

- Modify: `apps/explorer/backend/src/thumbnails.rs`

**Interfaces:**

- `concurrent_failures_install_independent_source_scoped_entries` retains its
  four-source `Barrier` and four-worker behavior.
- Temporary-artifact enumeration targets
  `failure_application_dir(&cache)` itself, not `failure_application_dir(&cache).parent()`.

- [ ] **Step 1: Change only the cleanup assertion path.**

In the existing test, replace the `read_dir()` target with:

```rust
fs::read_dir(failure_application_dir(&cache))
```

Leave source creation, barrier, failure-entry assertions, and all production
failure-writing code unchanged.

- [ ] **Step 2: Run the deterministic concurrent failure test.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml concurrent_failures_install_independent_source_scoped_entries -- --nocapture
```

Expected: the four workers synchronize and the actual application fixture
directory contains no `.failure-input-*` leftovers.

- [ ] **Step 3: Commit the test-quality correction.**

```bash
rtk git diff --check
rtk git add apps/explorer/backend/src/thumbnails.rs
rtk git commit -m test-fix-failure-fixture-cleanup
```

## Task 7: Review every cache caller and run final verification

**Files:**

- Inspect all Explorer backend callers of `cached_preview_url`,
  `read_valid_thumbnail`, `read_failure_entry`, `gio_thumbnail_snapshot`,
  `write_standard_thumbnail`, and `write_failure_entry`.
- Modify only source/tests required by a failing verification command.

- [ ] **Step 1: Classify cache callers with structural and literal searches.**

Run:

```bash
rtk rg -n "cached_preview_url|read_valid_thumbnail|read_failure_entry|gio_thumbnail_snapshot|write_standard_thumbnail|write_failure_entry" apps/explorer/backend
```

Classify every result as async `thumbnail-batch`, cache/thumbnails test, or
specialized non-critical path. The production result must show no cache caller
reachable from `run_list`, `run_search`, `read_sorted_entries`, or
`search_dir_recursive_with_preview`.

- [ ] **Step 2: Rebuild focused targets in the existing directory.**

Run:

```bash
rtk cmake --build build/debug --target tst_backend_client tst_navigation_controller -j2
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml entries::tests thumbnail_cache::tests thumbnails::tests -- --nocapture
```

- [ ] **Step 3: Run the configured C++ and backend suites.**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
```

Also run the available real-media/cache tests:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml real_ffmpeg_generator_produces_a_readable_standard_thumbnail gio_thumbnail_interop
```

- [ ] **Step 4: Run source gates and diff hygiene.**

Run:

```bash
rtk run python3 scripts/verify_orbit_source_gate.py
rtk git diff --check
rtk rg -n "GTK|GDK|gtk|gdk" apps/explorer/backend/Cargo.toml apps/explorer/CMakeLists.txt apps/explorer/src
rtk rg -n "PreviewMode::(Cached|Full)|cached_preview_url|--preview-mode.*full|--preview-mode.*cached" apps/explorer/backend apps/explorer/src
```

Expected: no active listing/search references to removed cache-enabled modes;
the GTK/GDK check must not show a new dependency.

- [ ] **Step 5: Perform one practical performance sanity check if the fixture is available.**

Using the existing build/backend executable and a temporary directory in the
repository’s normal test area, populate many valid cache-shaped PNGs and run
the list operation once with `--preview-mode direct`. Record the command and
whether it completes without invoking cache decoding; do not claim a measured
speedup unless a before/after timing was actually collected. The deterministic
counter result remains the primary regression proof.

- [ ] **Step 6: Inspect the final diff and commit any verification-only correction.**

Review that `read_valid_thumbnail()` still calls `next_frame()`, list/search
never read XDG/GIO/failure state, search carries `previews`, videos remain
empty initially, direct image URLs remain source-versioned, async results still
hydrate the model without relisting, and no commit `8b19959` behavior changed.

If corrections are needed, add their failing test first and run the narrowest
affected command before committing. Then run:

```bash
rtk git diff --check
rtk git status --short
```

Commit only source/test corrections with:

```bash
rtk git add apps/explorer
rtk git commit -m fix-preview-listing-boundary-verification
```

Finish by reporting exact commands and exit results, the caller classification,
the final list/search policies, and any unmeasured or unavailable performance
checks.
