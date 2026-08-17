# Explorer Reliability Manual Checklist

- Copy file and folder.
- Cut file and folder.
- Paste with conflict: replace.
- Paste with conflict: keep both.
- Paste with conflict: merge folder.
- Paste with conflict: skip.
- Move to trash (single and multiple items).
- Restore from trash (normal, missing original directory, destination conflict).
- Empty trash.
- Paste image from clipboard (PNG/JPG, collision naming).
- Extract archive (zip, tar.gz; rar/7z when tools available).
- Compress folder (zip, tar, tar.gz, tar.xz; rar when tool available).
- Verify folder refreshes after each operation.
- Verify extracted folder is auto-selected/revealed.

## Destructive-workflow contract

- Transfer requests are explicitly classified as copy or move. A drag/drop request does not rewrite clipboard state.
- Transfer preflight validates every source before mutation, rejects descendant targets, collapses redundant descendant selections, and reports self-drops as no-ops.
- Each transfer emits terminal per-item results. A batch can finish as `success`, `partial-success`, `failed`, or `cancelled`; failed and not-attempted items retain structured error codes and messages.
- Selection identity is the normalized full path, including duplicate names, recent entries, and virtual Trash entries. Successful destructive results are removed from selection; failed items remain selected.
- Backend workers stream progress before terminal completion. Cancellation creates a per-request marker so the filesystem operation can clean up cooperatively, with a bounded process-kill fallback.
- Trash uses `$XDG_DATA_HOME/Trash` for the home filesystem and selects a fresh owning-filesystem Freedesktop location for secondary local mounts. Metadata is reserved before the object move, stores encoded filesystem path bytes and local deletion time, and is removed only after successful restore or permanent deletion.
- `trash://` is an aggregate virtual view. Entries retain their owning trash location and metadata path, so restore and permanent delete work per item; orphaned objects remain visible with an explicit orphan state.
- Empty Trash counts logical file/metadata pairs and reports per-item deletion results. Restore conflicts are explicit failures; permanent deletion inside Trash does not require metadata.
- Sidebar Favorites are one ordered path-authoritative list. Defaults come from XDG user directories with English fallbacks, duplicate physical paths are removed, and drag/drop reorder persists the order.

## Focused qualification

The native regression targets cover transfer planning, explicit drag/drop ownership, conflict preflight, cut-clipboard reconciliation, path-based selection, terminal item results, live backend progress, aggregate Trash metadata, secondary-filesystem Trash, restore conflicts, and favorite ordering. The Rust backend is qualified with `cargo fmt --all -- --check` and `cargo test --manifest-path Cargo.toml`.
