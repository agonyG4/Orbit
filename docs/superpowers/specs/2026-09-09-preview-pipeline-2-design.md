# Preview Pipeline 2.0 Design

## Goal

Make Explorer's local image and video previews responsive, bounded, generation
safe, and interoperable with the Freedesktop thumbnail cache while preserving
the fast initial directory listing. Complete the existing icon-theme work by
propagating the actual display DPR through QML, the image provider, and the
theme renderer.

## Alternatives considered

1. Keep `warm-thumbnails` as a QML/facade helper and add DPR arguments. This
   would leave directory relisting, count-only completion, stale viewport
   accumulation, and split lifecycle ownership in place.
2. Add a second `ThumbnailController` beside `PreviewController`. This gives
   thumbnails an isolated owner, but duplicates generation and model-update
   concepts that already belong to the preview controller and increases wiring
   surface.
3. Extend `PreviewController` into the native preview lifecycle owner. It can
   own generation, viewport and selected-item intent, bounded exact-path
   requests, result validation, and immediate model updates while QML remains a
   view of state. This is the selected approach.

## Ownership and interfaces

`PreviewController` becomes a `QObject`-backed native lifecycle owner. It will
receive an `IRustBackendClient` and `DirectoryModel`, retain the active model
generation and local/remote state, and expose native slots or invokables for:

- a latest visible range plus its physical thumbnail target;
- a selected-item preview request plus its physical target;
- preview enable/disable state;
- generation/navigation invalidation.

It will connect to `IRustBackendClient::utilityReady` and consume only
`thumbnail-batch` responses that it issued. `AppStateFacade` will keep thin
forwarding methods for QML compatibility and will no longer track a warm
request id or interpret thumbnail completion. `NavigationController` will
notify the preview controller when a list/search/navigation/refresh generation
changes, but will not own thumbnail policy. `DirectoryModel::updatePreview`
remains the single model update pathway and will be extended only as needed to
validate source-version identity.

The old directory-offset `warm-thumbnails` path will be removed from the
Explorer preview flow. `FilesystemService` will not be the lifecycle owner; if
its generic utility surface remains useful, the controller will use that
existing backend abstraction directly.

## Exact-path request contract

The backend utility operation is `thumbnail-batch`. Its arguments contain a
bounded physical target and exact local paths, never a directory plus offset:

```text
thumbnail-batch <physical-target> <absolute-local-path> ...
```

The client will enforce a maximum of 32 candidates and a total encoded
argument-byte limit before dispatch. The backend will apply the same bounds,
deduplicate paths, stat each exact path, and never call directory enumeration
or sorting to rediscover UI rows.

The response is successful at the batch level when it was decoded, even if
individual files fail. Each item contains:

```json
{
  "filePath": "/absolute/path",
  "status": "ready|generated|direct|unsupported|failed|deferred",
  "previewUrl": "file:///...",
  "cacheTier": "normal|large|x-large|xx-large|fail",
  "sourceVersion": "mtime:size"
}
```

`previewUrl` may be empty for non-ready statuses. Item errors are data, not a
batch transport failure. The controller accepts an item only when its request
generation is current, the path still exists in the active model, and the
source version captured for the request still matches the current entry.

## Request lifecycle

For each visible-range update, the controller builds candidates in current
model order. It excludes directories, remote entries, metadata-limited entries,
unsupported/non-local paths, paths with a sufficient preview, and paths already
in flight. It replaces queued viewport work with the newest desired window,
deduplicates with ordered membership, and debounces rapid scroll changes.

Only one bounded thumbnail batch is normally active. A scroll change does not
cancel the active batch; it replaces the queued viewport and the finished
batch's results are either applied or rejected by generation/source identity.
Navigation, search, refresh, tab changes, and disabled previews invalidate the
old generation and clear queued work. Active generation work may finish
cooperatively when hard process cancellation would risk orphaning Magick or
FFmpeg children; stale results are ignored and no follow-up work is scheduled
for the obsolete generation.

Selected-item work is an ordered high-priority intent. It is deduplicated
against in-flight and queued paths, runs before ordinary prefetch when
possible, and is merged without erasing the latest viewport queue. It uses the
larger PreviewPanel physical target and the same generation, source validation,
batch bound, and result application rules.

Every ready/generated result is applied immediately through
`DirectoryModel::updatePreview`, so a newly generated video thumbnail becomes
visible without a folder relist. Failed and deferred items are recorded in the
controller's bounded source-version backoff state and do not create an
80-millisecond retry loop. A source mtime or size change clears that state.

## Initial listing boundary

The normal listing path remains a fast `std::fs`-based operation. It does not
spawn ImageMagick or FFmpeg, perform per-row thumbnail generation, or require
GIO thumbnail generation. A cheap direct local image URL or an inexpensive
already-available standard cache URL may be exposed as a provisional preview;
videos remain on their icon until a preview batch returns. Remote and
metadata-limited entries retain their conservative behavior.

Previewability policy is native and centralized in the backend's supported
generator/capability decision. C++ and QML will not maintain independent
extension regexes. QML will request a selected local non-directory preview and
render the returned `unsupported` item state when the backend cannot generate
one.

## Freedesktop/XDG cache

New thumbnails use `$XDG_CACHE_HOME/thumbnails`, falling back to the XDG cache
base directory rules, with these standard subdirectories:

- `normal`: target up to 128 px;
- `large`: target up to 256 px;
- `x-large`: target up to 512 px;
- `xx-large`: target up to 1024 px;
- `fail`: standard failure entries.

The selected tier is the smallest tier satisfying the physical target, with
requests above 1024 clamped to `xx-large`. A valid larger tier may satisfy a
smaller request and will be reused. The old
`~/.cache/explorer/thumbnails` directory is neither deleted nor migrated and is
never written as the canonical destination by the new pipeline.

The cache key is the MD5 of the canonical absolute local URI, rendered as
lowercase hexadecimal with `.png`. Canonicalization must preserve standard URI
escaping for spaces, `#`, non-ASCII names, and equivalent local path forms.

Before generation, the backend checks standard thumbnail state and validates
the actual PNG metadata. A cache hit is accepted only when `Thumb::URI` and
`Thumb::MTime` match the current source; `Thumb::Size` and `Thumb::Mimetype`
are checked when present. Public GIO size-specific thumbnail attributes are
used where available; private GVfs metadata is not parsed.

Explorer-generated PNGs carry at least `Thumb::URI` and `Thumb::MTime`, plus
`Thumb::Size`, `Thumb::Mimetype`, and `Software` when known. Writes use a
temporary file in the destination tier followed by an atomic same-directory
rename. Cache directories and files receive private user permissions. Failure
entries use standard metadata and are source-version scoped; valid failures
suppress repeated generation until the source changes. Files modified within
the short three-second cool-off are returned as `deferred`, not permanent
failures, and become eligible only through later bounded scheduling.

## Generator scope and concurrency

The generator supports the existing local image and video paths through
ImageMagick and FFmpeg. It does not add GTK or a general Freedesktop
`.thumbnailer` execution engine, and it does not broaden this work to PDF,
office, or arbitrary document thumbnailers. Generation uses one conservative,
bounded worker pool rather than constructing a Rayon pool per file. The
persistent backend worker remains serialized at the request level, while each
batch has a fixed candidate and process bound. Active thumbnail batches are
invalidated safely when stale; process cancellation is only hardened where it
cannot orphan content-generator children.

## Physical size and DPR

QML passes logical display requirements and the effective DPR associated with
the Explorer window. Physical decode targets are computed as
`ceil(logicalSize * dpr)` with the existing safe DPR/raster bounds. Thumbnail
visual width and height remain logical QML dimensions; only `sourceSize` and
the backend cache tier use physical pixels. Grid, list, PreviewPanel, emblem,
base-icon, and drag-preview images are audited consistently. QML cache identity
includes the preview source/version and DPR wherever the raster output can
change.

`AstreaIconImageProvider` parses logical size and DPR from the icon source
identity, renders themes through `IconThemeService::renderIcon(logicalSize,
dpr)`, and returns a QImage with correct DPR metadata. `IconThemeService` and
the Freedesktop catalog retain their existing scale-aware selection and include
DPR in source/cache identity. Exact raster custom icons scale toward the
physical requested size while preserving aspect ratio and avoiding needless
upscaling of tiny sources; SVG/vector inputs remain scalable. Fallback icons
retain the requested DPR. The effective scale comes from the window/screen
association, so moving the window between displays invalidates the relevant
QML/provider identities without a restart.

## Verification strategy

Tests will be added before each production behavior change. Rust unit tests
cover canonical URI/MD5, XDG locations, tier boundaries, valid/stale/size- and
mtime-invalidated standard entries, PNG metadata and GIO recognition, failure
cool-down, recent-file deferral, unsupported items, per-item failure, exact
paths, deduplication, argument bounds, cache hits, and requested-tier output.

Native Qt tests cover ordered latest-window scheduling, one in-flight batch,
generation and source-version rejection, remote/meta-limited exclusion,
disabled previews, immediate model hydration including video, failure
backoff, selected-item priority, and the icon provider's scale-aware themed,
raster, SVG, fallback, DPR, and cache-identity behavior. QML source tests
cover physical `sourceSize` for grid/list/icons/emblems/PreviewPanel, logical
geometry preservation, scale-change identities, retained emblems, delegate
reuse, and removal of the narrow selected-video regex.

Focused documentation will describe this boundary and its limitations in a
dedicated preview-pipeline document. The icon integration document will be
updated only for the changed DPR ownership. Verification reuses Orbit's
existing configured build directory, checks the focused and broader Explorer
tests, source gates, dependency constraints, and `git diff --check`.

## Constraints

- Work only in `/home/agony/GitHub/Orbit`; preserve unrelated Typhon changes.
- Reuse the existing Orbit build directory; do not create a worktree or build
  tree.
- Keep initial listing fast and remote behavior conservative.
- Keep thumbnail lifecycle native and separate from icon-metadata readiness.
- Do not add GTK/GDK, private Qt APIs, arbitrary thumbnailer plugins, or
  unrelated UI refactors.
