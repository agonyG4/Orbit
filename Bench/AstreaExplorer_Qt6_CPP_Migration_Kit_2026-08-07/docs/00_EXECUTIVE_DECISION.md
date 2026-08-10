# Astrea Explorer Native Qt 6 Migration — Executive Decision

## Decision

Migrate Astrea Explorer away from Quickshell and into a native Qt 6 application, while preserving its existing visual design.

The target is **not** a C++-only UI. The correct split is:

```text
C++ / Qt
- application lifecycle
- controllers and state machines
- QAbstractItemModel models
- clipboard and filesystem watchers
- process ownership
- configuration and persistence
- typed communication with the Rust backend

Rust
- existing Explorer backend
- directory listing and recursive search
- devices / mount / unmount / remount
- file operations
- thumbnails
- AppImage installation
- additional filesystem-heavy helper responsibilities migrated from Python

QML / Qt Quick
- existing layout
- existing components
- delegates
- menus and dialogs
- animation
- presentation bindings
```

The visual contract is strict: **no redesign is part of this migration**.

## Why Explorer Is a Good Candidate

Explorer currently uses QML for much more than presentation. The reviewed snapshot contains:

- 23 QML files;
- 37 `Process {}` nodes;
- a 526-line `AppState.qml` facade;
- a 1,085-line `FileOperationsState.qml`;
- a 925-line `PreviewState.qml`;
- a 686-line `NavigationState.qml`;
- a 1,679-line `explorer_helper.py` with 19 CLI subcommands;
- a separate 3,609-line Rust backend.

This produces a multi-layer command path such as:

```text
QML state
 -> Quickshell Process
 -> Python helper or Rust CLI
 -> stdout JSON / JSONL / text
 -> QML parsing
 -> QML ListModel/state
```

The migration should reduce that to:

```text
QML presentation
 -> typed C++ controller/model
 -> Rust backend client when heavy backend work is required
```

## Recommended Backend Integration

Three approaches were evaluated.

### A. Keep one-shot Rust CLI processes forever

Lowest migration risk, but keeps process-spawn and serialization overhead for directory listing, search, device refresh, file operations, and thumbnails.

### B. Native Qt 6 + persistent Rust worker — recommended final architecture

Keep the existing CLI contract during early migration, then add a backward-compatible persistent `serve` mode to `explorer_backend` using bounded request IDs and JSONL or another framed local protocol.

Advantages:

- no C++/Rust ABI coupling;
- Rust backend remains independently testable;
- streaming file-operation progress maps naturally to events;
- request cancellation and generation handling can be explicit;
- substantially fewer process spawns;
- backend crashes do not corrupt the Qt GUI process;
- existing CLI remains useful for tests and diagnostics.

### C. Link Rust directly into C++ through FFI

Potentially the lowest per-request overhead, but not recommended for this migration. It introduces ABI ownership, build-system, panic-boundary, cancellation, and cross-language lifetime complexity without evidence that local IPC is the bottleneck.

Only consider FFI after the native application is complete and measured.

## Final Target

```text
astrea-explorer
  QGuiApplication
  QQmlApplicationEngine
        |
        +-- AppState (C++ singleton facade)
        +-- DirectoryModel (QAbstractListModel)
        +-- NavigationController
        +-- SelectionController
        +-- FileOperationsController
        +-- PreviewController
        +-- DeviceController
        +-- RecentController
        +-- OpenWithController
        +-- PortalController
        +-- RustBackendClient
                 |
                 +-- explorer_backend serve
                       Rust worker
```

The existing visual QML remains the presentation layer.
