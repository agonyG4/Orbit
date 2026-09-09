# Explorer File Visual Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve every local symlink in Explorer listings and add bounded asynchronous GIO-derived icon/emblem metadata without delaying navigation or changing Orbit's existing Freedesktop resolver policy.

**Architecture:** Rust keeps the initial `std::fs` listing cheap, records link identity without following broken targets, and exposes one bounded `utility visual-metadata` batch operation. The Qt navigation/model layer owns generation and pending-path state, applies partial updates through `DirectoryModel::updateMetadata()`, and feeds rich exact-file or ordered-themed icon identities into the existing native icon service. QML only schedules visible ranges and draws a separate, bounded emblem layer.

**Tech Stack:** Rust 2024, `gio`/`glib`, serde JSON, Qt 6 C++ models/controllers, QML views, QtTest and Python QML source tests.

## Global Constraints

- Keep compilation in `/home/agony/GitHub/Orbit/build/debug` and use `rtk` for repository commands.
- Do not add GTK or per-file external processes.
- Keep GIO enrichment local-only, bounded to 64 paths and a bounded request payload.
- Keep existing `fileIconName` semantic overrides and the Freedesktop list-aware resolver intact.
- Reject stale visual metadata by generation and exact path; never let enrichment block listing/navigation.

### Task 1: Symlink-safe Rust listing

**Files:** `apps/explorer/backend/src/entries.rs`; Rust listing tests in the same module.

- [x] Add failing tests for valid file links, directory links, broken links, and recursive non-descent.
- [x] Run the focused Rust tests and observe the existing follow/drop behavior fail.
- [x] Centralize entry classification using `DirEntry::file_type()`, `symlink_metadata()`, and target metadata only when available.
- [x] Keep valid link target semantics, preserve broken links as rows, add `fileIsSymlink` and `fileSymlinkBroken`, and use the same classification in normal and recursive listing.
- [x] Run focused and full backend tests; commit the symlink fix.

### Task 2: GIO normalization and bounded backend operation

**Files:** `apps/explorer/backend/Cargo.toml`, root `Cargo.lock`, `apps/explorer/backend/src/main.rs`, new `file_visual_metadata.rs`, `apps/explorer/backend/src/utility.rs`, backend Rust tests.

- [x] Add normalizer tests for ordered themed names, custom URI/name precedence, local/non-local file icons, optional attributes, emblems, and unsupported icons.
- [x] Add the locked `gio` dependency after checking system `gio-2.0`/`glib-2.0` availability.
- [x] Implement exact requested GIO attributes only; normalize `GThemedIcon`, local `GFileIcon`, `GEmblemedIcon`, custom metadata, access flags, and deterministic emblems without image decoding or downloads.
- [x] Add `visual-metadata` dispatch with maximum 64 exact local paths and a total argument bound; return per-item ready/error-safe results.
- [x] Run backend unit/integration tests and verify the built backend starts; commit.

### Task 3: Qt contract, decoding, model updates, and request lifecycle

**Files:** `backend_types.h`, `rust_backend_client.{h,cpp}`, `fake_backend_client.{h,cpp}`, `directory_model.{h,cpp}`, `navigation_controller.{h,cpp}`, `app_state_facade.{h,cpp}`, related Qt tests.

- [x] Add failing Qt tests for optional decoding, new roles, changed-role emission, generation rejection, pending-path deduplication, and bounded batches.
- [x] Add neutral fields with safe defaults and a typed visual-metadata signal/request.
- [x] Track one generation-scoped pending set and debounce timer in `NavigationController`; request only visible local rows and reject stale responses.
- [x] Apply only present result fields by exact path, mark safe completions ready, and keep specialized `fileIconName` values unchanged.
- [x] Run focused Qt tests and commit.

### Task 4: Native exact-file/themed source and emblems

**Files:** `icon_theme_service.{h,cpp}`, `astrea_icon_image_provider.{h,cpp}`, QML views/state, QML tests.

- [x] Add failing tests for exact local source versioning, decode fallback, ordered themed names, missing emblem omission, thumbnail independence, and delegate reuse.
- [x] Add a `file/...` provider route with local-file validation, bounded Qt decoding, file-version cache identity, and themed fallback.
- [x] Add list-aware rich-name source methods and symbolic emblem source lookup; keep all theme resolution native.
- [x] Add bounded/debounced metadata warming to grid/list visible-range handling independently of `previewsEnabled`, and draw at most three emblems in a separate layer.
- [x] Run C++/QML tests and commit.

### Task 5: Packaging, documentation, and final verification

**Files:** `apps/explorer/docs/ICON_THEME_INTEGRATION.md`, build/dependency documentation as needed.

- [x] Document the fast listing/GIO enrichment split, precedence, emblems, symlinks, local-only policy, cache boundaries, and refresh limitation.
- [x] Build in the existing debug directory, run the affected native/QML and full backend gates, verify no GTK dependency was introduced, inspect the diff, and commit the final documentation/verification state.
