# Astrea Explorer Production Packaging and Clean-Install Closure

## Goal

Make the existing native Qt6 Explorer install reproducible and self-contained, while closing runtime, MIME association, portal lifecycle, worker-startup, and clean-install qualification gaps without changing the Explorer UI or migrating additional QML domains.

## Decisions

- Preserve the tracked `source/AstreaOS/src/bin/astrea-launch` executable as the existing launch provider. There is no `System/launch` source crate in the checkout, so packaging will fail if this required provider is absent and will install the tracked executable into the same runtime `bin/` directory used by the resolver.
- Make CMake own the Rust build closure. The Explorer backend and portal Cargo manifests are built through deterministic custom commands/targets, selecting Cargo debug or release output from the native configuration. Install depends on both artifacts and fails if either is missing.
- Keep the canonical runtime root at `<prefix>/share/Astrea` (or `ASTREA_ROOT`), with `bin/astrea-explorer`, `bin/astrea-launch`, `bin/astrea-filechooser-portal`, and `Core/bridge/apps/explorer_backend` as required production artifacts.
- Replace the current single `valid` runtime flag with resource validity plus capability flags and actionable diagnostics. Normal mode requires QML resources, the backend, and the launch provider; portal mode does not require unrelated optional Windows support.
- Add a focused `DesktopFileId`/`DesktopCatalog` implementation that derives IDs relative to each XDG applications root, applies user-before-system precedence, and shares identity between catalog, launch history, Open With, and MIME associations.
- Add `MimeAppsService` with a small freedesktop INI parser/writer, semicolon-list handling, XDG config resolution, `QLockFile`, `QSaveFile`, re-read-before-write behavior, and preservation of unrelated sections/keys. `QSettings` remains available for ordinary Astrea settings but is removed from `mimeapps.list` handling.
- Refactor the persistent worker into an event-driven state machine. Requests are queued while `QProcess` starts; `QTimer` owns startup/request timeouts; every queued request receives exactly one terminal signal; shutdown reaps the child safely.
- Rebuild the Rust portal around per-handle request owners. Each request exports `org.freedesktop.impl.portal.Request` at the supplied object path, owns an async dialog child, uses cancellation-aware `tokio::select!`, bounds diagnostics/result data, and removes the request object on every terminal path. Multiple handles are independent and subject to an explicit active-request limit.
- Add semantic tests and a clean-install harness using temporary HOME/XDG directories and a temporary install prefix. The migration gate is extended for source/install invariants, while the harness verifies installed normal and portal self-tests, MIME round trips, and portal cancellation.

## Components and data flow

```text
CMake configure
    |
    +--> Cargo build explorer_backend ------------------+
    +--> Cargo build astrea-filechooser-portal ---------+--> install prefix
    +--> install tracked astrea-launch ------------------+
    +--> build astrea-explorer --------------------------+

ExplorerRuntimeResolver
    +--> resourceRootValid
    +--> backendAvailable
    +--> launchAvailable
    +--> optional capability diagnostics

OpenWithController --> DesktopCatalog --> MimeAppsService --> atomic mimeapps.list
                   \-> LaunchService --> astrea-launch argv

Portal FileChooser(handle)
    --> PortalRequest(handle)
    --> async DialogRunner
        +--> result
        +--> Close()/timeout/crash
```

The current QML/native ownership model remains unchanged. No visual redesign, Quickshell dependency, or new QML migration is in scope.

## Error and lifecycle rules

- A required source artifact or Cargo build failure stops production packaging; stale binaries are never accepted as a substitute.
- A valid resource root is not reported as a ready production runtime when mandatory executables are missing.
- `LaunchService` never returns a valid spec with an empty or missing required program, and launch failures carry typed/actionable errors.
- MIME updates preserve unrelated content and never clobber a concurrent writer after acquiring the lock.
- Portal `Close()` is idempotent and wins over late dialog completion. The child is terminated and reaped, the result is discarded, and the request object is released exactly once.
- Portal stdout, stderr, and result files have documented byte limits; exceeding a limit produces a bounded failure.

## Verification

The implementation will add focused C++/Qt and Rust tests for runtime capabilities, desktop IDs/XDG precedence, MIME parsing and atomic round trips, worker startup/failure/cancellation, and portal request lifecycle/concurrency/bounds. It will also run clean Debug and Release builds, all relevant CTest/Python/QML/Rust suites, the native migration gate, and an isolated clean-install qualification with no developer Astrea root or Quickshell available.

