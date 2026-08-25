# ScreenTime Service UI Backend Split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split ScreenTime into backend/UI areas, add an installable user service helper, and improve UI consistency with a reusable segmented control.

**Architecture:** Keep the Python collector as the backend CLI, called by QML through a stable wrapper. Move QML into `ui/`, keep shared Astrea components linked locally, and isolate reusable UI primitives under `ui/components/common`.

**Tech Stack:** Python 3, unittest, systemd user service, Quickshell QML, AstreaComponents.

---

### Task 1: Backend Split

- [ ] Create `backend/` and move `screentime.py` plus tests.
- [ ] Add `bin/screentime` wrapper.
- [ ] Update imports/tests to use the backend module.
- [ ] Validate `py_compile`, unittest, status, and snapshot.

### Task 2: Service

- [ ] Update `bench-screentime.service` to call `bin/screentime monitor`.
- [ ] Add `bin/screentime-service` helper for install/start/stop/restart/status.
- [ ] Reinstall/restart the user service and verify active collection.

### Task 3: UI Split and Componentization

- [ ] Move QML files under `ui/`.
- [ ] Add reusable `ui/components/common/SegmentedControl.qml`.
- [ ] Replace inline segment code in `ui/ScreenTimeApp.qml`.
- [ ] Update QML bridge path to call `bin/screentime`.

### Task 4: UI Polish

- [ ] Align ScreenTime spacing/surfaces/hover treatment with Email patterns.
- [ ] Improve header/status/list controls.
- [ ] Validate `qmllint` and Quickshell load.
