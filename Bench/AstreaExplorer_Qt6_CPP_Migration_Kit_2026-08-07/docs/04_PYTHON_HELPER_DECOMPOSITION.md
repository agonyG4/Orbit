# Python Helper Decomposition Plan

The goal is to remove `explorer_helper.py` from the production Explorer runtime after behavior parity has been proven.

Do not rewrite all 1,679 lines blindly. Migrate each responsibility to its correct owner.

| Current helper command | Target owner | Migration direction |
|---|---|---|
| `create-folder` | Rust backend | Add typed `mkdir`/create-folder operation with the same child-name safety rules. |
| `rename` | Rust backend | Add exact rename operation or reuse file-op rename semantics without weakening collision safety. |
| `suggest-dirs` | C++ navigation service | Produce bounded completion suggestions asynchronously from current directory state. |
| `which` | C++ | Use `QStandardPaths::findExecutable`. |
| `network-mount-probe` | C++ or Rust device service | Keep network probe away from QML. Prefer the device/network service that already owns the mount state. |
| `copy-uri-list` | C++ / Qt clipboard | Use `QClipboard` + `QMimeData` with `text/uri-list`; remove the `wl-copy` dependency for Explorer clipboard sync. |
| `scan-conflicts` | Rust backend | Conflict planning belongs with file operations and must use the exact same path/symlink semantics. |
| `monitor-dir` | C++ | Replace the helper inotify process with `QFileSystemWatcher` plus bounded debounce/reload logic. |
| `trash` | Rust backend | Preserve freedesktop trash metadata, collision handling, symlink behavior, and tests. |
| `restore-trash` | Rust backend | Preserve `.trashinfo` decoding and fallback behavior. |
| `empty-trash` | Rust backend | Keep destructive operation out of QML. |
| `paste-image` | C++ / Qt clipboard | Read exact MIME bytes from `QMimeData` and publish a unique file name without invoking `wl-paste`. |
| `extract-archive` | Rust backend | Port security checks, password flow, conflict behavior, staged destination/rollback, JSON progress, and tool fallback. |
| `compress-folder` | Rust backend | Port format/tool detection and progress behavior. |
| `create-desktop-shortcut` | C++ shell integration service | Use `QStandardPaths::DesktopLocation` and a safe symlink/copy policy. |
| `merged-recents` | C++ recent service | Parse Finder recents, Astrea launch history, and XBEL into one typed model. |
| `open-with-apps` | Rust backend or dedicated C++ service | Prefer Rust for desktop-entry/MIME discovery to keep parsing and system command semantics outside QML. |
| `launch-open-with` | Rust backend / launch service | Preserve no-shell invocation and exact desktop-file targeting. |
| `set-default-open-with` | Rust backend | Preserve MIME/default-app semantics and explicit error reporting. |

## Migration Rule

A helper command may only be deleted after:

1. the new owner has deterministic tests covering the old behavior;
2. the old QML caller uses the new typed API;
3. the old command has zero production callers;
4. existing helper regression tests have either been ported or explicitly superseded;
5. real-session smoke behavior passes.

## Archive Code Requires Special Care

The Python helper contains meaningful hardening for:

- `..` and absolute-path archive entries;
- password-required detection;
- destination conflicts;
- rollback after overwrite failure;
- merge behavior;
- multiple external archive tools;
- streaming/progress behavior.

Do not replace it with a simplistic `QProcess("unzip")` implementation in C++.

Archive behavior should move to Rust with equivalent or stronger tests.
