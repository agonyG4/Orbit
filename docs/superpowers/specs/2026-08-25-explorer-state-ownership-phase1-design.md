# Orbit Explorer State Ownership Closure — Phase 1 Design

## Goal

Make `AppStateFacade` a stable QML-facing projection while moving Explorer
settings, sidebar favorites, and archive workflow state into dedicated native
owners. Preserve the complete public `AppState` contract and all existing
behavioral, persistence, backend, runtime, installation, and QML compatibility
contracts.

## Current context

The accepted repository reset is complete. This phase operates only inside the
existing `apps/`, `shared/`, `services/`, `old/`, `cmake/`, and `scripts/`
layout. The baseline checkout is clean at the structural-reset closure commit;
source gate, Explorer Python/QML tests, i18n tests and validator, Debug CTest,
and the existing native test suite pass before this phase.

The current facade owns persistent `ExplorerSettings`, sidebar favorite parsing
and transactions, and the complete archive state machine. `DeviceController`
already owns the runtime auto-mount set but its changes are not persisted by
the facade. The compatibility `FileOperationsState.qml` also masks native
archive, AppImage, and wallpaper state with constants.

## Architecture

The public path remains:

```text
AppState.qml
  -> NativeAppStateAdapter.qml
  -> AppStateFacade
```

The native composition becomes:

```text
SettingsService
  -> ExplorerSettingsController
       -> NavigationController
       -> DeviceController
       -> SidebarFavoritesController
  -> ArchiveController <- FilesystemService, NavigationController
  -> AppStateFacadeDependencies
  -> AppStateFacade
```

`AppStateFacade` returns values from owners, forwards setters and invokables,
translates existing service results, and emits the existing public signals. It
does not retain mutable settings, favorite, or archive domain state.

### ExplorerSettingsController

`ExplorerSettingsController` owns the loaded `Services::ExplorerSettings` and
the policy for loading, applying, and persisting it. It preserves the current
`explorer.conf` filename, group, and keys. It applies navigation values once
before subscribing to navigation changes, then persists navigation-originated
changes with equality guards. It coordinates auto-mount persistence by
initializing `DeviceController` and persisting its canonical serialized value
after runtime changes. Favorite JSON is stored and signalled here, while its
semantic interpretation remains in `SidebarFavoritesController`.

The controller is the only owner that writes settings. It never asks QML to
write configuration and does not maintain a second auto-mount runtime set.

### DeviceController auto-mount boundary

`DeviceController` remains the authoritative runtime owner. A safe JSON setter
uses the same canonical array-of-non-empty-strings normalization as startup;
malformed or non-array input becomes an empty canonical set. The settings
controller loads persisted JSON into the device controller and subscribes to
`autoMountChanged()` to persist the controller's canonical value. The facade's
compatibility getter reads the device controller when present, and its setter
delegates to the settings/device boundary.

### SidebarFavoritesController

`SidebarFavoritesController` owns `SidebarFavoritesModel`, default favorite
definitions, normalized path identity, hidden-default interpretation, duplicate
and pin eligibility checks, semantic pin/remove/reorder behavior, and the
existing drag preview/commit/cancel transaction. It persists only by calling
`ExplorerSettingsController`; it never opens or writes configuration itself.

The facade preserves every existing favorite property and invokable by
delegating to this controller. The model remains the same public native model
and QML drag visuals remain in QML.

### ArchiveController

`ArchiveController` owns every existing `m_archive*` value, request identity,
password/conflict continuation, occupancy invariant, and terminal aggregate
state. It accepts `FilesystemService` and `NavigationController` dependencies.
Both top-level start methods reject work while running, password continuation,
or conflict continuation is active. Password/conflict operations remain
continuations of the same workflow.

On a matching terminal result, the controller releases its slot before
publishing its state notification. The explicit completion path lets the facade
forward `filesystemActionFinished(...)` once while preserving the current
re-entrant signal-handler behavior and avoiding request-ID clearing after a
new operation has started.

### Typed facade dependencies

`AppStateFacadeDependencies` replaces the long positional production
constructor. It contains typed pointers for navigation, selection, model,
settings, favorites, archive, existing controllers/services, and runtime paths.
Mandatory navigation, selection, and model dependencies are asserted. Optional
focused-test dependencies retain the current nullable behavior. Concrete
controller/service headers are forward-declared in the facade header where
possible and included in the implementation file.

### QML projection

`NativeAppStateAdapter.qml` remains the native bridge. Native-owned values in
`FileOperationsState.qml` are aliases to the adapter, including archive
password/conflict fields and AppImage/wallpaper running state. QML-only input
state remains QML-owned. End-to-end tests exercise the actual adapter and
`AppState.qml` chain rather than only checking source strings.

## Testing strategy

1. Freeze the public facade surface with explicit Qt meta-object expectations
   for property names/types/writability, invokable signatures, and public
   adapter signals. The expectations are independent of declaration order and
   are not generated from the implementation under test.
2. Add focused controller suites for settings, sidebar favorites, and archive
   behavior. Existing facade tests move with ownership, while facade tests keep
   projection/delegation coverage.
3. Add auto-mount persistence tests for startup load, runtime enable/disable,
   compatibility JSON writes, reload behavior, and malformed input.
4. Add actual QML-chain projection tests for representative archive,
   AppImage, and wallpaper fields.
5. Preserve the complete archive regression set, including occupied-state
   admission, stale continuation contexts, inert cancellation, unsupported
   policies, and re-entrant terminal admission.
6. Run focused tests after each owner extraction, then the existing Python,
   i18n, source, Rust, Debug, Release, and clean-install gates.

## Explicit exclusions

This phase does not remove `AppState.qml`, `NativeAppStateAdapter.qml`,
`LegacyAppStateAdapter.qml`, or `state/*.qml`; rename namespaces; split Rust;
redesign `Main.qml`, Sidebar, Toolbar, or CMake layout; change backend or D-Bus
contracts; change settings format; or change runtime/install paths. Those are
follow-up work.

