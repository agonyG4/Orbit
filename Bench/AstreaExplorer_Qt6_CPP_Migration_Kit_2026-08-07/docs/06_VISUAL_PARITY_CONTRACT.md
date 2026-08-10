# Visual Parity Contract

This migration is an architecture/runtime migration, not a redesign.

## Non-Negotiable Rule

> The Explorer must look the same before and after the migration unless a visual difference is required to reproduce an already-existing behavior correctly.

## Do Not Intentionally Change

- window dimensions;
- minimum dimensions;
- sidebar width;
- toolbar height/layout;
- tab visuals;
- colors;
- corner radii;
- borders;
- fonts;
- icon sizes;
- spacing/padding;
- list column layout;
- icon-grid sizing;
- hover/selection appearance;
- animations/transitions;
- preview panel layout;
- context-menu styling;
- archive/file-operation progress surfaces;
- FileChooser dialog styling;
- portal dialog styling.

## Baseline Hashes

`manifests/VISUAL_BASELINE_SHA256.txt` records SHA-256 values for the current non-state QML/JS presentation files.

These hashes are not an instruction to keep every byte unchanged. Some files contain current process orchestration that must be removed. They are a review aid: every modified visual file must have a migration-specific reason, and visual-property changes should be rejected unless required for parity.

## Screenshot Qualification

Before switching the runtime, capture baseline screenshots on the real system for at least:

1. Explorer list view;
2. Explorer icon view at default zoom;
3. icon view at largest existing zoom preset;
4. preview panel open;
5. multi-tab view;
6. search state;
7. trash view;
8. file context menu;
9. Compress submenu;
10. Open With dialog;
11. properties dialog;
12. create-folder dialog;
13. rename dialog;
14. paste-conflict dialog;
15. archive password dialog;
16. archive conflict dialog;
17. device section in sidebar;
18. network-connect flow;
19. FileChooser open-file mode;
20. FileChooser save-file mode.

Capture after migration using the same theme, scale, window geometry, data set, and view state.

Pixel-perfect automation is desirable if the environment can make it deterministic, but manual side-by-side qualification is still required before declaring visual parity.

## QML Review Rule

When replacing a `Process {}` block in a visual component:

- replace only the behavior connection;
- preserve the surrounding Item/Rectangle/Row/Column/Popup hierarchy;
- preserve visual properties and animations;
- do not opportunistically clean up styling;
- do not rename UI-facing roles unless required by a typed compatibility layer.
