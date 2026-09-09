# Explorer Preview Pipeline

Explorer has two related but independent visual-metadata paths:

1. Native icon resolution produces synchronous theme and file-icon URLs for
   QML.
2. Native thumbnail scheduling produces asynchronous previews for visible
   local files and the selected item.

The QML layer owns presentation geometry, selection, drag behavior, and the
debounce timers around visibility changes. It does not own thumbnail
generation, directory-offset warming, extension allowlists, cache paths, or
retry policy.

## Lifecycle

NavigationController advances a preview generation whenever the active
directory, search result, tab, remote-directory state, or refreshed model
changes. It forwards that generation and the remote-directory flag to
PreviewController. The controller clears queued viewport and selected-item
intents at the boundary and rejects results from older generations.

The grid and list views discover their visible source-index range. After an
80 ms quiet period they send the exact range and a physical decode target
through:

    AppState.requestVisibleThumbnailRange(first, last, physicalTarget)
    AppState.scheduleVisibleFileVisualMetadata(first, last)

The two calls intentionally remain separate. Thumbnail generation is
filesystem/process work; visual metadata hydration is a bounded GIO request
that supplies icon names, custom icon files, and emblems. A thumbnail result
must not wait for icon metadata, and metadata hydration must not start
thumbnail generation.

The selected-item panel sends a path, rather than a model offset:

    AppState.requestSelectedThumbnail(selectedPath, physicalDecodeSize(320))

Selected work has priority over ordinary viewport prefetch and uses the same
generation and source-version validation. The panel shows filePreviewUrl only
after native work returns it; before then, the regular file icon remains
visible. Direct arbitrary local URLs are not treated as a ready selected
preview, so unsupported video and document types remain on their icon.

## Controller admission and queueing

PreviewController is the only native scheduler. It admits local,
non-directory, non-remote, non-metadata-limited entries. Entries with a
sufficient preview, a matching request already in flight, or a source-version
backoff are skipped. The visible queue is replaced by the newest viewport
range, preserving model order and removing duplicates. A selected intent is
merged independently and dispatched first.

Normally only one thumbnail-batch request is in flight. A scroll update does
not terminate a running ImageMagick or FFmpeg process: the next result is
checked against its generation and the current model entry, and stale work is
discarded. This avoids orphaning child processes while ensuring an obsolete
directory cannot receive a preview.

Each batch is bounded to 32 unique paths and the encoded argument payload is
bounded to 256 KiB. The Rust worker captures the source mtime and size before
generation. A result is applied immediately through
DirectoryModel::updatePreview, so a newly generated video thumbnail appears
without relisting the directory.

ready, generated, and cached results are accepted when their local URL and
source version still match. deferred results represent a file modified
within the three-second cool-off window. failed results are retained in
bounded source-version state and do not create a timer-driven retry loop. A
changed mtime or size gives the source a new identity and makes it eligible
again.

## Freedesktop thumbnail cache

The canonical cache is the shared XDG thumbnail tree:

    $XDG_CACHE_HOME/thumbnails/
      normal/       up to 128 px
      large/        up to 256 px
      x-large/      up to 512 px
      xx-large/     up to 1024 px
      fail/         source-version-scoped failure entries

When XDG_CACHE_HOME is not set, the cache falls back to
$HOME/.cache/thumbnails. Explorer does not write the retired
~/.cache/explorer/thumbnails tree and does not delete or migrate existing
files there.

The filename is the lowercase MD5 of the canonical local file:// URI with
.png appended. Canonicalization occurs before hashing so equivalent local
path spellings share one cache identity while spaces, #, and non-ASCII
characters retain standard URI escaping.

The smallest tier satisfying the physical target is selected. A valid larger
tier can satisfy a smaller request and is reused. Every accepted PNG must
contain matching Thumb::URI and Thumb::MTime; Thumb::Size and
Thumb::Mimetype are checked when present. Explorer-generated files also
carry Software.

Writes create the destination tier with private permissions, encode metadata
into a temporary file in that same directory, and atomically rename the
temporary file into place. Failure entries use the same metadata path and
therefore become invalid automatically when the source mtime or size changes.

Before generation, the backend checks both Explorer's standard cache
locations and the public GIO thumbnail attributes:

    thumbnail::path-normal    thumbnail::is-valid-normal
    thumbnail::path-large     thumbnail::is-valid-large
    thumbnail::path-xlarge    thumbnail::is-valid-xlarge
    thumbnail::path-xxlarge   thumbnail::is-valid-xxlarge
    thumbnail::failed-*

Private GVfs metadata is not parsed. The ordinary directory listing remains
cheap and does not spawn a generator or perform per-row GIO thumbnail
generation.

## Generator and failure policy

The supported native generator paths are the existing local image and video
capabilities:

- ImageMagick handles raster images and SVG, with aspect-ratio preservation.
- FFmpeg extracts a frame from local video files and scales it into the
  selected square target.

Explorer does not execute arbitrary .thumbnailer programs and does not
broaden this pipeline to PDF, office, or general document thumbnailers.
Generation uses one process-wide Rayon pool with at most four workers. A
single item failure is encoded in that item's result; it does not fail the
whole batch.

Recent files return deferred before a generator starts. A failed generation
records a standard fail entry for the current source version. The
controller's in-memory failure state and the shared failure entry both
suppress repeated work until the source changes. This is important for
unsupported or temporarily broken media: scrolling over the same item does
not create an unbounded retry loop.

## Physical pixels and DPR

QML geometry remains in device-independent pixels. Each visual surface
computes:

    effectiveDpr = clamp(Window.window.devicePixelRatio, 0.5, 4.0)
    physicalSize = max(1, ceil(logicalSize * effectiveDpr))

When no window is attached, Screen.devicePixelRatio is used. Grid, list,
preview-panel, sidebar, icon, emblem, and drag images use physical source
sizes. Their visible width and height remain logical so the UI does not grow
when a high-DPI screen requests a sharper asset.

Icon source URLs carry the logical size and DPR. The service uses DPR to
choose Freedesktop Scale=1 or Scale=2 assets and includes it in rendered
cache identity. The provider uses QML's physical requested size for raster
decode and sets the returned image DPR. This prevents a 2x icon from
colliding with its 1x counterpart while retaining the theme's logical
appearance.

## Remote and metadata-limited entries

The pipeline is local-only. Remote-directory entries, remote paths, and
metadata-limited entries are not sent to the local thumbnail worker. They
retain their ordinary icon behavior. The same rule is applied natively by
the controller and backend; QML only performs the lightweight admission
check needed to avoid requesting an obviously ineligible selected item.

## Debugging checklist

When a preview does not appear, inspect the following in order:

1. Confirm the selected model generation and the entry's current mtime/size.
2. Confirm the path is local, regular, and not metadata-limited.
3. Inspect the thumbnail-batch item status and sourceVersion.
4. Check the requested physical target and selected Freedesktop tier.
5. Validate Thumb::URI, Thumb::MTime, and optional size/MIME metadata in
   the candidate PNG.
6. Confirm the result generation still matches the current navigation model.

The expected behavior for a rejected item is visible in its status:
unsupported means the backend has no supported generator capability,
deferred means the file is still changing, and failed means generation
failed or a source-version-scoped failure entry was found.
