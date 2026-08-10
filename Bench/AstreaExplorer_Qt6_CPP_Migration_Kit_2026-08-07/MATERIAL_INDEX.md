# Material Index

## Design and Migration Documents

- `docs/00_EXECUTIVE_DECISION.md` — final architectural recommendation.
- `docs/01_CURRENT_STATE_AUDIT.md` — reviewed current-state facts.
- `docs/02_RUNTIME_DEPENDENCY_MAP.md` — current runtime and integration map.
- `docs/03_RUST_BACKEND_CONTRACT.md` — backend preservation and future transport.
- `docs/04_PYTHON_HELPER_DECOMPOSITION.md` — command-by-command helper removal plan.
- `docs/05_TARGET_QT6_CPP_ARCHITECTURE.md` — target C++/Rust/QML ownership.
- `docs/06_VISUAL_PARITY_CONTRACT.md` — strict no-redesign contract.
- `docs/07_MIGRATION_ROADMAP.md` — phase/gate sequence.
- `docs/08_FILE_BY_FILE_PLAN.md` — migration ownership by current file.
- `docs/09_FILECHOOSER_PORTAL_MIGRATION.md` — portal conversion plan.
- `docs/10_BUILD_PACKAGING_INTEGRATION.md` — native build/install/session details.
- `docs/11_TEST_AND_QUALIFICATION_PLAN.md` — deterministic and real-session tests.
- `docs/12_RISK_REGISTER.md` — major migration risks and mitigations.
- `docs/13_PERFORMANCE_PLAN.md` — PSS/CPU/spawn/latency measurement.
- `docs/14_IMPLEMENTATION_PROMPT.md` — ready-to-send implementation prompt.

## Source Reference

- `source/AstreaOS/src/Apps/Explorer/` — canonical reviewed Explorer source snapshot.
- `source/AstreaOS/src/Core/bridge/apps/explorer/` — Rust backend crate.
- `source/AstreaOS/src/Core/bridge/apps/explorer_backend` — tracked backend ELF from supplied snapshot.
- `source/AstreaOS/src/System/portal/` — FileChooser portal implementation.
- `source/AstreaOS/src/Features/Files/` — shared file UI module used by Explorer.
- `source/AstreaOS/src/Core/components/` — shared visual component module.
- `source/AstreaOS/src/System/i18n/` — i18n data/runtime.
- `source/AstreaOS/src/Quickshell/components/` — current component dependency retained for migration reference only.
- `source/original/Explorer.zip` — original standalone upload.

## Existing Astrea Documentation

`references/` contains the relevant existing Explorer/portal/backend architecture notes and backend performance changelog from the supplied AstreaOS snapshot.

## Manifests

- `manifests/inventory.json`
- `manifests/SOURCE_SHA256.txt`
- `manifests/VISUAL_BASELINE_SHA256.txt`
- `manifests/SOURCE_ORIGINS.md`

## Utilities

- `scripts/audit_quickshell_dependencies.py`
- `scripts/verify_snapshot_hashes.py`
