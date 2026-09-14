# Async Directory Metrics for Explorer Properties

## Decision

Explorer Properties will request recursive directory metrics on demand through a
dedicated backend process lane. Ordinary directory listing, previews, and the
interactive backend worker will not calculate recursive folder sizes.

The operation is a typed protocol at the native boundary:

```text
DirectoryMetricsRequest { paths: string[] }
DirectoryMetricsProgress {
    state: running,
    bytes, fileCount, directoryCount, unreadableCount, scannedEntryCount
}
DirectoryMetricsResult {
    state: success | partial | cancelled | failed,
    bytes, fileCount, directoryCount, unreadableCount, scannedEntryCount,
    errorCode, errorMessage
}
```

The Rust backend emits bounded/coalesced progress records followed by one
terminal record. The native `RustBackendClient` decodes these records and
`DirectoryMetricsService` owns one active workflow, filters late signals by
request id, and translates cancellation into a structured terminal state.

`ExplorerApplication` will create a second `PersistentWorkerTransport` and
`RustBackendClient` pair for this operation. Its request timeout is disabled;
the existing cancellation marker remains the stop mechanism. This worker is
independent from both the interactive and archive workers.

## Traversal semantics

- Size is logical/apparent content size. Regular entries contribute metadata
  length; directories contribute zero bytes; file contents are never read.
- Traversal uses `symlink_metadata` and never follows symbolic links. A symlink
  is counted as one non-directory entry using its own metadata length.
- Unix directory identities use device/inode pairs to prevent repeated
  traversal. This also prevents an overlapping root such as `A` and
  `A/subdirectory` from double-counting the subtree. The identity set lives only
  for the active request.
- Entries that disappear or cannot be inspected increment `unreadableCount`
  and make the result `partial`, while traversal continues. An unavailable
  root is a hard `failed` result. Unsupported remote/virtual roots return a
  structured partial/unsupported result without attempting an unbounded walk.
- Byte accumulation is checked and produces a structured failure on overflow.
- The traversal stack and visited identities are bounded by active directory
  topology; it never collects all file paths before counting.

## Native and QML lifecycle

`AppStateFacade` exposes the small state surface needed by Properties:

```text
directoryMetricsRequestId
directoryMetricsRunning
directoryMetricsState
directoryMetricsBytes
directoryMetricsFileCount
directoryMetricsDirectoryCount
directoryMetricsUnreadableCount
directoryMetricsScannedEntryCount
directoryMetricsError
requestDirectoryMetrics(paths)
cancelDirectoryMetrics(requestId)
```

Both FileContextMenu Properties and Sidebar Properties call this same API.
Basic Properties metadata remains a fast utility request. A directory shows a
localized calculating state until metrics progress or completion supplies a
known size. Partial results are rendered as “at least” and retain a concise
warning. Closing Properties cancels its request; switching targets invalidates
the old request before starting the new one. Native and QML request identity
checks prevent a late result for target A from changing target B.

Single regular files keep their immediate metadata path and do not start a
recursive scan. Multi-selection passes every selected root to the metrics
request and displays aggregate size and recursive counts. FileListView and the
Preview Panel retain unknown directory sizes.

## Verification strategy

Rust unit tests cover traversal, symlink/cycle policy, overlap, races through a
deterministic fault seam, cancellation, checked accumulation, and progress
coalescing. Native tests cover request encoding, decoding, service lifecycle,
stale-result filtering, cancellation, and two-worker responsiveness. QML/source
tests cover both Properties surfaces, multi-selection, partial display, close
cancellation, and the invariant that listing does not request metrics.
