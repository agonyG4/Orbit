# Preview Listing Boundary Design

## Goal

Finish the Preview Pipeline 2.0 boundary by keeping shared-thumbnail cache
discovery and full PNG validation out of initial directory and recursive-search
enumeration while preserving the exact-path asynchronous `thumbnail-batch`
pipeline and all fixes from commit `8b19959`.

## Chosen policy

The listing preview contract has two explicit modes:

- `None`: return no preview identity.
- `Direct`: return only a source-versioned direct URL for local image/SVG
  entries. This decision uses the existing metadata/extension capability check;
  it does not consult the XDG cache, call GIO, open the source for a probe, or
  launch a generator. Videos and generator-dependent media return no initial
  preview URL.

There is no cache-validating listing mode. The existing `Cached` and `Full`
  modes are removed because ordinary listing/search has no legitimate caller
  for either policy. `Direct` is the default for internal Rust listing helpers,
  while C++ list and search requests always pass an explicit mode.

## Request and data flow

`ListRequest.previews` remains the source of truth for normal enumeration.
`SearchRequest` gains the same field. `NavigationController::startSearch()`
copies `m_previews`, and `RustBackendClient` emits an explicit `none` or
`direct` preview-mode argument for both operations.

Rust parses the mode deliberately for both commands and passes it through every
recursive search call. Enumeration constructs metadata and direct identities
only. After the model is populated, the existing `PreviewController` submits
exact visible or selected paths to `thumbnail-batch`; that path performs source
readability checks, XDG/GIO lookup and full PNG validation, then generation when
needed and immediate `DirectoryModel` hydration.

## Cache boundary and validation

`cached_preview_url()` is removed from the listing-facing code because no
ordinary initial-list/search caller should discover shared thumbnails. The
remaining cache callers are limited to asynchronous `thumbnail-batch` processing
and cache/thumbnails tests. `read_valid_thumbnail()` continues to decode a
complete PNG frame with `next_frame()`. `read_failure_entry()`, GIO snapshots,
standard writes, failure writes, source readability checks, cache containment,
permissions, and generation-aware lifecycle behavior remain unchanged.

## Regression coverage

Rust entry tests cover explicit mode parsing, preview-disabled listing/search,
direct image-only listing behavior, video omission, corrupt/valid cached files
not affecting enumeration, zero full-PNG validation during enumeration, and
recursive propagation of both modes. C++ tests cover `SearchRequest.previews`,
explicit list/search arguments, and navigation forwarding. The concurrent
failure cleanup assertion inspects the versioned `orbit-explorer` application
fixture directory while preserving its existing four-worker barrier.

No QML, thumbnail generation, cache-format, or PreviewController redesign is
part of this change.
