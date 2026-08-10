# Rust Explorer Backend Contract

## Preserve the Backend

The Rust backend is already the strongest part of Explorer's non-visual architecture and should remain the owner of expensive/filesystem-heavy work.

Current crate:

```text
src/Core/bridge/apps/explorer
```

Current dependencies are deliberately small:

```text
md5 = "0.7"
rayon = "1.10"
```

## Existing Commands

### `list`

Conceptual shape:

```text
explorer_backend list <path> <show_hidden> <sort_field> <sort_asc> <folders_first> [preview-mode]
```

Returns a JSON array with roles equivalent to:

```text
fileName
filePath
fileUrl
fileIsDir
fileExecutable
fileHidden
fileSize
fileModified
fileKind
filePreviewUrl
fileRemote
fileMetadataLimited
fileFilesystem
```

These role names are part of the visual compatibility contract and should become `QAbstractItemModel` roles rather than being renamed during migration.

### `search`

Performs bounded recursive local search and a cheaper remote-filesystem search path. Existing search safety behavior must be retained.

### `devices`, `mount`, `unmount`, `remount`

Own device discovery and mount operations using the existing Rust logic and external system tooling.

### `warm-thumbnails`

Uses a dedicated Rayon pool capped at four workers and avoids expensive local thumbnail behavior for remote listings.

### `install-appimage`

Installs a selected AppImage into the user's local bin directory and writes a desktop entry using staged publication.

### `file-op`

Owns copy/move/cut semantics, conflict policies, progress events, preservation of timestamps, symlink behavior, and rollback-sensitive overwrite behavior.

It already supports JSONL progress events and is a natural model for a future persistent protocol.

## Recommended Evolution

Do not delete the existing CLI commands.

Add a backward-compatible server mode only after the native Qt application reaches behavior and visual parity:

```text
explorer_backend serve --stdio-jsonl
```

Suggested request envelope:

```json
{"version":1,"id":42,"op":"list","params":{}}
```

Suggested events:

```json
{"version":1,"id":42,"event":"result","ok":true,"data":{}}
{"version":1,"id":42,"event":"progress","data":{}}
{"version":1,"id":42,"event":"error","code":"...","message":"..."}
```

Requirements:

- bounded input line/message size;
- monotonically allocated C++ request IDs with wrap-safe reuse;
- no concurrent response ambiguity;
- explicit cancellation for long search/list/archive/file operations where possible;
- malformed request rejection without process termination;
- one request failure must not poison the worker;
- worker EOF/crash must fail outstanding requests exactly once;
- reconnect must create a new worker generation;
- no shell command strings;
- preserve exact CLI behavior for diagnostics and compatibility.

Adding `serde`/`serde_json` is acceptable if justified by the persistent protocol; do not hand-build a complex new parser merely to avoid dependencies.

## Why Not FFI First

Direct Rust/C++ FFI would couple:

- CMake and Cargo;
- allocation ownership;
- panic boundaries;
- cancellation;
- thread ownership;
- ABI compatibility;
- model lifetime.

A persistent local worker captures almost all of the practical spawn-latency benefit with a much smaller failure boundary. Benchmark before considering FFI.
