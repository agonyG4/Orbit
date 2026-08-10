# FileChooser Portal Migration

The FileChooser portal is part of Explorer's public integration surface and must migrate with the application.

## Current Contract

Astrea portal code launches `PortalDialog.qml` and passes options/result paths through environment variables.

Inputs:

```text
ASTREA_FILE_DIALOG_OPTIONS
ASTREA_FILE_DIALOG_RESULT_FILE
```

Compatibility inputs:

```text
BENCH_FILE_DIALOG_OPTIONS
BENCH_FILE_DIALOG_RESULT_FILE
```

Output prefixes:

```text
__ASTREA_FILE_DIALOG__
__BENCH_FILE_DIALOG__
```

Modes include:

```text
open_file
save_file
select_folder
```

with multiple selection where requested.

## Target

```text
System portal backend
 -> astrea-explorer --portal
 -> PortalController (C++)
 -> existing FileDialog QML
```

## Requirements

- preserve the environment contract during migration;
- preserve compatibility variables until a separately planned removal;
- parse options in C++ with bounded JSON input;
- write result files atomically (`QSaveFile` or equivalent);
- emit the same stdout result prefix when the file contract is not used;
- ensure cancellation emits exactly one rejected result;
- do not call `Qt.quit()` from arbitrary presentation objects after the controller has already finalized a result;
- prevent duplicate result delivery on close + explicit accept/reject races;
- portal mode must not mutate normal Explorer persisted navigation state unless current behavior intentionally does so;
- test open, save, select-folder, multiple-selection, cancel, malformed options, and dead portal consumer.

## Integration Changes

Update both current portal implementations present in the AstreaOS snapshot where applicable:

```text
src/System/portal/astrea_filechooser_portal.py
src/System/portal/src/lib.rs
```

Do not leave one implementation launching Quickshell while the other launches the native binary.
