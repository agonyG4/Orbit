# Source Origins

## Provided Archives

### Explorer.zip

SHA-256:

```text
a8546b41213cac339e266fd920a52cc61fab976c45ba0b2ec30374a8db9c7c77
```

### AstreaOS-Stable(1).zip

SHA-256:

```text
f4ef3561e19df4c6d5d818d79921396198d4bc575e35b4da838af1ad82ee0d2b
```

## Snapshot Comparison

The 29 regular files present in both the standalone Explorer archive and the AstreaOS `src/Apps/Explorer` subtree are byte-identical.

The AstreaOS subtree additionally contains tracked symlinks to shared modules:

```text
AstreaComponents -> ../../Core/components
AstreaFiles -> ../../Features/Files
AstreaI18n -> ../../System/i18n
QuickshellComponents -> ../../Quickshell/components
```

## Validation Performed While Building This Kit

The Explorer Python unit/structural suites ran successfully after the tracked symlinks were reconstructed against the provided AstreaOS dependency snapshot:

```text
Ran 69 tests
OK
```

Rust tests were not executed in this artifact environment because `rustc` and `cargo` are not installed. The source includes the Rust tests and the implementation prompt requires them to pass in the real development environment before migration changes.
