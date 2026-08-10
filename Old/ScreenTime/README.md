# Bench ScreenTime

Focused-app usage tracker for Hyprland.

It follows Hyprland focus changes through the `.socket2.sock` event stream, uses `hyprctl activewindow -j` only when it needs a fresh focused-window snapshot, groups apps through `app_rules.json`, and writes usage state outside the repo:

```text
~/.local/state/Bench/ScreenTime/usage.json
~/.local/state/Bench/ScreenTime/events.jsonl
~/.local/state/Bench/ScreenTime/settings.json
```

## Run

```bash
./bin/screentime monitor
```

The monitor uses a lock file so only one collector writes state at a time. It reloads `app_rules.json` while running, records collector health in `usage.json`, and splits elapsed time across hours and midnight instead of assigning the whole sample to the current period. If the Hyprland event socket is unavailable, it falls back to polling until the socket can be opened again.

Print the current summary:

```bash
./bin/screentime report
./bin/screentime report --day 2026-05-01
./bin/screentime snapshot --json
```

Show the state/config paths:

```bash
./bin/screentime path
```

Inspect backend health, schema, focused app, and paths:

```bash
./bin/screentime status
./bin/screentime status --json
./bin/screentime service status --json
./bin/screentime service enable --json
./bin/screentime service disable --json
./bin/screentime settings status --json
./bin/screentime settings hide-app terminal --json
./bin/screentime settings show-app terminal --json
```

Reset collected data:

```bash
./bin/screentime reset
```

## Categories

Edit `app_rules.json` to change the logic. It is intentionally separate from the monitor code.

Current examples:

```text
steam -> games
brave, zen, firefox, chrome -> browser
```

Matching checks exact titles, title aliases, and then focused window classes. This lets Quickshell/Astrea apps avoid collapsing into a generic `org.quickshell` bucket.

## Astrea user service

The Rust collector is installed as `astrea-screentimed.service`, matching the Astrea user-service naming pattern.

```bash
./bin/screentime-service enable
./bin/screentime-service status
./bin/screentime-service logs
```

The helper removes old `bench-screentime.service`, `astrea-screentimed-rs.service`, and `astrea-screentimed-legacy.service` unit links before starting the default Rust service. The backend also tries to discover the current Hyprland socket under `/run/user/$UID/hypr` if the service environment is incomplete.

`snapshot` and `status` compute effective collector health instead of trusting stale state blindly. A recent `last_sample_at` keeps the collector active; an old sample is reported as `Coletor sem atualizacao`.

## Rust Backend

`backend-rs` is the default ScreenTime backend. It writes `usage.json`/`events.jsonl`, stores UI preferences in `settings.json`, uses the Hyprland event socket strategy, and exposes the UI-facing `snapshot`, `status`, `report`, `reset`, `settings`, and `service` commands.

```bash
./bin/screentime-rs snapshot --json
./bin/screentime-rs status
./bin/screentime-rs service status --json
./bin/screentime-rs settings status --json
cargo test --manifest-path backend-rs/Cargo.toml
cargo build --release --manifest-path backend-rs/Cargo.toml
```

## Legacy-Backend

The old Python backend lives in `Legacy-Backend` and is kept as an explicit fallback.

```bash
./bin/screentime-legacy status
./bin/screentime-rs-service restore-legacy
```

Do not run it together with `astrea-screentimed.service`; both collectors intentionally share the same lock and state file, so only one backend should own tracking at a time.

## UI

```bash
quickshell -p ui/ScreenTimeApp.qml
```

The UI refreshes the JSON snapshot every five seconds through `./bin/screentime-rs snapshot --json`. The top button opens ScreenTime settings, where the service toggle and hidden-app preferences live. It uses the shared Astrea component link:

```text
ui/AstreaComponents -> ~/.local/share/Astrea/Core/components
ui/ShellComponents -> ~/.local/share/Astrea/Quickshell/components
```

The snapshot includes daily hourly bars, seven-day weekly bars, top apps, top categories, hidden-app settings, and display strings so the QML app can render an iOS Screen Time-inspired compact view without parsing collector state directly. Hidden apps are removed from app lists only; category totals remain counted.

App icons use the same `ShellComponents/AppIcon.qml` wrapper used by Alt-Tab, with focused-window class/title metadata coming from the backend snapshot. Reusable UI pieces live in `ui/components/common`, including `SegmentedControl.qml`.
