# Native Icon Theme Integration

Astrea Explorer resolves file, MIME, device, and sidebar icons in native Qt
code. QML keeps responsibility for layout, sizing, selection, drag behavior,
and thumbnail presentation.

## Ownership and semantics

`Services::IconThemeService` owns:

- canonical desktop icon-theme selection and identifier validation;
- MIME-derived candidates from `QMimeDatabase::MatchExtension`;
- special-directory identity from actual paths and `QStandardPaths`;
- symbolic aliases for semantic sidebar and device roles;
- a bounded rendered-image cache keyed by theme revision, candidate identity,
  requested size, and device-pixel ratio;
- canonical configuration and active-theme asset watching;
- revision invalidation for visible theme changes.

`Services::FreedesktopIconThemeCatalog` owns the Freedesktop-specific lookup
topology behind that policy. It proves installation from valid `index.theme`
metadata found through `QIcon::themeSearchPaths()`, uses the first index file
in search-path order as the theme description while collecting matching asset
roots from all search paths, and reads declared `Inherits`, `Directories`,
`ScaledDirectories`, `Size`, `Scale`, `Type`, `MinSize`, `MaxSize`, and
`Threshold` values. It resolves inherited themes recursively with cycle
protection, keeps the platform fallback after the declared family, and always
places `hicolor` at the end of the themed family. Candidate lists are tested
theme-by-theme, so an available generic icon in the selected theme cannot be
displaced by a more-specific icon from a fallback theme. The catalog returns
an exact `{themeName, iconName, filePath}` descriptor selected by the
Freedesktop size/scale rules; the service loads that file directly.

`Runtime::AstreaIconImageProvider` exposes rendered results through
`image://astrea-icons/...`. URLs contain candidate identities, never theme
filesystem paths. Theme images are synchronous in QML; thumbnail work remains
on its existing asynchronous path because it is filesystem-backed work.

`AppStateFacade` keeps the compatibility surface while making the two icon
pipelines explicit:

- `themedIconSource` / `portalIconSource` request full-color artwork;
- `sidebarIconSource` requests real symbolic artwork and symbolic fallbacks;
- `fileIconName` and `fileIconSource` remain path-aware native wrappers.

QML does not select a theme, construct a theme path, or recolor ordinary
full-color artwork into a sidebar icon.

Rich file icon sources preserve the existing explicit Orbit semantic override
first. For ordinary filesystem rows, a validated local custom-icon file is the
primary source and carries a version query component so replaced metadata
assets invalidate the QML image URL; the ordered GIO standard-icon names travel
alongside it as the exact-file decode fallback. When no exact file is present,
ordered GIO names precede the existing path/MIME candidates. Emblem identities
are retained as supplied, while the native lookup tries the corresponding
`emblem-` keyword, unprefixed identity, and compatible symbolic variants.
Emblem sources resolve without a generic missing-icon fallback, which lets the
two views omit absent emblems instead of drawing a misleading badge.

The Rust metadata boundary in
apps/explorer/backend/src/file_visual_metadata.rs uses GIO/GVfs for local
GFileInfo queries, including standard::icon, standard::is-symlink, access
flags, custom icon metadata, and emblem metadata. It preserves GIO's ordered
themed-icon names, or returns a validated local custom-icon URI plus a version
derived from the icon file's mtime and size while carrying the standard names
for exact-file decode fallback. Non-local or unsupported GIcon implementations
are represented as safe empty values; they are never downloaded by Explorer.

## Desktop theme selection

`IconThemeService` keeps three related values distinct:

- `configuredBaseTheme()` is the persisted `desktop_icon_theme` value;
- `appearance()` is Borealis' canonical Light/Dark interpretation;
- `effectiveTheme()` is the installed Qt theme selected for this process.

For example:

```text
desktop_icon_theme = MacTahoe
appearance         = dark
effectiveTheme     = MacTahoe-dark
```

The persisted base remains `MacTahoe`; appearance changes only affect the
runtime selection.

The effective desktop theme is selected in this order:

1. valid and installed `ASTREA_ICON_THEME`;
2. valid and installed `desktop_icon_theme` in
   `~/.config/AstreaOS/ui/theme.json`, preferring the matching `-dark` or
   `-light` sibling when the configured value is a base theme;
3. the installed `MacTahoe` compatibility default, using the same appearance
   sibling rule when the platform theme is the generic `hicolor` theme;
4. the installed platform `QIcon::themeName()` when it is a more specific
   installed theme;
5. Qt/Freedesktop fallback lookup, followed by a built-in Astrea fallback
   image when no themed icon is available.

`icon_theme` is reserved for Borealis/Astrea's internal application style and
is not interpreted as the desktop icon-theme key. This prevents a value such
as `dark` from shadowing the actual installed desktop theme.

Theme installation is proven from actual Freedesktop metadata, not from
`QIcon::fromTheme()` or `QIcon::hasThemeIcon()` probe results. Qt can answer
those probes through fallback themes and platform-native providers, which
would incorrectly make an absent appearance sibling look installed.

Theme identifiers are strict identifiers. Empty values, separators,
traversal, and arbitrary path-like values are rejected. A configured explicit
`-dark` or `-light` variant is respected as-is and is never double-suffixed.
The environment override is also exact: `ASTREA_ICON_THEME=DebugTheme` does
not become `DebugTheme-dark`.

The appearance rule is the same one used by Borealis: `theme == "light"` or
`theme_mode == 1` means Light; every other value means Dark. Theme existence
checks are catalog reads and do not temporarily mutate global `QIcon` state.

The catalog resolves each candidate list in family order: all candidates in
the selected theme, then recursively declared inherited themes in declaration
order, then the public Qt platform fallback theme, then `hicolor`. For each
theme and candidate, lookup first scans declared subdirectories for an exact
`DirectoryMatchesSize` result: scale must match, and Fixed, Scalable, and
Threshold directories apply their respective size rules. Only when no exact
asset exists does it scan available assets for the smallest
`DirectorySizeDistance`, based on scaled physical size; scale has no special
priority in this closest-match phase. Subdirectory declaration order comes
before base-directory order, while base-directory order still wins when the
same subdirectory exists in multiple roots. Only after that exact/closest
themed search fails does the service use Qt's ordinary global/platform
fallback behavior. The exact file is loaded through public `QIcon(filePath)`
APIs, so Qt remains responsible for decoding and rendering without performing
a second theme hierarchy lookup. PNG is always considered; XPM and SVG are
considered only when Qt's public image readers advertise support. SVGZ is
intentionally not considered without a deterministic exact-file support proof.

The service watches the filesystem-backed search roots and active/relevant
family directories. Debounced changes to `index.theme`, inherited content,
icon assets, or an appearance sibling invalidate catalog data, re-evaluate
theme selection, clear rendered results, and advance the provider URL revision
when visible output may have changed. Root-directory events are filtered
against a topology signature, so installing a missing preferred sibling is
noticed while an unrelated theme does not trigger a revision. Rewriting
`theme.json` without changing the effective theme does not trigger a sidebar
refresh.

The service API accepts an explicit device-pixel ratio and selects scaled
directories accordingly. Each Explorer visual surface computes a bounded
per-window ratio (`0.5..4.0`), passes it through the source URL, and requests
physical decode pixels while keeping its layout geometry in device-independent
pixels. The image provider uses the logical `size` for theme selection and
rendering, the physical requested size for raster decode, and preserves the
ratio on the returned image so Qt does not confuse DPR variants in its image
cache.

## File visual metadata lifecycle

The Rust worker handles bounded batches of at most 64 local paths. Navigation
keeps one bounded batch in flight, while queued work is replaced by the latest
visible list/grid range in model order; duplicate paths are coalesced and the
50 ms debounce is restarted for the newest range. Requests are canceled when
a directory, search, tab, or refresh changes the model generation. A result is
applied only when both its request generation and file path still match the
active model; failures leave the regular MIME/path icon fallback intact.

The first listing remains cheap: ordinary directory enumeration carries
symlink identity and broken-target state, while GIO icon/emblem metadata is
hydrated lazily for visible local rows. Remote and metadata-limited rows are
never sent to this local-only worker. Thumbnail readiness is independent of
icon metadata readiness, so a preview can appear without suppressing the
base icon or its emblem overlay.

Symlink rows keep the link's visible name/path and are not followed during
recursive search descent. If the target exists, directory/file semantics and
executable state come from the target; a broken link remains a visible,
non-directory row with fileSymlinkBroken=true. Automatic emblems follow the
Nautilus convention for symbolic links, inaccessible items, and read-only
items, then merge with GIO metadata emblems while preserving the first real
identity, order, and semantic deduplication. Trash items do not receive the
automatic read-only emblem.

GIO metadata is queried on demand rather than watched independently. A
filesystem refresh or another visible-range request rehydrates rows when the
model is replaced; changes to custom icon metadata made outside those events
may therefore wait until the next directory refresh or visibility request.

## Full-color and symbolic pipelines

Full-color file and portal icons use normal theme candidates, including MIME
icons, semantic directory names, device names, and generic fallbacks.

Sidebar icons use a separate candidate list that prefers the theme's actual
symbolic names. Important semantic mappings include:

- `user-home`, `user-desktop`, and `folder-download` keep their desktop-role
  identity;
- `inode-directory` maps to `folder-symbolic`;
- `folder-home` and `folder-desktop` map to `user-home-symbolic` and
  `user-desktop-symbolic`;
- `folder-downloads` tries both `folder-download-symbolic` and
  `folder-downloads-symbolic`;
- removable drives preserve `drive-removable-media-symbolic` before the
  hard-disk fallback;
- network and trash roles retain `network-workgroup-symbolic` and
  `user-trash-symbolic`.

The renderer displays the artwork supplied by the icon theme. It does not
turn every non-transparent pixel white. If no symbolic candidate exists, the
service uses a neutral symbolic fallback and never adds `mode=symbolic` to
the provider URL.

## Directory, MIME, and device behavior

Directories are matched using their actual normalized paths against XDG user
directories, the home directory, and the Trash files directory. A directory
named `Downloads` elsewhere does not receive the Downloads icon.

Regular files use MIME metadata without content probing. The service asks
Qt's shared MIME database for the MIME icon and generic icon, then applies
generic fallback candidates. Explicit semantic names supplied by recent and
device models are tried first, so model metadata remains useful without
moving theme policy into Rust or duplicating an extension table in JavaScript.

Rust device rows retain the semantic distinction between
`drive-removable-media` and `drive-harddisk`; the native symbolic pipeline
preserves that distinction when rendering the sidebar.

## Future portal follow-up

Explorer does not depend on a settings portal for icon selection. If Typhon
later exposes live settings, map the same canonical value through
`org.freedesktop.portal.Settings`, namespace `org.gnome.desktop.interface`,
key `icon-theme`, and publish live `SettingChanged` notifications. That work
belongs at the compositor/portal boundary and must not reintroduce a second
theme source inside Explorer.
