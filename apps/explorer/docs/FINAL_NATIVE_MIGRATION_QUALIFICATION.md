# Astrea Explorer Final Native Migration Qualification

Date: 2026-08-14

## Result

The native Qt6/C++ Explorer is the configured production runtime. The public
QML contract remains `AppState.qml`; no production QML process or Quickshell
runtime is required. CMake builds and installs the Rust backend, the portal,
and the source-owned `astrea-launch` provider as one required runtime closure.
The normal native runtime uses one persistent backend worker.

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
| Source-owned launch provider | PASS: recovered Rust source/manifest/tests; no tracked ELF prerequisite |
| Required CMake closure | PASS: `astrea-explorer`, backend, portal, and `astrea-launch` build from source |
| Runtime capability validation | PASS: normal requires backend + `astrea-launch`; portal requires backend |
| XDG desktop IDs and MIME associations | PASS: injectable XDG roots, canonical IDs, effective spec lookup, validation, lock + atomic writes |
| Persistent worker lifecycle | PASS: asynchronous startup, queueing, cancellation, timeout, and crash paths |
| Portal lifecycle | PASS: async child I/O, explicit kill/reap, per-handle `Request.Close`, bounded output, four-dialog cap |
| Private portal D-Bus integration | PASS: known handle, close/removal, service survival, next request, normal completion |
| Service artifact guard | PASS: service installation refuses missing runtime executables |
| Clean-install harness | PASS: isolated HOME/XDG roots, fresh prefix, required artifacts, no build/cache/Quickshell leakage |
| Installed normal startup smoke | PASS with no `ASTREA_ROOT`, no QML override, and no Quickshell plugin |
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
  canonical nested desktop IDs, user-over-system XDG precedence, and
  deterministic ordering. Effective default/added/removed associations are
  validated against desktop entries; user writes use a line-preserving
  freedesktop `mimeapps.list` parser with locking and atomic replacement.
- Clipboard image paste reads Qt MIME data and writes atomically with unique
  names. The workflow contains no `wl-copy` or `wl-paste` dependency.
- Archive extraction validates paths and link entries, stages output, cleans up
  failed staging, and publishes according to explicit conflict policy.

## Test evidence

- Native CTest Debug: 25/25 passed.
- Native CTest Release: 25/25 passed.
- Explorer Rust backend: 41/41 passed.
- Astrea launch Rust tests: 13/13 passed.
- System portal Rust tests: 9/9 passed, including the private D-Bus lifecycle test.
- Relevant Explorer Python/QML integration: 20/20 passed.
- ZIP generator synthetic tests: 3/3 passed.
- Persistent worker: focused transport test passed; source-tree QML cases that
  require an installed backend explicitly skip when the capability is absent.
- QML lint: exit 0; no syntax or migration-introduced error diagnostics. The
  existing warning baseline remains for dynamic compatibility properties and
  legacy layout/model idioms.
- Runtime dependency audit: 0 process nodes, 0 Quickshell imports.
- Clean install: executable, QML module descriptors, catalogs, source-built
  launch provider, portal, service script, and Rust backend installed
  successfully; build/cache, compiled-source, and Quickshell artifacts were
  not installed. Both installed normal and portal self-tests passed with
  isolated HOME/XDG roots and no forced `ASTREA_ROOT`.
- Canonical source archive: `/home/agony/GitHub/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-14-final-source-only.zip`, 287 entries, verified for deterministic ordering, compiled-payload exclusion, symlink closure, and required launch source/manifest.
- Extracted archive qualification: source gate PASS and isolated clean-install/build/self-test PASS from a fresh extraction. Source reproducibility is therefore qualified for this checkout and archive.
