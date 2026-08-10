# Astrea Explorer Qt 6 / C++ Migration Kit

This package is a reviewed migration kit for replacing Astrea Explorer's Quickshell-owned application runtime with a native Qt 6/C++ application while preserving the current visual design and the existing Rust backend.

## Start Here

Read in this order:

1. `docs/00_EXECUTIVE_DECISION.md`
2. `docs/01_CURRENT_STATE_AUDIT.md`
3. `docs/05_TARGET_QT6_CPP_ARCHITECTURE.md`
4. `docs/06_VISUAL_PARITY_CONTRACT.md`
5. `docs/07_MIGRATION_ROADMAP.md`
6. `docs/04_PYTHON_HELPER_DECOMPOSITION.md`
7. `docs/09_FILECHOOSER_PORTAL_MIGRATION.md`
8. `docs/11_TEST_AND_QUALIFICATION_PLAN.md`
9. `docs/14_IMPLEMENTATION_PROMPT.md`

## Included Source Material

`source/AstreaOS/` contains the Explorer-related source subset extracted from the provided AstreaOS stable archive while preserving the original repository-relative structure and symlinks.

Included material covers:

- Explorer QML/state/helper/tests;
- Rust Explorer backend source and tracked binary;
- `Features/Files` shared file UI;
- `Core/components` shared components;
- `System/i18n`;
- Quickshell shared components currently referenced by Explorer;
- FileChooser portal source;
- directly referenced launcher/wallpaper/state helpers;
- relevant session config and repository test script.

`source/original/Explorer.zip` preserves the standalone Explorer archive supplied separately.

## Important Scope

The kit does not modify the original repositories. It is an analysis, design, migration plan, source reference snapshot, and implementation prompt.

The final design keeps:

- Rust for the existing heavy backend;
- C++/Qt for state, models, process ownership, and orchestration;
- QML/Qt Quick for presentation.

It does **not** recommend rewriting the visual QML in manual C++.
