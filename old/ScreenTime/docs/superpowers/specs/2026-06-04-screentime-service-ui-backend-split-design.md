# ScreenTime Service and UI/Backend Split Design

## Goal

Turn the Bench ScreenTime prototype into a cleaner app module with a dedicated backend area, a dedicated UI area, an installable user service, and a reusable segmented control component.

## Structure

- `backend/screentime.py`: collector, snapshot, status, CLI.
- `backend/tests/`: Python backend tests.
- `ui/ScreenTimeApp.qml`: app window.
- `ui/state/`: QML bridge that calls the backend CLI.
- `ui/components/common/SegmentedControl.qml`: reusable pill segmented control.
- `ui/components/sections/`: ScreenTime-specific charts, summary, and rows.
- `bin/screentime`: root command wrapper for the backend CLI.
- `bin/screentime-service`: install/start/status helper for the user service.
- `bench-screentime.service`: systemd user service pointing at the backend wrapper.

Keep `AstreaComponents` as a local symlink to the live Astrea component tree, matching other Bench apps.

## UI Direction

Keep the iOS Screen Time-inspired content hierarchy, but make the polish more consistent with the Email prototype:

- Astrea card surfaces and borders.
- Reusable chip/control styling instead of inline custom controls.
- Hover/pressed states on controls and rows.
- Cleaner header and status treatment.
- Reusable `SegmentedControl` with a generic model/currentValue API.

## Backend Direction

Preserve current state paths under `~/.local/state/Bench/ScreenTime`. The backend CLI remains the source of truth for `monitor`, `snapshot`, `status`, `path`, and `reset`. The service helper installs and starts the user service without requiring the UI.

## Validation

- `python3 -m py_compile backend/screentime.py`
- `python3 -m unittest discover -s backend/tests`
- `./bin/screentime status`
- `./bin/screentime snapshot --json --limit 5`
- `qmllint` for changed QML files
- `timeout 4s quickshell -p ui/ScreenTimeApp.qml`
- `systemctl --user status bench-screentime.service --no-pager`
