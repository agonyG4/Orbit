# Phase 3.3.2 Recent Parity and Production Sequencing Design

## Scope

This is a corrective qualification pass over the existing Recent subsystem.
It does not change the AppState compatibility boundary, migrate additional
domains, redesign QML, or replace the transitional Recent persistence helper.

The public UI contract remains `AppState.qml`. Native navigation and Recent
projection continue to be authoritative in C++, while Recent recording and
persistence remain transitional QML behavior until a later migration phase.

## Compatibility decisions

### Launch-history timestamps

The established Python behavior is the compatibility reference. A valid file
or desktop target with a missing, malformed, or zero timestamp is retained and
uses the target's filesystem modification time. A positive numeric timestamp is
preserved. Invalid identity, missing targets for launch records, unsupported
record kinds, and malformed JSON remain filtered.

The native loader will therefore distinguish timestamp parsing from record
validation instead of treating a failed timestamp parse as a failed record.
Desktop records use the same fallback through the resolved `.desktop` file.

### Finder missing targets

The legacy Finder loader parses valid object records without stat-ing the target,
so it intentionally preserves stale Finder entries. Native behavior will match
that contract for records with a non-empty, usable `filePath`, retaining the
serialized metadata when filesystem metadata is unavailable. Empty identity or
malformed object structure remains filtered. Launch-history and XBEL records
continue to require an existing target because those legacy loaders resolve
metadata from the filesystem.

## Production persistence qualification

Tests will load the real `RecentState.qml` with only a controllable
`Quickshell.Io.Process` substitute. They will call public `recordAccess()` and
complete the production save/load signals; they will not call `startSave()`, set
`items` to manufacture saves, or rewrite generation properties.

The test double records commands, prevents a second save while one is running,
and exposes deterministic completion. The assertions cover:

- A followed by B: one active save, automatic B scheduling after A, and B as
  the final payload;
- A followed by B followed by C: the newest C snapshot is scheduled without a
  stale intermediate rollback;
- save failure with a pending newer generation;
- stale load completion after a public record operation;
- `recordAccess -> save completion -> refreshCurrentFolder` while the current
  path is `recent://`;
- asynchronous post-load event-loop completion and refresh counts.

## Bounded source loading

`RecentController::load()` remains synchronous because the native recent://
projection already has a bounded public result and no existing Recent worker
request abstraction. The launch-history reader will mirror the legacy limit:
it reads newest-first, deduplicates accepted paths, and stops after the source
limit. Desktop resolution will be cached for the duration of one load so
repeated IDs do not rescan application directories. Finder/XBEL merge ordering,
newest-record selection, and final projection ordering remain unchanged.

Stress coverage will use the production `load()` entry point with a large mixed
history containing valid, duplicate, malformed, missing, and desktop records.
It will assert deterministic bounded output and metadata correctness rather
than rely on machine-dependent timing thresholds.

## Test and validation boundary

Existing desktop qualification, DirectoryModel roles, bridge/navigation tests,
Python helper tests, and Rust tests remain part of the regression suite. The
QML lint warning count will be recorded before and after using the same file
scope; existing warnings are not globally fixed or suppressed. Clean Debug and
Release builds, `git diff --check`, and offscreen native startup smoke remain
required. Missing `quickshell-ioplugin` is reported as a visual-runtime
blocker, not hidden as a test success.
