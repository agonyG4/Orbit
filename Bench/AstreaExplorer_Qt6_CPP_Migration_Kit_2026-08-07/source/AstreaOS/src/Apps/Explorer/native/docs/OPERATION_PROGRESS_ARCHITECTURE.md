# Explorer operation progress contract

Astrea Explorer publishes file and archive operation updates as aggregate
state snapshots. The native `AppStateFacade` remains the source of truth and
emits `fileOperationStateChanged()` or `archiveStateChanged()` after the
corresponding aggregate state has been updated.

`NativeAppStateAdapter.qml` listens to those native aggregate signals and
captures a snapshot directly from the facade in the signal handler. It emits
the public `fileOperationChanged(snapshot)` and
`archiveOperationChanged(snapshot)` events. `FileOperationsState.qml` is the
public Explorer boundary: it forwards those events and exposes current
snapshot functions for initial presenter synchronisation.

## Archive ownership

The native `AppStateFacade` owns one archive slot. `startArchiveExtraction()`
and `startFolderCompression()` are top-level archive submissions and are
single-flight: while that slot is running, either new submission is rejected
without changing the active request, its metadata, or its published snapshot.
File copy and move work is owned by `FileOperationsController` and may run at
the same time as the one archive operation.

The QML context menu mirrors the native availability state to disable Extract
and Compress actions, but that is only a UX guard. Native admission remains
authoritative for keyboard, automation, or stale-menu callers. A terminal
archive result releases the native slot before its aggregate signal is
published, so a new archive may start immediately even while the presenter is
still displaying the previous terminal card.

Password and conflict submissions are continuation APIs for the currently
owned archive request. They are not competing top-level submissions. No new
password/conflict protocol is introduced by this ownership rule; if the
backend does not publish a prompt, those dormant prompt paths remain dormant.

The numeric progress fields are part of the live snapshot. A semantic state
such as `running` is not itself a progress event; every native progress update
must therefore publish the aggregate event even when the semantic state does
not change.

`OperationProgressPresenter.qml` consumes only `AppState.fileOps`. It keeps
the latest file and archive snapshots, gives archive work display priority,
and resumes a hidden file operation from its latest snapshot when archive
work ends. It owns presentation lifetime only: terminal data is held pending
until `minimumRunningDisplayMs` has elapsed, then displayed during the
terminal hold and fade phases. That minimum interval is a presentation rule,
not a mutation of native operation state.

Each new live operation increments the presenter generation and invalidates
pending minimum, hold, and fade callbacks. A fast terminal event stores its
terminal snapshot without changing the visible running title, progress, or
`terminal` flag. This prevents a transient contradictory card while keeping
native completion immediate and truthful.

The presenter is not a scheduler and does not own admission, cancellation, or
queueing. Its terminal-card lifetime is independent from the native archive
slot lifetime.

Archive progress remains intentionally indeterminate because the archive
backend publishes terminal lifecycle events rather than a streaming progress
protocol. Unrelated archive aggregate events do not create a card when no
archive operation is active.
