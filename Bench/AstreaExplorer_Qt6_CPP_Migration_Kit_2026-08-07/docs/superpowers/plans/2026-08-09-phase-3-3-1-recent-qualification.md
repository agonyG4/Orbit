# Phase 3.3.1 Recent Qualification Plan

1. Extend `tst_recent_controller` with desktop-ID fixtures, mixed valid and
   invalid history, deterministic tie/order checks, access-timestamp checks,
   DirectoryModel role projection, launch command qualification, and a bounded
   repeated-refresh/access stress scenario.
2. Add a QML compatibility qualification test with a controllable
   `Quickshell.Io.Process` double. Exercise production `RecentState.qml` save
   and load generation guards, out-of-order completion, stale refresh results,
   and asynchronous event-loop propagation through public `AppState.qml`.
3. Extend the existing AppState compatibility coverage for native-to-QML and
   QML-to-native Recent/navigation/selection/error observations without adding
   another AppState registration or context property.
4. Update `native/docs/APPSTATE_COMPATIBILITY.md` with the qualified ownership
   matrix and explicit future candidates. Do not migrate unrelated Process
   domains.
5. Configure fresh Debug and Release builds, run all tests and linting, inspect
   generated artifacts and the staged diff, then create the exact focused
   commit requested by the phase.
