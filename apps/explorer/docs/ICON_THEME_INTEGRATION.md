# Native Icon Theme Integration

Astrea Explorer resolves file, MIME, device, and sidebar icons in native Qt
code. QML keeps responsibility for layout, sizing, selection, drag behavior,
and thumbnail presentation.

## Ownership and semantics

`Services::IconThemeService` owns:

- canonical desktop icon-theme selection and identifier validation;
- `QIcon` lookup and theme search paths;
- MIME-derived candidates from `QMimeDatabase::MatchExtension`;
- special-directory identity from actual paths and `QStandardPaths`;
- symbolic aliases for semantic sidebar and device roles;
- a bounded rendered-image cache keyed by theme revision, candidate identity,
  requested size, and device-pixel ratio;
- canonical configuration watching and revision invalidation.

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
3. the usable platform `QIcon::themeName()`;
4. the installed `MacTahoe` compatibility default, using the same appearance
   sibling rule when available;
5. Qt/Freedesktop fallback lookup, followed by a built-in Astrea fallback
   image when no themed icon is available.

`icon_theme` is reserved for Borealis/Astrea's internal application style and
is not interpreted as the desktop icon-theme key. This prevents a value such
as `dark` from shadowing the actual installed desktop theme.

Theme identifiers are strict identifiers. Empty values, separators,
traversal, and arbitrary path-like values are rejected. A configured explicit
`-dark` or `-light` variant is respected as-is and is never double-suffixed.
The environment override is also exact: `ASTREA_ICON_THEME=DebugTheme` does
not become `DebugTheme-dark`.

The appearance rule is the same one used by Borealis: `theme == "light"` or
`theme_mode == 1` means Light; every other value means Dark. During theme
probes, the service restores the previous global `QIcon` theme so discovery
does not leak temporary state into the running application.

Only an effective runtime theme change increments the revision, clears
rendered results, emits `themeChanged`, and changes the provider URL revision.
Rewriting `theme.json` without changing the effective theme does not trigger a
sidebar refresh. A Light/Dark transition that selects a different installed
variant updates the visible icons without restarting Explorer.

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
