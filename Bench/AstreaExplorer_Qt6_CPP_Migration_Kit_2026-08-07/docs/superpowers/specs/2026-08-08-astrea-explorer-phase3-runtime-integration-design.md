# Astrea Explorer Phase 3 Native Runtime Integration Design

## Goal

Connect the existing Explorer QML interface to the native Qt runtime while
preserving the current QML visual tree, Borealis theme, translations, asset
resolution, Rust backend contract, and legacy launcher fallback.

The native executable will load the existing
`source/AstreaOS/src/Apps/Explorer/Main.qml`. It will not introduce a second
visual entry point.

## Resolved runtime root

The native runtime uses a testable `ExplorerRuntimeResolver` to select the
directory containing the Astrea runtime directories (`Apps`, `Core`,
`Features`, `System`, and `Quickshell`). A candidate is accepted only when it
contains all required Explorer/QML structure:

- `Apps/Explorer/Main.qml`;
- `Apps/Explorer/qmldir`;
- `Apps/Explorer/Theme.qml`;
- `Core/components/Theme.qml`;
- `Features/Files`;
- `System/i18n/I18n.qml`.

Resolution is strict and ordered:

1. If `ASTREA_ROOT` is set in the process environment, its exact value is the
   only candidate. An empty or invalid explicit value is an error and never
   falls through to another candidate.
2. Otherwise, test `QDir::homePath() + "/.local/share/Astrea"`. An existing
   but incomplete installed tree is skipped with a diagnostic.
3. Otherwise, walk the native executable directory and its parents. The first
   valid directory is the development checkout runtime root. This supports a
   build directory below `src/Apps/Explorer/native` without embedding a
   developer home directory or repository path in source code.

The resolver returns normalized paths for the Explorer entry point, QML import
roots, the Rust backend, the Explorer helper, the launcher, and the Windows
runner. The application passes the resolved backend path to
`OneShotCliTransport` and sets `ASTREA_ROOT` only when it was not already set.
This keeps QML, services, backend discovery, and launcher construction on the
same root without overriding an explicit environment value.

## Application loading

`ExplorerApplication` remains the owner of one `QGuiApplication`, one
`QQmlApplicationEngine`, and the long-lived native graph. It registers the
native `AppState` facade and loads the resolved local QML URL with
`QQmlApplicationEngine::load()`.

The engine receives the resolved runtime import roots before loading QML. QML
warnings are captured during startup so the self-test and integration test can
fail on missing imports, missing properties, component creation errors, or
other runtime warnings. The self-test checks that the real Explorer root is
created; the old `NativeBootstrap` module remains available through an
explicit bootstrap test path for fallback diagnostics and is not the default
runtime.

The installed and legacy launchers are not changed in this phase. The native
binary's explicit bootstrap path is a development fallback only; it does not
silently replace an invalid explicit `ASTREA_ROOT`.

## Native ownership graph

The application constructs and owns the graph below:

```text
ExplorerApplication
  ├─ OneShotCliTransport
  ├─ RustBackendClient
  ├─ DirectoryModel
  ├─ DirectoryWatchService
  ├─ SettingsService
  ├─ ClipboardService
  ├─ LaunchService
  ├─ FileOperationService
  ├─ NavigationController ─┐
  ├─ SelectionController  ├─ AppStateFacade ── QML
  ├─ FileOperationsController
  ├─ DeviceController
  ├─ PreviewController
  ├─ RecentController
  ├─ OpenWithController
  └─ PortalController
```

`DirectoryModel`, navigation, selection, and settings are wired first because
they are required by the existing Explorer surface. The facade exposes only
compatibility properties and invokables that current QML actually reads or
calls. It delegates behavior to focused controllers and services rather than
recreating the legacy monolithic `AppState` object.

The existing model role names remain unchanged. Model reset notifications
increment the facade revision, navigation owns generation-safe directory
requests, selection reconciles by stable file path, and directory watch events
refresh the active navigation request.

## QML compatibility boundary

The existing QML files remain the visual and interaction layer. Runtime wiring
will not change geometry, colors, typography, delegate structure, animations,
or asset names. The native facade supplies the current path, history, tabs,
directory model, loading/error state, search state, selection state, and
settings properties already represented in the C++ layer.

QML state that is still transitional is intentionally retained until a native
owner has behavior parity. The migration inventory classifies each occurrence
of `Process {}`, Quickshell I/O, filesystem commands, backend calls, duplicated
state machines, and synchronization timers as:

- native owner already available and wired;
- native owner available but not yet exposed;
- legacy fallback retained for parity;
- shared infrastructure outside this phase.

The inventory is documentation and a migration boundary, not permission to
delete legacy QML. `AppState.qml`, `PortalDialog.qml`, `explorer_helper.py`,
and the current launch path remain available while parity is incomplete.

## Error and fallback behavior

- Explicit `ASTREA_ROOT` invalid: report the validation errors and stop native
  startup; do not use installed or development candidates.
- Installed root invalid: record the reason and continue to development
  discovery.
- No valid root: report every candidate and stop before constructing a QML
  engine with an unrelated path.
- Backend process failure: propagate the typed error through
  `NavigationController` and `AppStateFacade::loadError`.
- QML import/property/component failure: capture and report the QML errors;
  the native self-test returns nonzero.
- Legacy Quickshell launcher and portal behavior: unchanged and available as
  fallback until the native parity gate is passed.

## Test design

Tests are added before each production behavior change. The focused coverage
will include:

1. resolver precedence, explicit-root strictness, invalid installed fallback,
   development ancestor discovery, and required-structure validation;
2. native application startup loading the real `Main.qml` with no QML errors;
3. required facade properties and invokables visible to QML;
4. directory model reset and role visibility through the facade;
5. navigation/history propagation and generation-safe refresh;
6. selection preservation across model replacement;
7. backend failure propagation to the facade error property;
8. native and legacy structural QML tests, plus `qmllint` when available;
9. Debug and Release builds and the existing C++/Qt, Python, and Rust suites
   when their toolchains are available.

Unavailable toolchains or real-session visual qualification are reported as
`NOT RUN`; they are never treated as passing by inference.

## Scope and commit boundary

Only files under this migration kit's Explorer native/runtime integration
paths and its Phase 3 documentation are in scope. Unrelated staged changes in
the `/home/agony/GitHub/Orbit` root are preserved and are not staged or
committed. The implementation commit message is:

```text
feat(explorer): connect native Qt runtime to Explorer UI
```
