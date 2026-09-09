# Preview Pipeline 2.0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Explorer's private, directory-offset thumbnail warming with a bounded exact-path preview pipeline that hydrates the active model immediately and propagates physical display requirements and real DPR through previews and icons.

**Architecture:** Extend `PreviewController` into the native preview lifecycle owner. It will schedule the latest viewport and selected-item intents, issue one bounded `thumbnail-batch` utility request at a time, reject stale generation/source results, and update `DirectoryModel` immediately. Rust will own standard XDG thumbnail cache lookup/generation; QML will report logical requirements plus the current window DPR and retain only presentation bindings.

**Tech Stack:** Rust 2024, `gio` 0.21, `md5` 0.7, `png` 0.18, Rayon 1.10, serde/serde_json, Qt 6 Core/Gui/Quick/Test, QML 2.15, ImageMagick, FFmpeg, CMake, Python unittest.

**Spec:** `docs/superpowers/specs/2026-09-09-preview-pipeline-2-design.md`

## Global Constraints

- Work only in `/home/agony/GitHub/Orbit`; preserve unrelated Typhon changes.
- Reuse `/home/agony/GitHub/Orbit/build/debug`; do not create another build tree or worktree.
- Keep the initial `std::fs` listing fast; it must not generate thumbnails or perform per-row GIO thumbnail generation.
- Use exact local paths for `thumbnail-batch`; never relist or sort a parent directory to rediscover visible rows.
- Bound each batch to 32 unique candidates and 256 KiB of encoded arguments, with one expensive thumbnail batch normally active.
- Use `$XDG_CACHE_HOME/thumbnails`, falling back to `$HOME/.cache/thumbnails`; never write new canonical thumbnails to `~/.cache/explorer/thumbnails`.
- Use the MD5 of the canonical absolute local URI as the lowercase hexadecimal cache filename.
- Validate `Thumb::URI`, `Thumb::MTime`, and optional `Thumb::Size`/`Thumb::Mimetype` before accepting a cache hit.
- Write standard PNG metadata and use an atomic same-directory rename with private cache permissions.
- Keep failure and recent-mtime backoff source-version scoped and avoid tight retry loops.
- Keep generator scope to local images and videos through ImageMagick and FFmpeg; do not add GTK or arbitrary `.thumbnailer` execution.
- Keep remote and metadata-limited entries conservative and separate thumbnail readiness from icon-metadata readiness.
- Compute physical decode targets as `ceil(logicalSize * effectiveDpr)` while retaining logical QML geometry.
- Propagate actual window/screen DPR through icon source identity, the provider, and `IconThemeService`.
- Do not set `cache: false` globally or remove `smooth`/`mipmap` mechanically.
- Use `apply_patch` for edits, `rtk` for shell/test commands, and no subagents.
- Add a regression test before each production behavior change and commit each independently testable task.

---

## File map

Create:

- `apps/explorer/backend/src/thumbnail_cache.rs` — XDG tier paths, URI/MD5 identity, freshness/failure validation, PNG metadata, atomic writes, and cache-focused tests.
- `apps/explorer/docs/PREVIEW_PIPELINE.md` — English preview lifecycle, cache, tier, DPR, generator, and limitation documentation.

Modify:

- `apps/explorer/backend/Cargo.toml` and `Cargo.lock` — add the small `png` encoder dependency used to write standard text chunks.
- `apps/explorer/backend/src/main.rs` — expose `thumbnail-batch` and remove the direct legacy warm command.
- `apps/explorer/backend/src/utility.rs` — route the exact-path utility operation and return per-file JSON.
- `apps/explorer/backend/src/thumbnails.rs` — batch parsing, supported media policy, bounded generator execution, GIO lookup, and result assembly.
- `apps/explorer/backend/src/entries.rs` — use only cheap direct-image or validated shared-cache preview URLs during listing.
- `apps/explorer/src/controllers/preview_controller.h/.cpp` — native scheduling, generation/source checks, request dispatch, selected priority, and model hydration.
- `apps/explorer/src/controllers/navigation_controller.h/.cpp` — forward navigation/model generation invalidations to `PreviewController`.
- `apps/explorer/src/controllers/app_state_facade.h/.cpp` — expose thin exact-range/selected-preview methods and DPR-aware icon methods; remove warm request ownership.
- `apps/explorer/src/services/filesystem_service.h/.cpp` — remove the directory-offset `warmThumbnails` method while retaining unrelated utility operations.
- `apps/explorer/src/explorer_application.cpp` — construct and wire the preview controller before exposing `AppState`.
- `apps/explorer/src/models/directory_model.h/.cpp` — preserve precise preview updates and add source-version comparison support if the controller needs it.
- `apps/explorer/src/runtime/astrea_icon_image_provider.cpp` — parse logical size/DPR identity, scale exact raster files physically, and preserve returned QImage DPR.
- `apps/explorer/src/services/icon_theme_service.h/.cpp` — add DPR to source identities and forward it to existing scale-aware rendering.
- `apps/explorer/qml/state/PreviewState.qml` and `apps/explorer/qml/AppState.qml` — remove QML thumbnail lifecycle state and expose thin exact-range/selected methods.
- `apps/explorer/qml/components/views/FileIconView.qml` and `FileListView.qml` — report latest ranges, request physical source sizes, and pass effective DPR to icons/emblems/drag previews.
- `apps/explorer/qml/components/layout/PreviewPanel.qml` — request selected local previews without a narrow extension regex and use physical preview/icon source sizes.
- `apps/explorer/tests/cpp/tst_preview_controller.cpp` — lifecycle, priority, stale-response, source-version, and immediate video hydration tests.
- `apps/explorer/tests/cpp/tst_icon_image_provider.cpp` and `tst_icon_theme_service.cpp` — deterministic DPR, physical-size, theme-scale, custom-raster/SVG, fallback, and identity tests.
- `apps/explorer/tests/cpp/tst_app_state_facade.cpp` and `tst_app_state_compatibility.cpp` — update the native surface for exact preview and DPR-aware icon calls.
- `apps/explorer/tests/qml/test_explorer_qml.py` — source-level checks for physical source sizes, logical geometry, viewport scheduling, video admission, emblem retention, and DPR identities.

## Task 1: Add deterministic Freedesktop cache primitives

**Files:**

- Create: `apps/explorer/backend/src/thumbnail_cache.rs`
- Modify: `apps/explorer/backend/src/main.rs`
- Modify: `apps/explorer/backend/Cargo.toml`
- Modify: `Cargo.lock`
- Test: inline `#[cfg(test)]` module in `apps/explorer/backend/src/thumbnail_cache.rs`

**Interfaces:**

- Produce `ThumbnailTier::{Normal, Large, XLarge, XXLarge, Fail}` with `directory_name()`, `max_pixels()`, and `tier_for_target(u32)`.
- Produce `SourceVersion { modified_secs: i64, size: u64 }` with a stable `identity()` string used by result validation and backoff.
- Produce `cache_root(xdg_cache_home: Option<&Path>, home: &Path) -> Result<PathBuf, String>` and `thumbnail_root(...)` without caching environment variables globally.
- Produce `canonical_uri(path: &Path) -> Result<String, String>` and `cache_filename(uri: &str) -> String` using lowercase MD5 hexadecimal plus `.png`.
- Produce `tier_path(root: &Path, tier: ThumbnailTier, uri: &str) -> PathBuf`.
- Produce `read_valid_thumbnail(...)`, `read_failure_entry(...)`, and `write_standard_thumbnail(...)` with source metadata validation.

- [ ] **Step 1: Write failing cache identity and tier tests.**

Add tests that use a unique temporary XDG directory and assert:

```rust
assert_eq!(tier_for_target(128), ThumbnailTier::Normal);
assert_eq!(tier_for_target(129), ThumbnailTier::Large);
assert_eq!(tier_for_target(256), ThumbnailTier::Large);
assert_eq!(tier_for_target(257), ThumbnailTier::XLarge);
assert_eq!(tier_for_target(512), ThumbnailTier::XLarge);
assert_eq!(tier_for_target(513), ThumbnailTier::XXLarge);
assert_eq!(tier_for_target(1024), ThumbnailTier::XXLarge);
assert_eq!(tier_for_target(1025), ThumbnailTier::XXLarge);
```

Also assert that `root/./photo # é.png` and its normalized absolute form produce
the same canonical URI, that the URI contains percent encoding for spaces and
`#`, and that `cache_filename(uri)` is 32 lowercase hex characters followed by
`.png`. Assert XDG override and `$HOME/.cache` fallback paths.

- [ ] **Step 2: Run the focused tests and verify they fail for the missing module.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnail_cache
```

Expected: compilation failure naming the missing `thumbnail_cache` module or
the unimplemented cache helpers.

- [ ] **Step 3: Add the module and implement the pure cache identity helpers.**

Register `mod thumbnail_cache;` in `main.rs`. Resolve relative paths to
absolute lexical paths, use `gio::File::for_path(...).uri()` for the canonical
local URI, and compute the filename with the already-present `md5` crate.
Reject non-absolute XDG cache overrides, clamp tier selection above 1024 to
`XXLarge`, and expose testable path functions that accept explicit roots.

- [ ] **Step 4: Add failing freshness, metadata, permissions, and atomic-write tests.**

Create a small fixture PNG with the `png` crate, then cover:

```rust
assert!(read_valid_thumbnail(&candidate, &uri, &version, ThumbnailTier::Large).is_ok());
assert!(read_valid_thumbnail(&stale_mtime, &uri, &version, ThumbnailTier::Large).is_err());
assert!(read_valid_thumbnail(&changed_size, &uri, &version, ThumbnailTier::Large).is_err());
assert!(read_failure_entry(&failure, &uri, &version).is_ok());
assert!(read_failure_entry(&failure, &uri, &changed_version).is_err());
```

Verify generated PNG text metadata includes `Thumb::URI`, `Thumb::MTime`,
`Thumb::Size`, `Thumb::Mimetype`, and `Software`; verify no final `.tmp` file
remains, the final file is readable, and Unix permissions are `0600` with
private cache directories.

- [ ] **Step 5: Run the new tests to capture the red state.**

Run the same `rtk cargo test ... thumbnail_cache` command. Expected: failures
for metadata parsing, freshness, failure identity, and atomic output.

- [ ] **Step 6: Implement metadata validation and standard PNG writing.**

Use `png::Decoder` to read text chunks and reject missing/mismatched required
metadata. Treat optional `Thumb::Size` and `Thumb::Mimetype` as constraints
when present. Use `png::Encoder::add_text_chunk` to re-encode generator output
with standard metadata into a temporary file created inside the destination
tier, set private permissions, then rename it over the final path. Keep the
write path independent of generator command execution.

- [ ] **Step 7: Run the focused cache tests and commit the cache layer.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnail_cache
rtk git diff --check
rtk git add apps/explorer/backend/src/main.rs apps/explorer/backend/src/thumbnail_cache.rs apps/explorer/backend/Cargo.toml Cargo.lock
rtk git commit -m feat-preview-cache-primitives
```

Expected: all cache tests pass and only the cache/module/dependency files are
in the commit.

## Task 2: Replace warm relisting with exact-path thumbnail batches

**Files:**

- Modify: `apps/explorer/backend/src/thumbnails.rs`
- Modify: `apps/explorer/backend/src/utility.rs`
- Modify: `apps/explorer/backend/src/main.rs`
- Modify: `apps/explorer/backend/src/entries.rs`
- Test: inline backend tests in `thumbnails.rs` plus existing entry tests

**Interfaces:**

- Consume `thumbnail_cache::{SourceVersion, ThumbnailTier}`.
- Produce `pub fn run_batch(args: &[String]) -> Result<String, String>` where
  `args[0]` is a physical target and `args[1..]` are exact paths.
- Produce `pub fn run_batch_with_generator(args: &[String], generator: &dyn ThumbnailGenerator) -> Result<String, String>` for deterministic tests.
- Produce `pub fn preview_url(path: &Path, is_dir: bool, modified_ms: i64, size: u64) -> String` for cheap listing-time lookup.
- Produce per-item JSON fields `filePath`, `status`, `previewUrl`, `cacheTier`, and `sourceVersion`.
- Keep `is_previewable(path: &Path) -> bool` as the single backend generator capability decision.

- [ ] **Step 1: Write failing request-shape tests.**

Add a test-only `CountingGenerator` implementing the planned generator seam:

```rust
trait ThumbnailGenerator: Sync {
    fn generate(&self, input: &Path, output: &Path, target: u32) -> Result<(), String>;
}
```

Test `run_batch_with_generator` with paths in a directory whose read access is
removed after the exact files are created; a correct batch can still stat the
known files and never calls `read_sorted_entries`. Add assertions that the
request preserves supplied path order, deduplicates repeated paths, and never
calls the counting generator for a valid cache hit.

- [ ] **Step 2: Run the backend tests and verify the old architecture fails.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnails
```

Expected: failure because no exact-path batch parser or generator seam exists
and the old implementation still depends on directory listing arguments.

- [ ] **Step 3: Implement bounded exact-path parsing and cache-first lookup.**

Parse one physical target followed by paths, reject a target of zero, reject
more than 32 deduplicated candidates, and reject encoded argument payloads over
262144 bytes. Reject remote URI forms, directories, missing files, and
metadata-limited inputs as item-level statuses. For each accepted local file,
capture `SourceVersion`, select the smallest satisfying tier, search valid
requested-or-larger standard tiers, then consult GIO with the public attributes
`thumbnail::path-*`, `thumbnail::is-valid-*`, and `thumbnail::failed-*` before
generation. Never call `entries::read_sorted_entries` from the batch path.

- [ ] **Step 4: Add failing tests for output statuses and bounded arguments.**

Assert JSON contains one item per unique path with `unsupported`, `failed`,
`deferred`, `ready`, or `generated` as applicable, and that one failed item
does not make the top-level batch fail. Assert over-limit requests return a
bounded-request error before any generator invocation. Assert a mock video
generator result contains the exact generated shared-cache URL and selected
tier.

- [ ] **Step 5: Implement generator execution and standard cache writes.**

Use one `OnceLock<rayon::ThreadPool>` with four or fewer threads for the whole
batch, not one pool per file. Implement `ProcessThumbnailGenerator` with
target-derived ImageMagick and FFmpeg arguments; use `target x target` with
aspect-ratio preservation, retaining SVG-specific density handling. Encode
generator output through `write_standard_thumbnail`, use a same-directory
temporary file, and record a standard failure entry or source-version failure
backoff when generation fails. Return `deferred` for files modified less than
three seconds ago. Do not spawn arbitrary `.thumbnailer` programs.

- [ ] **Step 6: Route the new operation and remove the legacy backend command.**

Route utility `thumbnail-batch` to `thumbnails::run_batch(&args[1..])`, print
its JSON unchanged, and change the direct CLI usage to list/search/devices and
`thumbnail-batch` without `warm-thumbnails`. Update `entries.rs` to pass file
size into `preview_url`, consult only validated shared tiers, and retain direct
local image fallback without generator or per-row GIO work. The listing code
must not create the old private cache directory.

- [ ] **Step 7: Run backend tests and commit the exact-path backend.**

Run:

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnails
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml entries
rtk git diff --check
rtk git add apps/explorer/backend/src/main.rs apps/explorer/backend/src/utility.rs apps/explorer/backend/src/thumbnails.rs apps/explorer/backend/src/entries.rs
rtk git commit -m feat-exact-thumbnail-batch
```

Expected: exact-path, cache-hit, bounds, status, tier, and entry-list tests
pass without invoking directory sorting from the batch implementation.

## Task 3: Make PreviewController own lifecycle and immediate model hydration

**Files:**

- Modify: `apps/explorer/src/controllers/preview_controller.h/.cpp`
- Modify: `apps/explorer/src/models/directory_model.h/.cpp` only if source-version helpers are needed
- Modify: `apps/explorer/tests/cpp/tst_preview_controller.cpp`
- Modify: `apps/explorer/src/backend/fake_backend_client.*` only for deterministic request/result inspection

**Interfaces:**

- Construct `PreviewController(IRustBackendClient *client, DirectoryModel *model, QObject *parent = nullptr)`.
- Expose `void beginGeneration(quint64 generation, bool remoteDirectoryActive)`.
- Expose `void setEnabled(bool enabled)`.
- Expose `void requestVisibleRange(int firstIndex, int lastIndex, int physicalTarget)`.
- Expose `void requestSelectedPreview(const QString &filePath, int physicalTarget)`.
- Retain `QUrl previewUrl(const DirectoryEntry &, bool)` as a cheap existing-preview interpretation that returns no extension-based decision.
- Retain `bool applyPreview(const QString &, const QUrl &, quint64)` as the guarded model update path.

- [ ] **Step 1: Extend tests with a model fixture and fake utility responses.**

Create ordered local image/video entries plus remote, metadata-limited, and
directory entries. Add tests that schedule ranges and inspect
`FakeRustBackendClient::utilityRequests()` after the debounce interval:

```cpp
controller.requestVisibleRange(1, 4, 192);
QTRY_COMPARE(client.utilityRequests().size(), 1);
QCOMPARE(client.utilityRequests().at(0).operation, QStringLiteral("thumbnail-batch"));
QCOMPARE(client.utilityRequests().at(0).arguments.at(0), QStringLiteral("192"));
```

Assert model order, exclusion, deduplication, and one in-flight request.

- [ ] **Step 2: Run the preview test target to record the red lifecycle state.**

Run:

```bash
rtk cmake --build build/debug --target tst_preview_controller -j2
build/debug/tst_preview_controller
```

Expected: compilation/API failures for the missing client injection and range
methods, followed by absent scheduling assertions.

- [ ] **Step 3: Implement the native controller state machine.**

Make the class a `QObject` with a 50 ms single-shot timer, ordered viewport
queue plus membership set, selected priority entries, in-flight request map,
per-path source-version state, and enabled flag. Build candidates from
`DirectoryModel::get`/`entryForPath` in model order, skip ineligible rows and
sufficient results, replace queued viewport work on every new range, and keep
the active batch untouched during scroll. Group requests by the first intent's
physical target so list-sized work does not force grid-sized tiers.

- [ ] **Step 4: Add failing stale-result and hydration tests.**

Complete a fake response containing one generated video and one failed image,
then assert the generated URL immediately reaches `FilePreviewUrlRole` without
`applyEntries` or a navigation call. Complete responses after
`beginGeneration` changes and after the source entry's size/mtime changes; in
both cases assert the old URL is not written. Assert disabled previews clear
queued work and ignore later responses.

- [ ] **Step 5: Implement result decoding, generation checks, and selected priority.**

Decode `UtilityResult::data.items`, match each response to its request record,
validate generation, current model path, current `fileSize`/`fileModified`
source identity, and non-empty local preview URL before calling
`DirectoryModel::updatePreview`. Track cache tier/source version after a ready
result so sufficiently good entries are not requeued. Record failed/deferred
source identities without starting a retry timer. Let an active bounded batch
finish on invalidation when hard cancellation could orphan an FFmpeg/Magick
child; remove its queued obsolete work and discard its stale results.

- [ ] **Step 6: Add selected-item priority tests and implementation.**

Schedule a viewport and then a selected path; assert the first newly dispatched
request is the selected path, the latest viewport remains queued, and the
selected path is not duplicated if it is already in flight. Assert selected
physical target is preserved. Add a fake completion for a selected video and
assert immediate model hydration.

- [ ] **Step 7: Run all preview-controller tests and commit the lifecycle owner.**

Run:

```bash
rtk cmake --build build/debug --target tst_preview_controller -j2
build/debug/tst_preview_controller
rtk git diff --check
rtk git add apps/explorer/src/controllers/preview_controller.h apps/explorer/src/controllers/preview_controller.cpp apps/explorer/src/models/directory_model.h apps/explorer/src/models/directory_model.cpp apps/explorer/src/backend/fake_backend_client.h apps/explorer/src/backend/fake_backend_client.cpp apps/explorer/tests/cpp/tst_preview_controller.cpp
rtk git commit -m feat-native-preview-lifecycle
```

Expected: range replacement, one in-flight batch, generation/source rejection,
priority, disabled state, item failures, and immediate video hydration pass.

## Task 4: Wire navigation, AppState, and utility ownership

**Files:**

- Modify: `apps/explorer/src/controllers/navigation_controller.h/.cpp`
- Modify: `apps/explorer/src/controllers/app_state_facade.h/.cpp`
- Modify: `apps/explorer/src/services/filesystem_service.h/.cpp`
- Modify: `apps/explorer/src/explorer_application.cpp`
- Modify: `apps/explorer/tests/cpp/tst_navigation_controller.cpp`
- Modify: `apps/explorer/tests/cpp/tst_app_state_facade.cpp`
- Modify: `apps/explorer/tests/cpp/tst_app_state_compatibility.cpp`

**Interfaces:**

- Add `PreviewController *preview` to `AppStateFacadeDependencies`.
- Add `NavigationController::setPreviewController(PreviewController *)` and call `beginGeneration` whenever navigation/search/refresh/model generation changes; call `setEnabled` when listing previews change.
- Add `AppStateFacade::requestVisibleThumbnailRange(int firstIndex, int lastIndex, int physicalTarget)` and `requestSelectedThumbnail(const QString &path, int physicalTarget)`.
- Remove `requestThumbnailWarm(QString,int,int)` and `FilesystemService::warmThumbnails(QString,int,int)` from the active native surface.
- Add a DPR argument to icon source methods while retaining source URL identity for revision/version.

- [ ] **Step 1: Add failing wiring tests.**

Assert `NavigationController` invalidates the preview generation on list,
search, refresh, tab switch, and remote transitions. Assert the facade forwards
exact indices and physical target to the controller. Update the invokable
surface test to require `requestVisibleThumbnailRange` and
`requestSelectedThumbnail`, and to reject the retired offset warm method.

- [ ] **Step 2: Run focused navigation/facade tests and verify red failures.**

Run:

```bash
rtk cmake --build build/debug --target tst_navigation_controller tst_app_state_facade -j2
build/debug/tst_navigation_controller
build/debug/tst_app_state_facade
```

Expected: missing preview dependency/setter and invokable-surface assertions.

- [ ] **Step 3: Implement generation forwarding and facade delegation.**

Forward the active `m_generation` and remote state from `NavigationController`
to `PreviewController` at every lifecycle boundary, clear obsolete work on
refresh/search/tab changes, and keep visual metadata scheduling unchanged.
Store the preview controller in the facade and make its two new methods direct
calls with no request-id bookkeeping or result parsing.

- [ ] **Step 4: Remove the old utility warm path.**

Delete the FilesystemService warm method and the facade's
`m_thumbnailWarmRequest`/count-only operation handling. Keep archive,
destructive-operation, network, properties, and other filesystem utility
behavior unchanged. Update compatibility fixtures to the new native surface.

- [ ] **Step 5: Wire object construction and run focused tests.**

Construct `PreviewController previewController(&backendClient, &directoryModel,
&application)` after the navigation dependencies exist, call
`navigation.setPreviewController(&previewController)`, and assign
`appStateDependencies.preview`. Reconfigure the existing target in
`build/debug`, then run:

```bash
rtk cmake --build build/debug --target tst_navigation_controller tst_app_state_facade tst_app_state_compatibility astrea-explorer -j2
build/debug/tst_navigation_controller
build/debug/tst_app_state_facade
build/debug/tst_app_state_compatibility
```

- [ ] **Step 6: Commit the ownership wiring.**

```bash
rtk git diff --check
rtk git add apps/explorer/src/controllers/navigation_controller.h apps/explorer/src/controllers/navigation_controller.cpp apps/explorer/src/controllers/app_state_facade.h apps/explorer/src/controllers/app_state_facade.cpp apps/explorer/src/services/filesystem_service.h apps/explorer/src/services/filesystem_service.cpp apps/explorer/src/explorer_application.cpp apps/explorer/tests/cpp/tst_navigation_controller.cpp apps/explorer/tests/cpp/tst_app_state_facade.cpp apps/explorer/tests/cpp/tst_app_state_compatibility.cpp
rtk git commit -m refactor-preview-ownership-wiring
```

## Task 5: Complete real-DPR icon provider and service propagation

**Files:**

- Modify: `apps/explorer/src/runtime/astrea_icon_image_provider.cpp`
- Modify: `apps/explorer/src/services/icon_theme_service.h/.cpp`
- Modify: `apps/explorer/src/controllers/app_state_facade.h/.cpp`
- Modify: `apps/explorer/tests/cpp/tst_icon_image_provider.cpp`
- Modify: `apps/explorer/tests/cpp/tst_icon_theme_service.cpp`

**Interfaces:**

- Source creators accept logical size plus `qreal devicePixelRatio` and append bounded `dpr` to their URL identity.
- Provider parses `size`, `dpr`, `revision`, `version`, and `fallback`; it uses the QML `requestedSize` as physical decode size when available and query `size` as logical render size.
- Theme paths call `renderIcon(candidates, logicalSize, boundedDpr)` and return its DPR-bearing QImage.

- [ ] **Step 1: Add failing provider/service tests with a temporary scale-aware theme.**

Extend existing temporary icon-theme helpers with visibly different Scale=1
and Scale=2 assets. Add assertions:

```cpp
const QImage one = provider.requestImage(QStringLiteral("theme/folder?size=32&dpr=1"), &size, QSize(32, 32));
const QImage two = provider.requestImage(QStringLiteral("theme/folder?size=32&dpr=2"), &size, QSize(64, 64));
QCOMPARE(two.devicePixelRatio(), 2.0);
QCOMPARE(two.size(), QSize(64, 64));
QVERIFY(two.pixelColor(32, 32) != one.pixelColor(16, 16));
```

Add custom-raster tests for physical scaling, native-size preservation for a
tiny source, aspect ratio, and DPR metadata; add SVG and themed-fallback tests.
Assert the service URLs differ for DPR 1 and 2 and that rendered cache entries
do not collide.

- [ ] **Step 2: Run focused icon tests to capture the hardcoded-DPR failures.**

Run:

```bash
rtk cmake --build build/debug --target tst_icon_image_provider tst_icon_theme_service -j2
build/debug/tst_icon_image_provider
build/debug/tst_icon_theme_service
```

Expected: DPR-2 theme selection/size/identity assertions fail because the
provider currently supplies `1.0` and ignores physical requested size.

- [ ] **Step 3: Add DPR to service source identities and facade calls.**

Bound DPR to `0.5..4.0`, append a stable decimal `dpr` query item to theme,
symbolic, emblem, file, and rich-file source URLs, and include it in the
render/cache key. Pass the QML argument through `AppStateFacade` to
`IconThemeService`; leave the existing Freedesktop catalog selection intact so
its Scale=2 asset resolution receives the real value.

- [ ] **Step 4: Implement provider physical decode and DPR preservation.**

Use query `size` for logical theme rendering and `requestedSize`/`ceil(size*dpr)`
for physical custom-raster decode. Keep aspect ratio, set the returned QImage
DPR, retain SVG vector behavior, and use the parsed DPR for themed fallback
rendering. Do not upscale a raster smaller than its useful native size merely
to satisfy a large request.

- [ ] **Step 5: Run focused native icon tests and commit.**

```bash
rtk cmake --build build/debug --target tst_icon_image_provider tst_icon_theme_service -j2
build/debug/tst_icon_image_provider
build/debug/tst_icon_theme_service
rtk git diff --check
rtk git add apps/explorer/src/runtime/astrea_icon_image_provider.cpp apps/explorer/src/services/icon_theme_service.h apps/explorer/src/services/icon_theme_service.cpp apps/explorer/src/controllers/app_state_facade.h apps/explorer/src/controllers/app_state_facade.cpp apps/explorer/tests/cpp/tst_icon_image_provider.cpp apps/explorer/tests/cpp/tst_icon_theme_service.cpp
rtk git commit -m feat-end-to-end-icon-dpr
```

## Task 6: Move QML to native scheduling and physical source sizes

**Files:**

- Modify: `apps/explorer/qml/state/PreviewState.qml`
- Modify: `apps/explorer/qml/AppState.qml`
- Modify: `apps/explorer/qml/components/views/FileIconView.qml`
- Modify: `apps/explorer/qml/components/views/FileListView.qml`
- Modify: `apps/explorer/qml/components/layout/PreviewPanel.qml`
- Modify: `apps/explorer/qml/components/layout/Sidebar.qml` if its source calls need the same DPR identity
- Test: `apps/explorer/tests/qml/test_explorer_qml.py`

**Interfaces:**

- Define per-window `effectiveDpr` as `Window.window ? Window.window.devicePixelRatio : Screen.devicePixelRatio`, clamped to `0.5..4.0`.
- Define `physicalDecodeSize(logical) = Math.max(1, Math.ceil(logical * effectiveDpr))` in each visual surface that owns geometry.
- Call `AppState.requestVisibleThumbnailRange(first, last, physicalDecodeSize(previewLogicalSize))`.
- Call `AppState.requestSelectedThumbnail(selectedPath, physicalDecodeSize(320))`.
- Pass logical icon size and `effectiveDpr` to `richFileIconSource`, `fileIconSource`, `emblemIconSource`, and sidebar/themed source wrappers.

- [ ] **Step 1: Add failing QML source assertions.**

Extend the Python tests to require `QtQuick.Window`, `devicePixelRatio`,
`Math.ceil`, physical `sourceSize`, and logical `width`/`height` bindings in
grid, list, emblem, preview-panel, and drag images. Require exact-range and
selected-thumbnail calls with physical targets. Assert no
`requestThumbnailWarm`, offset/limit warm wrapper, or narrow image-only
`isPreviewableFile` regex remains in preview paths.

- [ ] **Step 2: Run the QML tests to verify the missing physical/DPR behavior.**

Run:

```bash
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
```

Expected: the new source checks fail against the old warm-up fields, regex,
logical-only source sizes, and missing DPR arguments.

- [ ] **Step 3: Remove QML lifecycle state and startup warming.**

Delete `pendingThumbnailWarmRequest`, `activeThumbnailWarmRequest`, startup
warm queues, offset-based warm functions, and the delayed startup call that
warms a guessed directory. Keep formatting, zoom, and independent visual
metadata forwarding in `PreviewState`; keep `AppState` wrappers thin.

- [ ] **Step 4: Update grid/list viewport reporting and source sizes.**

Keep the existing visible-range discovery and metadata scheduling, but replace
the thumbnail call with the native exact-range method and a physical target.
Set icon, preview, emblem, and drag `sourceSize`/`Drag.imageSourceSize` to
physical decode values while leaving visual frame `width` and `height` in DIPs.
Keep preview images asynchronous and theme icons synchronous; retain
`cache:true`, `smooth`, `mipmap`, and `retainWhileLoading` where current
behavior benefits from them.

- [ ] **Step 5: Update PreviewPanel admission and selected scheduling.**

Replace `AppState.isPreviewableFile` with a local, non-directory,
non-remote/non-metadata-limited request condition. Let the backend return
`unsupported`; do not add an extension list in QML. Use only
`filePreviewUrl` as the ready preview source, request a selected path once per
selection/visibility change, and use physical 320-DIP preview and 64-DIP icon
source sizes. Keep the icon fallback visible while an async preview loads and
retain emblems over a ready preview in the file views.

- [ ] **Step 6: Run QML tests and commit the presentation boundary.**

```bash
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
rtk git diff --check
rtk git add apps/explorer/qml/state/PreviewState.qml apps/explorer/qml/AppState.qml apps/explorer/qml/components/views/FileIconView.qml apps/explorer/qml/components/views/FileListView.qml apps/explorer/qml/components/layout/PreviewPanel.qml apps/explorer/qml/components/layout/Sidebar.qml apps/explorer/tests/qml/test_explorer_qml.py
rtk git commit -m feat-qml-physical-preview-sizing
```

## Task 7: Add interoperability, failure, and documentation coverage

**Files:**

- Modify: `apps/explorer/backend/src/thumbnail_cache.rs` and `thumbnails.rs` tests if any interoperability gaps remain
- Create: `apps/explorer/docs/PREVIEW_PIPELINE.md`
- Modify: `apps/explorer/docs/ICON_THEME_INTEGRATION.md`
- Modify: `apps/explorer/tests/cpp/tst_directory_model.cpp`
- Modify: `apps/explorer/tests/cpp/tst_navigation_controller.cpp`
- Modify: `apps/explorer/tests/qml/test_explorer_qml.py`

- [ ] **Step 1: Add GIO recognition and failure/backoff tests before documentation.**

Generate a standard thumbnail into the temporary XDG cache, query the source
through public GIO file attributes, and assert the valid path/valid flag are
recognized. Add tests for standard failure suppression, source mtime/size
eligibility, recent-file `deferred`, and one-item failure in a successful batch.
Add a large-directory fixture proving the batch test only sees the supplied
bounded paths.

- [ ] **Step 2: Run the targeted backend and native regression tests.**

```bash
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnail_cache
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml thumbnails
rtk cmake --build build/debug --target tst_directory_model tst_navigation_controller tst_preview_controller -j2
build/debug/tst_directory_model
build/debug/tst_navigation_controller
build/debug/tst_preview_controller
```

Expected: all cache, exact-path, lifecycle, model, and navigation tests pass.

- [ ] **Step 3: Write focused English documentation.**

Document the initial-list/preview-enrichment boundary, exact-path request
contract, latest viewport and selected priority, one in-flight bound, model
hydration, XDG tiers and canonical URI identity, metadata freshness, failure
cool-down, three-second recent-file deferral, local-only generator scope,
physical target/DPR propagation, remote exclusions, private-cache retirement,
and the limitation that arbitrary document `.thumbnailer` plugins are not
executed.

- [ ] **Step 4: Update icon documentation only for changed DPR ownership.**

Remove the old statement that provider requests intentionally use DPR 1.0 and
state that QML supplies window-associated DPR in the icon source identity while
the provider passes it to the existing service/catalog renderer.

- [ ] **Step 5: Commit the interoperability and documentation closure.**

```bash
rtk git diff --check
rtk git add apps/explorer/backend/src/thumbnail_cache.rs apps/explorer/backend/src/thumbnails.rs apps/explorer/docs/PREVIEW_PIPELINE.md apps/explorer/docs/ICON_THEME_INTEGRATION.md apps/explorer/tests/cpp/tst_directory_model.cpp apps/explorer/tests/cpp/tst_navigation_controller.cpp apps/explorer/tests/qml/test_explorer_qml.py
rtk git commit -m docs-preview-pipeline-interoperability
```

## Task 8: Final verification and review

**Files:**

- Inspect all files changed by Tasks 1–7; modify only if verification exposes a regression.

- [ ] **Step 1: Reconfigure and build in the existing directory.**

Run:

```bash
rtk cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
rtk cmake --build build/debug -j2
```

Do not pass a new build path and do not create a second Cargo target
directory. Confirm the packaged backend still locates its existing Magick and
FFmpeg runtime programs.

- [ ] **Step 2: Run the focused and broader configured tests.**

Run:

```bash
rtk ctest --test-dir build/debug --output-on-failure
rtk cargo test --manifest-path apps/explorer/backend/Cargo.toml
rtk pytest -q apps/explorer/tests/qml/test_explorer_qml.py
```

The final source must be the source under test; do not report results from an
earlier build.

- [ ] **Step 3: Run source/dependency gates.**

Run:

```bash
rtk run python3 scripts/verify_orbit_source_gate.py
rtk run git diff --check
rtk rg -n GTK GDK gtk gdk apps/explorer/backend/Cargo.toml apps/explorer/CMakeLists.txt apps/explorer/src
rtk rg -n 'warm-thumbnails|\.cache/explorer/thumbnails|cache_key|scale=256:256|512x512>|renderIcon\([^\n]*1\.0' apps/explorer/backend apps/explorer/src apps/explorer/qml
```

The GTK/GDK search must show no new dependency, and the legacy warm/private
cache/fixed-size/provider-DPR patterns must be absent from the active pipeline.
An old private cache directory may remain on disk but must not be a new write
destination.

- [ ] **Step 4: Inspect the final diff against the review checklist.**

Confirm exact-path batching never calls directory sorting, newly generated
video results call `updatePreview`, stale generations/source versions cannot
write, duplicate paths collapse, disabled previews stop scheduling, standard
cache metadata and atomic permissions are present, failures/backoff are
bounded, cache tiers follow physical target boundaries, provider/QML DPR
identities change across displays, logical geometry is unchanged, emblems and
delegate reuse remain correct, remote files stay excluded, and no unrelated UI
refactor is present.

- [ ] **Step 5: Commit any verification-only correction and report exact results.**

If a correction is required, add its regression test first, rerun the narrowest
affected test, then commit it with:

```bash
rtk git add -u apps/explorer Cargo.lock
rtk git commit -m fix-preview-pipeline-verification
```

Finish with `rtk git status --short` showing a clean worktree and report the
root causes, final architecture, request lifecycle, XDG behavior, tier/DPR
policy, changed files, tests and commands executed, plus the unsupported
arbitrary-document-thumbnailer limitation.
