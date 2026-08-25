# Building Orbit

## Prerequisites

Install Qt 6.11 or newer with Core, Gui, Qml, Quick, QuickControls2, and Test;
CMake 3.23 or newer; a C++17 compiler; Rust/Cargo; and `dbus-run-session` for
the portal test.

## Canonical builds

Use the stable per-configuration trees:

```sh
cmake --preset debug
cmake --build --preset debug
cmake --preset release
cmake --build --preset release
```

The root `CMakeLists.txt` owns Qt discovery, testing, and the Explorer
subdirectory. `cmake/OrbitRust.cmake` builds all Rust packages into the
configuration's CMake tree; no Rust `target/` directory is created in source.

## Installation

Choose an explicit prefix when installing:

```sh
cmake --install build/debug --prefix "$PWD/build/debug/install"
```

The installed application is `bin/astrea-explorer`; its runtime and service
artifacts are under `share/Astrea`.

For an isolated qualification build, run:

```sh
python3 scripts/verify_explorer_clean_install.py
```
