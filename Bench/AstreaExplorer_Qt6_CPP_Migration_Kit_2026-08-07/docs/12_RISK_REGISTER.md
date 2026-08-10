# Migration Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| Visual drift while touching QML process blocks | High | Visual hash manifest, screenshot baseline, behavior-only edits in visual files. |
| Rewriting mature Rust behavior in C++ | High | Preserve Rust backend; extend it instead of replacing it. |
| Python archive security regression | Critical | Port tests first; migrate archives last; keep rollback/path-validation semantics. |
| Stale async results overwrite newer navigation | High | Explicit request/generation IDs in C++. |
| GUI freezes on filesystem/backend work | High | No blocking backend waits on GUI thread; asynchronous QProcess/worker handling. |
| Persistent backend protocol becomes unbounded | High | Message caps, request bounds, cancellation, generation ownership. |
| Rust worker crash leaves UI stuck | High | Fail pending requests exactly once; restart with new generation. |
| Portal works in one implementation but not the other | High | Update/test both Python and Rust portal launch paths present in snapshot. |
| Settings paths reset after moving app identity to C++ | Medium | Preserve application identity and configuration compatibility. |
| Clipboard behavior changes after removing wl-copy/wl-paste | High | Explicit MIME tests with `text/uri-list` and image bytes. |
| QFileSystemWatcher behaves differently on remote filesystems | High | Preserve current remote-listing policy and disable local watcher where appropriate. |
| AppIcon dependency forces Quickshell back into Explorer | Medium | Move/reuse AppIcon in neutral shared Qt module without visual change. |
| “Cleanup” expands scope into redesign | High | Migration PRs reject styling/layout edits. |
| FFI scope explosion | Medium | Persistent worker first; FFI only after measurement. |
| One huge C++ AppState replaces one huge QML AppState | High | Facade delegates to focused controllers/models. |
| Old helper is removed before full coverage | High | Zero-caller + ported-test gate for every helper command. |
| Existing tracked backend binary diverges from source | Medium | Build backend from source in packaging; record hashes; do not treat tracked ELF as source of truth. |
