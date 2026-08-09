# Phase 3.3: Runtime Capability Safety and Behavioral Parity

## Goal

Close the native compatibility-boundary gaps while keeping `AppState.qml` as the only public QML contract, preserving the existing visual UI, retaining transitional legacy domains, and committing only Explorer native-runtime changes.

## Constraints and invariants

- Preserve all unrelated staged changes in the Orbit repository.
- Do not add new `Process {}` migrations or new public `AppState` registrations.
- `NativeAppState` is registered only under `Astrea.Explorer.Native 1.0`; `AppState.qml` remains the public contract.
- Native ownership is enabled only by a process-local capability backed by the actual registered native facade.
- Keep legacy fallback behavior, QML imports/theme/translations/assets, resolver behavior, launcher integration, and existing UI unchanged.

## Files to map and update

- Runtime/capability: `source/AstreaOS/src/Apps/Explorer/AppState.qml`, `state/NavigationState.qml`, `compatibility/NativeAppStateAdapter.qml`, native `explorer_application.*`, and compatibility tests.
- Recent projection: native `RecentController`, `DirectoryEntry`/`DirectoryModel` roles, Open-With desktop-entry catalog infrastructure, recent controller tests, and minimal icon-role presentation plumbing if required for application entries.
- Recent persistence: legacy `state/RecentState.qml` and its QML/integration tests; use generation/order guards without timers.
- Clipboard parity: native `ClipboardService`, `AppStateFacade` copy/cut methods, and focused Qt tests.
- Hygiene: search all Explorer-tree references to the old marker, inspect/remove the accidental `source/AstreaOS/-I` artifact, correct the intended QML lint invocation, and exclude generated/cache/archive files from lint/packaging.

## Test-first execution

1. Add failing tests for process-local capability selection and inherited-marker fallback, including an asynchronous event-loop/navigation assertion and real-native delegation.
2. Add failing native recent tests for file and desktop projection, access timestamps in `fileModified`, invalid/duplicate/missing records, ordering, stale persistence generations, and no backend dependency.
3. Add failing clipboard tests for empty operations, order-insensitive repeated-cut cancellation, copy/cut transitions, and coherent system clipboard publication/clearing.
4. Run focused tests to capture the red baseline.

## Implementation

1. Replace the environment marker with an application/engine capability set only by the native application after real facade registration. Make the QML adapter and navigation ownership derive from the ready native object, so fallback cannot retain native-only flags or recurse.
2. Reuse the existing desktop-entry/catalog service for launch-history desktop records. Preserve application name, icon, `.desktop` launch identity, metadata, ordering, and graceful invalid-record handling. Expose only the minimal file-icon role needed by existing delegates.
3. Make recent `fileModified` equal the access timestamp while retaining filesystem metadata separately where useful. Add generation-safe in-memory recent projection/persistence ordering and ensure failed saves never roll back visible state.
4. Make native copy/cut no-op for empty selections, compare repeated cuts as sets, clear/correct native clipboard state on cancellation, and keep QML mirror bindings coherent through existing signals.
5. Remove behavioral reads of the old marker, inspect and remove only the accidental lint artifact, and define a lint set that excludes generated/build/archive inputs without suppressing source warnings.

## Verification and handoff

- Build and run clean Debug and Release test trees, including all C++/Qt tests.
- Run Python tests, Rust tests, and intended QML lint; record warning/error counts.
- Run resolver and native/fallback smoke tests, explicitly reporting missing `quickshell-ioplugin` if the real UI cannot load.
- Inspect the final diff and status, stage only package files belonging to this phase, commit exactly `fix(explorer): close native bridge parity gaps`, then report native-authoritative versus legacy/transitional domains and all validation results.
