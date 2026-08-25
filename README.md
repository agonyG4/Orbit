# Orbit

Orbit is the AstreaOS native applications repository. The current production
application is the Qt 6/C++ Explorer with Rust components for filesystem
backend work, launching, and the file chooser portal.

The `old/` directory contains preserved legacy application source. It is an
archive boundary, not part of the active build. `Bench/` and the former
`Apps/Explorer/native` migration layout are retired.

## Build

Prerequisites are Qt 6.11+, a C++17 compiler, CMake 3.23+, Rust/Cargo, and a
session D-Bus implementation for the portal test.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Release uses the corresponding `release` configure, build, and test presets.
The canonical Rust check is:

```sh
cargo test --workspace --locked
```

## Tests and qualification

Run the Python/QML checks with:

```sh
python3 -m unittest discover -s apps/explorer/tests/qml -p 'test*.py'
python3 scripts/verify_orbit_source_gate.py
python3 scripts/verify_explorer_clean_install.py
```

The clean-install check builds from the repository root into an isolated
prefix, clears runtime environment overrides, and smoke-tests normal and
portal Explorer startup.

## Repository shape

Production ownership is explicit:

```text
apps/explorer/       Explorer QML, C++, Rust backend, and tests
shared/qml/Astrea/   Astrea.Components, Astrea.Files, Astrea.I18n modules
services/            launch, file chooser portal, and session integration
scripts/             permanent source and clean-install gates
docs/                repository architecture, build, and test guidance
old/                 legacy source archive
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/BUILDING.md](docs/BUILDING.md),
and [docs/TESTING.md](docs/TESTING.md) for the living repository contract.
