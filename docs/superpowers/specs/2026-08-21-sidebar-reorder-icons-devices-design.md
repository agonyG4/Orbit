# Astrea Explorer Sidebar Reorder, Symbolic Icons, and Device Rendering

Date: 2026-08-21

Status: Approved design; implementation plan pending written-spec review.

## Scope

Close three related sidebar defects in the Qt 6/C++ Astrea Explorer migration:

1. Make favorite reordering a real ordered interaction with visible displacement,
   cancellation, persistence, and safe separation from filesystem drag-and-drop.
2. Make sidebar and portal semantic icons render through an explicit symbolic
   image path while preserving full-color artwork for file-content icons.
3. Make enumerated devices render their native title and device icon correctly.

The existing Explorer layout, persisted favorite JSON shape, Rust device
enumeration, and unrelated working-tree changes remain in scope only as
compatibility constraints. The Rust backend will not change unless validation
finds a separate backend defect.

## Evidence from the current checkout

- `Sidebar.qml` currently renders favorites with a `Repeater` over a
  `QVariantList` and places a `DragHandler` directly on the layout-owned
  `SidebarItem` delegate.
- That handler has no explicit target, so it translates the delegate that the
  parent `Column` positions.
- The favorite `DropArea` returns when `drop.accepted` is true before checking
  the internal favorite MIME type. Qt drop events begin accepted, making the
  reorder branch unreachable.
- `AppStateFacade::moveSidebarFavorite` mutates and persists a complete list at
  drop time, but there is no transient ordered model for displacement during a
  drag.
- `AppStateFacade::deviceModel()` returns a `QVariantList` of maps. The device
  delegate reads `model.*` rather than `modelData.*`, so the title/icon fields
  are not reliably bound.
- Rust device enumeration already emits `title`, `subtitle`, `icon`, mount
  state, and capability fields.
- `IconThemeService` currently renders all provider requests using the normal
  `QIcon` artwork path. QML `MultiEffect` tinting is not a sufficient contract
  for semantic monochrome icons, while file icons must retain normal artwork.

## Architecture

### Ordered favorites model

Add:

```text
Apps/Explorer/native/src/models/sidebar_favorites_model.h
Apps/Explorer/native/src/models/sidebar_favorites_model.cpp
```

`SidebarFavoritesModel` derives from `QAbstractListModel` and exposes stable
ordered rows with these roles:

```text
PathRole  = Qt::UserRole + 1
LabelRole
IconRole
```

The model owns the effective ordered rows used by the sidebar. It supports:

- loading the existing object-array or legacy string-array JSON representation;
- returning the current rows as a `QVariantList` for compatibility;
- finding a row by normalized path;
- moving one row with `beginMoveRows()`/`endMoveRows()`;
- a drag transaction that snapshots the initial order, previews moves without
  writing settings, commits the final order, or restores the snapshot.

Downward moves use Qt's destination-child semantics correctly: the model calls
`beginMoveRows()` with `to + 1` when the final index is below the source and
then moves the item to the requested final index.

`AppStateFacade` owns the model and exposes it as a `QAbstractItemModel*` QML
property. Existing `sidebarFavorites`, JSON properties, pin/remove methods,
and compatibility adapters remain available. They all delegate to the model
and one persistence boundary, so QML cannot maintain an independently mutable
favorite order.

The facade exposes transaction operations equivalent to:

```text
beginSidebarFavoriteDrag(path)
previewSidebarFavoriteMove(path, finalIndex)
commitSidebarFavoriteDrag()
cancelSidebarFavoriteDrag()
```

The commit operation serializes the final model order once through the existing
settings service. Pin/remove and external JSON writes refresh the model and
preserve the existing settings format and default-favorite hiding behavior.

### QML reorder presentation

Replace only the Favorites `Repeater` with a nested, non-interactive
model-backed `ListView` inside the existing sidebar `ScrollView`/`Column`:

- `model: AppState.sidebarFavoritesModel`;
- `interactive: false` so the outer sidebar remains the scrolling owner;
- `height: contentHeight`;
- subtle `NumberAnimation` displacement/move transitions in the existing
  approximately 100–160 ms `OutCubic` style.

The real delegate uses `DragHandler { target: null }`. A transient proxy copies
the favorite's label/icon and follows the pointer in a clipped overlay above
the list. The model changes only when the pointer crosses a new insertion
position, so neighboring delegates move through proper row moves rather than
arbitrary `y` translation. A visible insertion marker identifies the target.

The drag coordinate is recalculated against the current list position on every
pointer update. This keeps the target correct if the outer sidebar scrolls
during the drag.

On release, the final model order is committed. On handler cancellation, the
transaction is rolled back and the proxy disappears. A same-index move is a
no-op. The QML state explicitly distinguishes an internal favorite drag from
filesystem URL drops.

The existing row `DropArea` remains for file/folder URL drops only and is
disabled while an internal favorite transaction is active. It no longer
interprets an internal favorite MIME payload, avoiding accidental file
operations and the old accepted-drop early return.

### Symbolic semantic icons

Extend `IconThemeService` and `AstreaIconImageProvider` with an explicit render
mode:

```text
FullColor  — file-content icons and previews
Symbolic   — sidebar and portal semantic icons
```

Semantic requests encode the mode in the provider URL. Native rendering
converts the resolved theme icon into a monochrome alpha-preserving image;
QML continues to select idle, hover, and active colors with the existing
palette. Full-color file requests retain the current `QIcon` artwork path.

`AppStateFacade::themedIconSource()` becomes the semantic route while
`fileIconSource()` remains the full-color route. The existing provider cache
key includes render mode, size, device-pixel ratio, and candidates so the two
policies cannot share an incorrectly colored image.

### Device rendering

Keep the Rust and native device contracts unchanged. In `Sidebar.qml`, bind
device delegate fields through `modelData` because `deviceModel` is a
`QVariantList` of maps. Use the backend-provided icon/title and retain a safe
drive-type fallback only when the icon field is empty. Display the existing
subtitle without changing the surrounding sidebar visual language.

## Testing and validation

Add focused regression coverage:

### Native model and facade

- load legacy string-array and current object-array favorites;
- move upward, downward, same-index, first, and last rows;
- verify `beginMoveRows` model notifications and final order;
- verify cancel restores the snapshot;
- verify commit persists once and restart reloads the final order;
- verify pin/remove/default-hidden behavior remains compatible.

### QML/source interaction tests

Extend `test_explorer_qml.py` to assert:

- the Favorites view is model-backed and non-interactive;
- the drag handler has `target: null`;
- the proxy/insertion path exists;
- no favorite reorder branch is guarded by the old unconditional
  `drop.accepted` return;
- filesystem drop handling remains wired separately;
- device bindings use `modelData.*` and preserve title/icon fields;
- semantic and file icon routes remain distinct.

### Icon service/provider

Add tests that render the same colored theme icon in both modes and verify that
symbolic output is monochrome with preserved alpha while full-color output
retains its original color. Continue testing requested sizes, fallback output,
theme reload, and cache invalidation.

### Device path

Extend the existing typed-device test to assert title, subtitle, and icon
survive backend decoding. No Rust changes or Rust-only tests are planned unless
the evidence changes.

### Build and visual review

Reuse the existing `build-closure-release` directory, run the focused tests,
the full native CTest suite, the Python QML tests, `git diff --check`, and the
normal offscreen Explorer self-test. Perform a visual review of the installed
`Super+E` runtime covering dark-mode icon contrast, favorite drag displacement,
cancel/commit behavior, scrolling during drag, filesystem drops, and device
rows.

## Acceptance criteria

- Favorite rows visibly displace around a clear insertion marker while the
  proxy follows the pointer.
- Upward and downward moves commit the exact visible order; same-position drops
  do nothing; cancellation restores the original order; restart preserves the
  committed order.
- Sidebar scrolling during a drag does not corrupt the target or persisted
  order.
- Filesystem drops onto sidebar destinations continue to work and never become
  favorite reorders.
- Semantic sidebar/portal icons are visibly tintable light symbolic images in
  dark mode; file-content icons remain normal theme artwork.
- Connected devices show their title and actual removable/internal device icon.
- Existing persisted favorite JSON remains readable and compatible.
- No unrelated files are reset, cleaned, overwritten, or staged.
