# Build, Packaging, and Session Integration

## Native Binary

Target executable:

```text
astrea-explorer
```

Suggested modes:

```text
astrea-explorer
astrea-explorer --portal
```

Optional diagnostic modes may be added later, but avoid turning the GUI executable into a general shell command multiplexer.

## Qt Requirements

Use Qt 6 components appropriate to actual source usage, expected to include at least:

```text
Core
Gui
Qml
Quick
QuickControls2
```

Add Concurrent only if the C++ implementation genuinely uses it.

## QML Packaging

Prefer `qt_add_qml_module` so QML types, imports, and cache compilation are deterministic.

Preserve the current Explorer module/type API where practical. Do not reorganize all visual QML paths during the runtime migration.

Enable normal Qt release/AOT tooling supported by the chosen Qt version rather than implementing manual QML compilation.

## Shared Modules

Explorer currently consumes `Core/components`, `Features/Files`, and `System/i18n` through relative symlinks.

During native packaging, choose one consistent model:

1. package those shared modules as real Qt QML modules; or
2. preserve the repository-relative module resources during the first native migration.

The migration must not copy/fork their visual implementation into Explorer merely to make imports easier.

## Launch Configuration

Replace Explorer launch commands that currently invoke `qs -p` with the installed native binary.

Audit:

- file-manager variables/config;
- desktop entries;
- application manager mappings;
- portal launch paths;
- test fixtures;
- Bench/development launchers.

Do not hardcode `/home/agony` or a repository checkout into the final native launch path.

## Rust Backend Build

Keep the Rust crate independently buildable and testable.

The native Explorer package should either:

- install `explorer_backend` beside/under a deterministic Astrea libexec path; or
- discover it through an explicit configured libexec path.

Do not rely on the current source-tree-relative path once the application is packaged.

## Runtime Data

Do not store mutable user data in the application installation directory.

Preserve existing compatible locations unless intentionally migrated:

```text
~/.config/...
~/.local/state/Astrea/...
~/.local/share/Trash/...
~/.cache/...
```
