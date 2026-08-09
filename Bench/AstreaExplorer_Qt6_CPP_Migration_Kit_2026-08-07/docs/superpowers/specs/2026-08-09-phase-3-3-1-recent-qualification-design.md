# Phase 3.3.1 Recent Qualification Design

## Scope

This phase qualifies the existing native bridge. It does not introduce another
state owner, migrate additional `Process {}` domains, or change the Explorer
visual layer.

The public boundary remains `AppState.qml`; native-authoritative domains are
observed through `NativeAppState`, while transitional domains continue through
their existing QML implementations.

## Qualification seams

1. `RecentController` tests will use temporary files, desktop-entry fixtures,
   and mixed launch-history records. The resulting `DirectoryEntry` values will
   be applied to the existing `DirectoryModel` so desktop metadata, timestamps,
   ordering, duplicate handling, and QML role names are checked together.
2. Existing `OpenWithController::resolveDesktopEntry` and
   `LaunchService::desktopLaunch` will be the only desktop parser/launcher
   paths under test. The test asserts the launcher command contract without
   launching an external application.
3. A controlled `Quickshell.Io.Process` test double will be used only by QML
   compatibility tests. It will record commands and expose explicit completion
   control, allowing save/load generations and stale refresh completions to be
   exercised without sleeps, timers, or retry loops. Production `RecentState.qml`
   remains the implementation under test.
4. Compatibility tests will drive actions through `AppState.qml` and observe
   the same public properties used by Explorer UI. Native controller/model
   updates and QML requests will both be covered, including an event-loop
   post-load update.
5. Documentation will record the current native-authoritative, transitional,
   and future-candidate domains without changing ownership.

## Persistence ordering contract

Each save/load request carries the current monotonic generation. A completion
may update the public model only if it is current. A stale save completion may
trigger the latest snapshot to be written, but it must never write an older
snapshot or replace newer in-memory items. A stale load completion is ignored.
The controlled process double makes completion order explicit and deterministic.

## Validation

Use fresh Debug and Release build directories, run all registered C++/Qt tests,
Python tests, Rust tests, QML lint, `git diff --check`, and offscreen startup
smoke checks. If the real runtime cannot render because the external
`quickshell-ioplugin` is unavailable, report that separately from automated
qualification.
