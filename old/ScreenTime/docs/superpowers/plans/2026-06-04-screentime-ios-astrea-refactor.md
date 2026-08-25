# ScreenTime iOS/Astrea Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor Bench ScreenTime into an Astrea-styled compact app with iOS Screen Time-inspired charts and lists.

**Architecture:** Keep the existing collector and QML process bridge. Extend `screentime.py` snapshot shaping with hourly and weekly aggregates, then rebuild the UI with shared `AstreaComponents` and local ScreenTime-specific chart/list components.

**Tech Stack:** Python 3, unittest, Quickshell QML, QtQuick Layouts, shared Astrea QML components.

---

### Task 1: Snapshot Contract

**Files:**
- Create: `tests/test_screentime_snapshot.py`
- Modify: `screentime.py`

- [ ] Add tests that build a temporary state and assert `day.hourly`, `week.days`, `week.apps`, `week.categories`, `day.top_apps`, `day.top_categories`, and `display`.
- [ ] Run `python3 -m unittest discover -s tests` and confirm the new tests fail before backend changes.
- [ ] Add timestamp-aware hourly accounting to the collector state.
- [ ] Add snapshot helpers for day/week range aggregation and display strings.
- [ ] Run unit tests and `./screentime.py snapshot --json --limit 5`.

### Task 2: Astrea Component Migration

**Files:**
- Create symlink: `AstreaComponents -> /home/agony/.local/share/Astrea/Core/components`
- Modify: `ScreenTimeApp.qml`
- Modify/Create: `components/sections/*.qml`
- Keep or remove local `components/common/*` depending on final imports.

- [ ] Replace local theme usage with `AstreaComponents` tokens.
- [ ] Build compact header, segmented control, summary card, charts, legend, status line, and most-used list.
- [ ] Add app/category toggle state in QML.
- [ ] Keep loading, error, stale collector, and empty-data states visible and compact.

### Task 3: Validation

**Files:**
- Modify: `README.md` if launch/import instructions change.

- [ ] Run `python3 -m py_compile screentime.py`.
- [ ] Run `python3 -m unittest discover -s tests`.
- [ ] Run `./screentime.py snapshot --json --limit 5`.
- [ ] Run `qmllint` on changed QML files where imports resolve.
- [ ] Run `timeout 4s quickshell -p /home/agony/GitHub/Bench/ScreenTime/ScreenTimeApp.qml`.
- [ ] Review `git diff` and confirm unrelated Bench changes were not touched.
