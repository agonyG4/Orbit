# Astrea Explorer Final Source-Closure Implementation Plan

> For the implementation agent: execute this plan in the Orbit checkout while preserving unrelated working-tree changes. Use the repository's RTK wrapper for reads, search, commands, and Git operations. Do not claim reproducibility until the extracted-archive gate and isolated clean install pass.

## Global constraints

- Work in `/home/agony/GitHub/Orbit` and the explicitly supplied `/home/agony/GitHub/zip_astrea_explorer_migration_kit.py`.
- Preserve the pre-existing dirty ZIP deletion, generated Python bytecode, and Rust target artifacts; never stage them.
- Keep Explorer's `Explorer -> LaunchService -> astrea-launch` boundary and safe argv semantics.
- Prefer source-owned implementations and tests. A precompiled executable may not be a build or archive prerequisite.
- All XDG and runtime-root behavior must be injectable through explicit environment/test inputs.

## Tasks

### 1. Audit and replace the launch provider

- Inspect Orbit history, current callers, service units, and the legacy executable's observable CLI behavior. Record the audit and any parity limitation in the final journal.
- Add a source-owned `source/AstreaOS/src/System/launch` Rust crate with manifest, binary, unit tests, and a documented CLI contract covering `--file`, `--desktop`, `--url`, `--steam`, `--command`, `--argv-json`, `daemon`, `doctor`, and `history`.
- Implement desktop-entry discovery using XDG application precedence, safe `Exec` expansion, direct argv execution, URI/file fallback, bounded history persistence, and a daemon entry point suitable for the existing service unit.
- Update Astrea CMake to build/install `astrea-launch` from this crate and to make the Explorer, backend, portal, and launch targets required source-built components. Remove the tracked ELF provider and all stale-binary success paths.
- Add source and CLI/contract tests, and update `astrea-services.sh` plus the migration gate so required launch build failures are fatal and source/manifest/build markers are required instead of an executable blob.

### 2. Close XDG root and desktop-ID semantics

- Introduce an injectable XDG environment/root model for configuration, data, applications, current desktop, and home values, with spec defaults when variables are absent.
- Make `DesktopFileId` canonical for nested application paths (`/` to `-`), reject files outside configured application roots, and preserve user-over-system precedence.
- Thread the root model through desktop catalog/open-with code and add tests for nested IDs, outside-root rejection, user/system shadowing, and environment defaults.

### 3. Implement effective MimeApps resolution and safe writes

- Rework `MimeAppsService` to read the ordered XDG user/system and desktop-specific locations, apply Default/Added/Removed Associations semantics, validate desktop candidates against the effective catalog, and fall back to the first valid association.
- Keep user writes in `XDG_CONFIG_HOME/mimeapps.list`, with lock/re-read/atomic replacement and preservation of unrelated sections/keys.
- Add focused tests for precedence, desktop-specific files, removal, invalid candidates, fallback, isolated roots, and concurrent-safe write behavior.

### 4. Finish installed-prefix runtime discovery and clean-install isolation

- Extend runtime resolution to prefer explicit `ASTREA_ROOT` (with loud failure when invalid), then `<prefix>/share/Astrea` for an installed executable, then user install, then development candidates.
- Add resolver tests for arbitrary installed prefixes, invalid explicit roots, user precedence, and development fallback.
- Harden `verify_explorer_clean_install.py` to create isolated HOME/XDG config/data/state/cache/runtime directories, clear Astrea/QML overrides, avoid borrowing prior installs, and run installed smoke without forcing `ASTREA_ROOT`.

### 5. Qualify portal lifecycle and private D-Bus behavior

- Refactor the portal service around an injectable dialog runner and ensure cancellation, timeout, shutdown, spawn/read errors, and normal completion all terminate/reap children and remove request objects.
- Add race/limit tests and a private-session-D-Bus integration harness that verifies a known request handle, `Request.Close`, object removal, service survival, a subsequent request, and normal completion.
- Register the integration test in the appropriate build/test path without depending on the user's session bus.

### 6. Repair the canonical source ZIP generator

- Update `/home/agony/GitHub/zip_astrea_explorer_migration_kit.py` so `bin` is not globally excluded, generic `obj`/`out` are retained unless path-aware generated rules identify them, and generated/cache directories are excluded by context.
- Preserve executable source scripts and internal relative symlinks; reject/report absolute, outside-root, and missing symlink targets; preserve permissions and deterministic ordering.
- Add `--verify`/automatic post-generation validation for duplicate paths, compiled artifacts, required source-owned launch closure, symlink validity, and expected root layout.
- Add synthetic fixture tests for source `bin`, `obj`, `out`, build/cache/target directories, compiled headers, executable scripts, symlinks, determinism, and atomic replacement.

### 7. End-to-end qualification and documentation

- Run focused unit tests first, then Debug/Release CMake builds and tests, Rust backend/launch/portal tests, Python ZIP tests, migration gate, service syntax checks, and the isolated clean-install harness.
- Generate the canonical source ZIP, extract it to a fresh directory, rerun the source gate and clean-install/build smoke there, and inspect the archive for forbidden compiled payloads and symlink violations.
- Update qualification and journal documents only with command-backed results, explicitly separating historical results from this final source-closure run.
- Review the diff, stage only task-owned source/tests/docs/script changes, and create focused commits; do not include pre-existing generated artifacts or the user-owned deleted archive.
