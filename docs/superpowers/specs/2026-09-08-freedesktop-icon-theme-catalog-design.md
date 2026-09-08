# Freedesktop Icon Theme Catalog Design

## Goal

Make Explorer select and resolve icons according to the selected Freedesktop
theme family, without relying on Qt fallback success to prove theme
installation and without moving icon policy into QML or adding GTK/GIO.

## Alternatives considered

1. Reorder calls to `QIcon::fromTheme()` or temporarily change the global Qt
   theme. This cannot distinguish a selected theme from Qt fallback providers,
   and it still resolves candidate names one at a time across the whole
   hierarchy.
2. Resolve and render icon files directly in Explorer. This would duplicate
   Qt's size, scale, SVG/XPM, and platform rendering behavior and would make
   Explorer a second icon renderer.
3. Add a small Qt-only Freedesktop catalog that reads theme metadata and
   presence, uses the specification's list-aware hierarchy traversal to select
   a candidate identity, and delegates the winning identity to public
   `QIcon::fromTheme()`. This preserves Qt rendering while fixing theme and
   candidate priority. This is the selected approach.

## Ownership and interfaces

Create `FreedesktopIconThemeCatalog` in
`apps/explorer/src/services/freedesktop_icon_theme_catalog.{h,cpp}`.

The catalog will provide:

- strict `themeExists(themeName)` based on a valid `index.theme` found in
  `QIcon::themeSearchPaths()`;
- deterministic `themeFamily(themeName)` traversal;
- `resolveIconName(themeName, candidates)` for list-aware candidate selection;
- `watchPaths(themeName)` for filesystem-backed active-family invalidation;
- explicit cache invalidation.

The catalog will parse the first valid `index.theme` in search-path order for
metadata, while checking icon assets in every matching theme directory across
all search roots. It will include both `Directories` and `ScaledDirectories`
entries and recognize PNG, SVG, XPM, and SVGZ files. Presence checks are
cached by theme and candidate and are bounded by the active family and
observed candidate names.

`IconThemeService` remains the owner of theme selection, appearance variants,
candidate construction, rendered-image caching, symbolic semantics, and QML
URLs. `resolveIcon()` will ask the catalog for one winning candidate, render
that candidate with public `QIcon::fromTheme()`, and retain a final per-name
Qt fallback path only when the catalog finds no themed candidate.

## Theme-family order

Starting at the effective selected theme, the catalog will traverse each
declared `Inherits` entry recursively in declaration order, with a visited set
to prevent cycles and duplicates. `QIcon::fallbackThemeName()` is appended as
an implementation fallback after the declared subtree. `hicolor` is appended
last and is never allowed to move earlier because of a malformed inheritance
list. Missing themes are skipped.

For candidates `A|B|C`, each theme is checked as `A`, then `B`, then `C`
before moving to the next theme. This is the Freedesktop `FindBestIcon`
semantics needed for MIME and semantic fallbacks.

## Live invalidation

The existing debounced watcher will also track filesystem-backed icon search
roots, active theme directories, declared icon subdirectories, and
`index.theme` files for the active family. Resource paths such as `:/icons`
will not be passed to `QFileSystemWatcher`. A relevant theme topology or asset
event invalidates the catalog and rendered cache, re-evaluates appearance
selection, rebuilds watcher paths, advances the revision, and emits
`themeChanged` when visible results may have changed even if the effective
theme name is unchanged. Search-root events are limited to top-level theme
topology changes; active-family directory events cover asset changes.

## Rendering

Rendering remains synchronous on the GUI thread. The service will use the
public device-pixel-ratio-aware `QIcon::pixmap()` overload and avoid an
unnecessary `QImage::scaled()` pass. Aspect ratio, alpha, symbolic identity,
and the built-in fallback remain unchanged.

## Tests

Extend the existing deterministic QtTest fixtures with temporary search roots
and explicit fallback themes. Tests will cover strict theme existence without
probe icons, list-aware selected-theme priority, inherited ordering, final
`hicolor`, cycles, split roots, `ScaledDirectories`, unchanged-name asset
invalidation, and all existing MIME, symbolic, special-directory, provider,
and rendering behavior. Test helpers will emit spec-valid `index.theme`
metadata and preserve `QIcon` global state with guards.

## Constraints

- Qt public APIs only; no `qiconloader_p.h` or copied Qt internals.
- No GTK/GIO dependency and no extension-to-icon-name table.
- Keep build artifacts in Orbit's existing build directory.
- Keep theme policy in native C++ and candidate identity in provider URLs.
