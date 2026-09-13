# Explorer Archive 2.0 Safety, Lifecycle, and Performance Closure Design

## Goal

Close the remaining P0/P1 Archive 2.0 gaps without changing the dedicated archive worker, structured JSONL protocol, staged provider execution, capability-driven UI, or existing compatibility aliases.

## Architecture

Extraction requests carry an explicit `destinationMode`: `new-directory` means the destination path is a replaceable child directory selected by “Extract to …”; `into-directory` means the destination is an existing container used by “Extract Here” and “Extract…”. The Rust engine always extracts into private staging first. For `into-directory`, it builds a member-wise publication plan and never removes, renames, or replaces the container itself.

Publication uses a private sibling backup directory for conflicting existing entries. Regular-file replacements and top-level incoming entries are moved into backup before publication; directory conflicts are merged recursively, with only colliding descendants backed up. Failure or cancellation removes newly published entries and restores backups. `keep-both` reserves unique incoming names without mutating unrelated entries. Unsafe directory-merge overwrite is rejected before any destructive step when a safe transaction cannot be represented.

The archive workflow exposes a structured logical state (`running`, `waiting-password`, `waiting-conflict`, `success`, `cancelled`, `failed`) through the controller, facade, and QML operation snapshot. Request-level completion remains internal; `operationFinished` is emitted only for logical terminal states.

7z `-slt` output is parsed into deterministic records. `Encrypted`, `Symbolic Link`, `Hard Link`, and link-relevant attributes are independent fields. Any link representation is rejected before `7z x`, after path validation and before destination publication. Post-extraction tree validation remains defense in depth.

## Provider and performance rules

7z creation uses original source paths whenever one working directory can represent the selected top-level basenames. The normal same-parent selection uses `cwd = common parent` and source basenames, with zero source-clone bytes. A safe common-root path is also used directly when it preserves the requested archive layout. Only cross-parent requests that cannot be represented directly use an `input` fallback. Fallback copying is recursive, chunked, cancellation-aware, reports a preparation phase, and removes partial input on every exit path.

Provider capabilities are directional: create provider, extract provider, create password support, extract password support, and profile/threads are reported from actual plans. ZIP prefers 7z when available because 7z levels are meaningful; bsdtar ZIP has no selectable profiles. XZ and Zstandard thread options are added only when the provider probe confirms the option, and the resulting argument plan is tested.

The canonical archive-stem helper matches compound suffixes longest-first for all supported aliases. It is shared by native archive planning and the QML-facing name/action logic. User-entered compression names are validated as a single filename and cannot contain separators, `..`, or an absolute path.

Archive passwords are removed from the JSON argument passed to the operation child. The persistent worker protocol gains a bounded optional stdin payload; the worker writes the payload to the child pipe and closes it. The archive operation consumes the secret from stdin. The external 7z provider may still receive `-p<password>` in its own argv because that provider interface has no safer channel; this boundary is documented and the secret is never logged or placed in the Astrea operation argv.

## Testing and qualification

Rust unit and integration tests cover destination modes, every visible conflict policy, preserved unrelated files, atomic file replacement, recursive directory merge, rollback/cancellation, canonical suffix aliases, 7z record fixtures and pre-extraction link rejection, direct same-parent 7z planning, cancellable fallback preparation, provider/profile/thread argument truthfulness, capability matrices, and archive-name validation. C++ tests cover transport payloads, controller continuation lifecycle, cancellation state, directional capabilities, and presenter snapshots. QML source/runtime tests prove the three extraction actions call distinct semantics.

Verification reuses `build/debug` and includes focused tests before the complete CTest, Rust, QML, source-gate, and `git diff --check` runs. Real-provider tests are capability-gated and report unavailable executables as skipped. Performance qualification records same-parent temporary clone bytes (required: zero), TAR.ZST threading behavior, cancellation latency and residual staging bytes; unavailable providers are not claimed as passed.

## Non-goals

- Do not create a worktree or alternate build directory.
- Do not restore the generic Utility archive path.
- Do not change Preview Pipeline, icon, drag MIME, or unrelated file-operation architecture.
- Do not make a user-selected extraction container replaceable.
- Do not expose unsafe recursive directory replacement merely to retain a conflict option.
