# Astrea Explorer Final Native Migration Qualification

Date: 2026-08-11

## Result

The native Qt6/C++ Explorer is the configured production runtime. The public
QML contract remains `AppState.qml`; no production QML process or Quickshell
runtime is required. The Rust CLI remains available for diagnostics while the
normal native runtime uses one persistent backend worker.

## Migration gates

| Gate | Result |
| --- | --- |
| QML/JS files audited | 40 |
| Production `Process {}` nodes | 37 -> 0 |
| Production Quickshell import lines | 15 -> 0 |
| Python helper | removed; no production callers |
| System launcher | `astrea-explorer` with no checkout path |
| Portal launcher | `astrea-explorer --portal`; no `qs` |
| Static source gate | PASS (`scripts/verify_native_migration_gate.py`) |
| Persistent worker smoke | PASS: JSONL directory listing completed |
| Installed normal startup smoke | PASS with `QML2_IMPORT_PATH` unset and no Quickshell plugin |
| Installed portal startup smoke | PASS: `astrea-explorer --portal --self-test` |

## Architecture summary

- C++ owns lifecycle, QML loading, the `AppState` facade, controllers, models,
  settings, Recent integration, clipboard, launch/Open With, device/network
  workflow state, portal mode, and process supervision.
- Rust owns listing/search, bulk file operations, trash, archive validation and
  extraction/compression, thumbnail warming, AppImage installation, and other
  filesystem-sensitive operations.
- QML owns presentation, delegates, layout, animation, hover/focus state,
  text editing, and popup presentation.
- Translation catalogs are loaded by the native bootstrap and exposed to the
  existing QML singleton without a helper process or runtime-specific import.
- Open With uses one asynchronous, cached desktop catalog with MIME filtering,
  XDG default association lookup, and deterministic ordering.
- Clipboard image paste reads Qt MIME data and writes atomically with unique
  names. The workflow contains no `wl-copy` or `wl-paste` dependency.
- Archive extraction validates paths and link entries, stages output, cleans up
  failed staging, and publishes according to explicit conflict policy.

## Test evidence

- Native CTest Debug: 22/22 passed.
- Native CTest Release: 22/22 passed.
- Explorer Rust backend: 41/41 passed.
- System portal Rust tests: 7/7 passed.
- Relevant Explorer Python/QML integration: 20/20 passed.
- Persistent worker: real `serve` JSONL list request returned a valid response.
- QML lint: exit 0; no syntax or migration-introduced error diagnostics. The
  existing warning baseline remains for dynamic compatibility properties and
  legacy layout/model idioms.
- Runtime dependency audit: 0 process nodes, 0 Quickshell imports.
- Source install: executable, QML module descriptors, catalogs, and Rust
  backend installed successfully; build/cache/Quickshell artifacts were not
  installed.
- Source-only archive: generated only after the final qualification checks;
  archive contents were inspected for build directories and compiled files.
  Final path: `AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-11-final-source-only.zip`.
