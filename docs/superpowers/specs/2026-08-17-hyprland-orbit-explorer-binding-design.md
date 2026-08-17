# Hyprland Super+E Orbit Explorer Binding

## Goal

Make Hyprland's `Super+E` shortcut launch the installed Orbit/Astrea Explorer app directly.

## Current context

The current binding invokes the generic `astrea-launch --desktop astrea-explorer.desktop` route. The desktop entry resolves to `/home/agony/.local/share/Astrea/bin/astrea-explorer-open`, which is the Explorer app opened for the Orbit workspace.

## Design

Update `/home/agony/.config/hypr/bindings/keybindings.lua` so `explorerLaunch` invokes the installed `astrea-explorer-open` launcher directly, while preserving the existing `ASTREA_ROOT` resolution. The launcher defaults to `$HOME` when no folder is supplied, matching normal file-manager shortcut behavior.

No source-tree QML command or new wrapper script is needed. The Orbit checkout remains the source project; the installed launcher remains the stable desktop integration point.

## Behavior and failure handling

- `Super+E` starts the Orbit/Astrea Explorer at the user's home directory.
- Existing `ASTREA_ROOT` overrides continue to work.
- If the launcher is unavailable, Hyprland reports the command failure as it does for other bindings; this change does not add a fallback or alter unrelated shortcuts.

## Verification

- Confirm the binding references `astrea-explorer-open` and still uses `hl.dsp.exec_cmd`.
- Run a Lua syntax check if an appropriate Lua interpreter is available.
- Reload Hyprland and invoke `Super+E` to confirm the Explorer window opens.
- Confirm unrelated working-tree changes are not staged or modified.
