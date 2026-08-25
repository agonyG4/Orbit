# Orbit Structural Reset Design

## Goal

Turn Orbit into the maintainable AstreaOS native-applications repository described by the supplied structural-reset brief while keeping Explorer behavior, visual output, language boundaries, protocols, and the qualified installed runtime contract unchanged.

## Current evidence

The active implementation is currently under `Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07/source/AstreaOS`. The checkout also tracks generated CMake/Qt/Rust output inside that migration kit, keeps Explorer C++ under `Apps/Explorer/native`, uses QML compatibility symlinks, and has separate Rust lockfiles. `Old/` contains legacy applications and generated artifacts.

The baseline captured before any move is:

- `verify_native_migration_gate.py`: PASS.
- Python/QML discovery suite: 28 tests PASS.
- Explorer backend, launch, and portal Rust suites: 55, 16, and 9 tests PASS.
- Isolated clean-install script: PASS.
- Existing tracked Debug CTest tree: 20/22 PASS; the two failures are stale real-QML bootstrap tests.
- Existing tracked Release CTest tree: 22/26 PASS; the two stale real-QML failures and two missing test executables are pre-existing build-tree issues.

Fresh root builds are the qualification target; stale tracked build output is not treated as a source of truth.

## Scope and non-goals

In scope:

1. Promote the active Explorer source, tests, backend, shared QML, services, scripts, and living documentation into their canonical owners.
2. Add root CMake, CMake presets, a root Cargo workspace, and a focused Cargo integration helper.
3. Generate a development runtime staging tree under the build directory and keep the installed Astrea runtime destinations stable.
4. Replace migration-specific validation with permanent Orbit source and clean-install gates.
5. Remove migration material, generated output, source compatibility symlinks, and stale legacy build/cache artifacts.
6. Rename `Old/` to `old/` without changing legacy application behavior.

Out of scope:

- Explorer UI redesign or behavior changes.
- Protocol, JSON contract, D-Bus, CLI, persistence, launch, file-operation, or backend changes.
- Namespace mass-renaming or unrelated large-file decomposition.
- Migration of any application from `old/`.
- Changing the externally consumed installed runtime hierarchy.

## Target ownership

The active source will have one owner for each domain:

```text
apps/explorer/
  qml/          Explorer presentation, state, and local components
  src/          Explorer C++/Qt application and ownership-separated libraries
  backend/      explorer_backend Rust crate
  tests/        Explorer C++, QML/Python, and parity tests
  docs/         living Explorer-specific documentation
shared/qml/Astrea/
  Components/  shared component public wrappers and categorized implementations
  Files/       shared file UI and drag/drop module
  I18n/        translation QML/data module
services/
  launch/      astrea-launch Rust crate
  filechooser-portal/  astrea-filechooser-portal Rust crate
  session/     astrea-services.sh
cmake/         OrbitRust.cmake
scripts/       permanent repository gates
docs/          current Orbit architecture/build/test documentation
old/           legacy source only
```

The existing `Astrea::Explorer::Native` namespace remains unchanged unless a move exposes a concrete compiler or ownership issue. Removing the historical directory name is sufficient.

## Build design

`CMakeLists.txt` at the repository root is the canonical native entry point. It owns the project declaration, C++17/Qt 6.11 policy, common settings, testing, shared helper inclusion, and `add_subdirectory(apps/explorer)`.

`apps/explorer/CMakeLists.txt` owns Explorer targets, C++ libraries, tests, QML module registration, and installation mapping. Repeated Rust custom commands are replaced with a small `cmake/OrbitRust.cmake` helper that accepts a workspace package, profile, shared target directory, output artifact, and source dependencies. Rust output is always under the active build tree.

The root `Cargo.toml` is a workspace with resolver 3 and exactly these active members:

```toml
apps/explorer/backend
services/launch
services/filechooser-portal
```

Package names, binary names, dependency versions, protocols, and lock resolution are preserved. One root `Cargo.lock` is authoritative for active Orbit crates; lockfiles in moved crate directories are removed.

`CMakePresets.json` provides stable `debug` and `release` configure/build/test presets using `build/debug` and `build/release`. These are ignored generated trees and are reused across normal verification runs.

## Runtime and installation design

The source layout and installed runtime layout are intentionally separate contracts.

The development build generates `build/<config>/runtime/Astrea/` with the installed-compatible directories:

```text
Apps/Explorer
Core/components
Features/Files
System/i18n
Core/bridge/apps/explorer_backend
bin/astrea-launch
bin/astrea-filechooser-portal
System/services/astrea-services.sh
```

The staging tree is populated deterministically by CMake from the new source owners and build artifacts. It is never tracked and never recreated with source symlinks.

Runtime resolution is deterministic and tested in this order:

1. Explicit `ASTREA_ROOT`; an explicitly supplied empty or invalid root fails rather than silently falling through.
2. A valid installed runtime derived from the executable location.
3. The configured development staging root for a development build.
4. A valid user-local installed runtime.
5. A useful failure diagnostic.

The install destinations remain the currently qualified Astrea paths so external AstreaOS consumers are not broken by this source refactor. Clean-install verification removes `ASTREA_ROOT` and all import-path overrides, isolates HOME/XDG paths, checks required files and executability, and launches both normal and portal self-tests.

## QML module design

Shared QML is moved to real module directories under `shared/qml/Astrea/`. Existing public wrapper files and categorized implementation files are preserved. Active Explorer imports change from relative symlink paths to `Astrea.Components`, `Astrea.Files`, and `Astrea.I18n`, with aliases retained where the current code relies on them. The Explorer QML module is registered from `apps/explorer/qml` and receives `NativeBootstrap.qml` in its local `native/` subdirectory only as a QML ownership grouping, not as a source-layout boundary.

The active source contains no Quickshell dependency and no QML `Process` nodes. Legacy applications remain under `old/`; any relative links that would become dangling after the move are either removed when they are only migration plumbing or updated only when needed to preserve a legacy source reference. No generated file, cache, or build tree is retained.

## Validation design

The permanent source gate checks architecture and hygiene, including absence of `Bench/`, `Old/`, `source/AstreaOS`, `apps/explorer/native`, Quickshell/process markers in active production source, tracked build/cache output, tracked targets, and active QML compatibility symlinks. It also checks required root build/workspace directories and package members.

The clean-install gate builds from the repository root, installs to an isolated prefix, checks the stable runtime contract, rejects source/build/cache leakage, and runs normal and portal `--self-test` smoke tests without `ASTREA_ROOT`.

Qualification runs:

- `python3 scripts/verify_orbit_source_gate.py`
- Python/QML tests from `apps/explorer/tests` and shared module test locations
- `cargo fmt --all --check`
- `cargo check --workspace --locked`
- `cargo clippy --workspace --all-targets --locked -- -D warnings`
- `cargo test --workspace --locked`
- `cmake --preset debug`, build, and CTest
- `cmake --preset release`, build, and CTest
- QML load/lint checks available in the checkout
- `python3 scripts/verify_explorer_clean_install.py`
- `git diff --check` and stale-path/build-output audits

Any environment-blocked command is reported with its exact command and reason; no successful result is inferred from historical migration reports.

## Commit boundaries

The work is kept reviewable through coherent commits:

1. This design and implementation plan.
2. Root build/workspace infrastructure and promoted source layout.
3. QML modules, runtime staging, and installation mapping.
4. Permanent gates and current documentation.
5. Migration-kit retirement, legacy cleanup, and final qualification fixes.

Each commit is checked for unrelated changes before it is created.
