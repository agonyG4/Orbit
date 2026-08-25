# Testing Orbit

The normal qualification workflow is:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cargo test --workspace --locked
python3 -m unittest discover -s apps/explorer/tests/qml -p 'test*.py'
python3 scripts/verify_orbit_source_gate.py
python3 scripts/verify_explorer_clean_install.py
```

CTest covers the native controllers, runtime resolver, QML compatibility
boundary, Rust service integration, and shadow-parity fixtures. The Rust
workspace command covers the Explorer backend, launcher, and portal packages.
The Python suite checks the QML source contract without requiring a display.

The source gate is a permanent structural check: it rejects the retired
migration tree, uppercase legacy boundary, generated files, source symlinks,
historical runtime paths, and missing canonical ownership directories.

The clean-install gate builds from the Orbit root into a temporary build tree,
installs into a temporary prefix, isolates HOME and XDG state, rejects cache
leakage, and runs both normal and portal `--self-test` modes with runtime
overrides removed.
