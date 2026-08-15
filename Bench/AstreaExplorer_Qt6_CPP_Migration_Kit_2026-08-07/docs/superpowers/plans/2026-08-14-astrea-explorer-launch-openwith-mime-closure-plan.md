# Astrea Explorer Launch, Open With, and MIME/XDG Correctness Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the remaining Astrea Explorer launch, Open With, desktop-entry, MIME association, and XDG correctness gaps with one shared desktop-application catalog and a tested end-to-end target-forwarding contract.

**Architecture:** Extract desktop-entry discovery and metadata into a reusable catalog service. The catalog owns asynchronous discovery and publishes an immutable snapshot consumed by Open With, MIME resolution, and Recent history. MIME resolution becomes the sole source of effective application/default decisions. The Rust launch provider receives explicit desktop targets, expands desktop-entry field codes against those targets, and preserves the target contract through direct execution and the `gio launch` fallback.

**Tech Stack:** Qt 6/C++, Rust/Cargo, CMake/CTest, QTest, serde, XDG desktop-entry and mimeapps conventions.

## Global Constraints

- Work only in `/home/agony/GitHub/Orbit/Bench/AstreaExplorer_Qt6_CPP_Migration_Kit_2026-08-07`.
- Preserve the unrelated dirty/generated files already present in Orbit and all changes in `/home/agony/GitHub/Typhon`; stage only files changed for this closure.
- Use `/home/agony/.local/bin/rtk` for repository reads, searches, builds, and tests; use `apply_patch` for source edits.
- Keep the existing source-owned Rust launch provider and packaging architecture intact unless a directly required change is necessary.
- Do not broaden the work into portal, Recent architecture, migration, or packaging redesign except where the shared catalog or launch contract directly requires an integration update.
- Use TDD: add a focused failing test or fixture before each production behavior change, then run the smallest relevant test and the full affected suite.

---

## 1. Establish the failing closure contracts

- [x] Add Rust launch-provider tests for `LaunchRequest::Desktop` target serialization/backward compatibility, strict CLI parsing of multiple `--file`/`--url` targets, desktop-entry field-code expansion, invalid field-code rejection, and target-preserving `gio launch` fallback.
- [x] Add C++ tests for `LaunchService::desktopLaunch` preserving desktop ID plus ordered target arguments, shared catalog ID/root precedence, and Open With projection using MIME resolution rather than a second MIME filter.
- [x] Add XDG/mimeapps tests covering every `XDG_CURRENT_DESKTOP` component, generic-versus-desktop-specific precedence, base desktop-entry `MimeType`, added/removed associations, default validity, and atomic `setDefault` behavior.
- [x] Add a launch contract fixture that creates a temporary desktop entry and recorder command, invokes the actual `astrea-launch` CLI with the C++-shaped argument sequence, and asserts the recorder receives the final target argv.
- [x] Run the focused Rust and Qt tests and record the expected failures before implementing production changes.

Primary files:

- `source/AstreaOS/src/System/launch/src/lib.rs`
- `source/AstreaOS/src/System/launch/src/main.rs`
- `source/AstreaOS/src/System/launch/tests/launcher_tests.rs`
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_launch_service.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_open_with_controller.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_mime_apps_service.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

## 2. Build the shared desktop-application catalog

- [x] Create `DesktopApplicationCatalog` under `native/src/services` with canonical desktop-file ID calculation, root precedence, Application/Hidden/Exec validation, Name/Icon/Exec/MimeType metadata, and a snapshot type usable by other services.
- [x] Implement asynchronous discovery with generation-safe publication on the Qt thread; keep filesystem scanning off the GUI thread and make the snapshot immutable after publication.
- [x] Move desktop-entry reading and catalog construction out of `OpenWithController`; retain a thin compatibility resolver only where existing callers need it.
- [x] Update `OpenWithController` to consume the shared catalog snapshot, expose the catalog-backed records, and remove its independent MIME matching/filtering implementation.
- [x] Update `RecentStore` to resolve desktop history through the shared catalog snapshot instead of building a private catalog on demand.
- [x] Wire one catalog instance through `explorer_application.cpp`, `OpenWithController`, `MimeAppsService`, and `RecentStore`; ensure startup and catalog-ready ordering are deterministic.
- [x] Add the catalog source files to the native operations/recent targets and ensure all affected tests link them.

Primary files:

- `source/AstreaOS/src/Apps/Explorer/native/src/services/desktop_application_catalog.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/desktop_application_catalog.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/open_with_controller.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/recent_store.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/recent_store.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/src/explorer_application.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

## 3. Make XDG and MIME resolution authoritative

- [x] Extend `XdgPaths` with ordered, normalized, deduplicated desktop names from all `XDG_CURRENT_DESKTOP` components.
- [x] Represent mimeapps search locations with enough provenance to distinguish desktop-specific files from generic files while preserving the required config/data precedence order.
- [x] Refactor `MimeAppsService` around one effective-resolution algorithm that combines valid catalog entries, desktop-entry `MimeType`, defaults, added associations, and removed associations.
- [x] Ensure defaults are accepted only when the desktop ID is valid and effectively associated with the MIME type; use effective associations as the fallback default order.
- [x] Preserve explicit-file test/override behavior while applying the same resolver semantics.
- [x] Make `setDefault` acquire the lock, reread current contents, update default/added/removed sections consistently, write atomically, and leave unrelated entries intact.
- [x] Make Open With, AppStateFacade, and any default-setting path call this resolver rather than performing independent MIME checks.

Primary files:

- `source/AstreaOS/src/Apps/Explorer/native/src/services/xdg_paths.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/xdg_paths.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/src/controllers/app_state_facade.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_mime_apps_service.cpp`

## 4. Close the Rust desktop-launch protocol and Exec semantics

- [x] Add explicit desktop launch targets for local files and URLs with serde defaults so older daemon requests remain valid.
- [x] Parse the CLI as `--desktop <id>` followed by an ordered sequence of `--file <path>` and `--url <uri>`, rejecting missing values, unknown flags, and trailing arguments.
- [x] Introduce a context-aware desktop-entry command expansion path for `%f`, `%F`, `%u`, `%U`, `%i`, `%c`, `%k`, and `%%`, preserving argv boundaries and rejecting unknown or malformed field codes.
- [x] Resolve desktop IDs and paths using canonical nested IDs and the same XDG application roots used by the catalog; preserve `NoDisplay`/`Hidden` policy consistently.
- [x] Preserve targets in the `gio launch` fallback, direct execution, launch rules, history records, and daemon request/response serialization.
- [x] Update rule matching and request metadata to ignore only the new target payload, not the logical desktop identity.
- [x] Update Rust unit/integration tests and usage/help text for the new protocol.

Primary files:

- `source/AstreaOS/src/System/launch/src/lib.rs`
- `source/AstreaOS/src/System/launch/src/main.rs`
- `source/AstreaOS/src/System/launch/tests/launcher_tests.rs`
- `source/AstreaOS/src/System/launch/Cargo.toml`

## 5. Verify the complete C++ → astrea-launch → desktop-entry → argv path

- [x] Add or update the C++ launch-service test to assert the exact emitted argv for one target and multiple ordered targets.
- [x] Add the actual executable contract test with isolated XDG directories, a temporary desktop file, a recorder executable, and deterministic environment that exercises parser expansion and final child argv.
- [x] Verify both direct execution and the `gio launch` fallback retain the target payload; if the environment lacks a usable Gio launcher, keep a deterministic unit test for the fallback command construction.
- [x] Verify Open With launch, default setting, and Recent desktop-history lookup all use the shared catalog and preserve canonical desktop IDs.

Primary files:

- `source/AstreaOS/src/Apps/Explorer/native/src/services/launch_service.h`
- `source/AstreaOS/src/Apps/Explorer/native/src/services/launch_service.cpp`
- `source/AstreaOS/src/Apps/Explorer/native/tests/tst_launch_service.cpp`
- `source/AstreaOS/src/System/launch/tests/launcher_tests.rs`
- `source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt`

## 6. Qualification and handoff

- [x] Run focused Rust tests, native QTest targets, and the end-to-end contract test after implementation.
- [x] Run Debug and Release builds plus CTest; rerun launch-provider, backend, portal, Python, QML, ZIP, source/extracted clean-install, and migration-gate checks already used by the repository.
- [x] Inspect `git diff --check`, the final diff, and staged paths; confirm pre-existing generated/deleted files are not included.
- [x] Create focused commits for the closure only, preserving the starting `8087d08` history and reporting final commit IDs.
- [x] Report catalog ownership, MIME/XDG ordering, launch protocol/Exec behavior, end-to-end evidence, test counts, and any genuine limitations.
