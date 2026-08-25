# ScreenTime iOS/Astrea Refactor Design

## Goal

Modernize the old Bench ScreenTime prototype into an Astrea-style app inspired by the iPhone Screen Time view. The app should keep the existing focused-window collector and editable `app_rules.json`, but present the data with a richer day/week summary, hourly bars, category legend, and most-used app list.

## Scope

In scope:

- Replace local one-off visual primitives with shared `AstreaComponents` where practical.
- Keep the compact single-window experience, sized closer to the iPhone reference than to a wide dashboard.
- Add snapshot data for daily hourly usage and weekly daily usage.
- Preserve current app/category rule matching and persistent state paths.
- Improve UI refresh resilience, empty states, and degraded collector health messaging.

Out of scope:

- A full settings editor for categories.
- Long-term historical analytics beyond the current week and selected day.
- Moving ScreenTime into the live Astrea app tree.
- Rewriting the collector around event subscriptions instead of the current sampling loop.

## Architecture

The app stays split into three small layers:

- `screentime.py`: collector, state migration, snapshot shaping, formatting.
- `state/ScreenTimeState.qml`: QML process bridge that calls `screentime.py snapshot --json`.
- QML UI files: window layout and ScreenTime-specific visual components.

The UI imports a local `AstreaComponents` symlink pointing to `/home/agony/.local/share/Astrea/Core/components`, matching other Bench prototypes. ScreenTime-specific charts remain local components because the shared system does not currently provide Screen Time charts.

## Snapshot Contract

The existing snapshot keeps `totals`, `day.categories`, `day.apps`, `all_time`, `current`, and `health`.

Add:

- `day.hourly`: 24 rows with hour index, seconds, duration, and category breakdown.
- `week.days`: 7 rows ending on the selected day, each with date, short label, seconds, duration, and category breakdown.
- `week.categories` and `week.apps`: sorted rows aggregated across the same 7-day range.
- `day.top_categories`: sorted category rows with percent of day total.
- `day.top_apps`: sorted app rows with percent of day total, category, class, and duration.
- `display`: preformatted strings for selected date, generated time, and collector status.

The backend should compute these from existing state. If older state does not have hourly detail, the app should still show totals and lists while charts render empty/partial data.

## UI Design

The main view follows the approved reference:

- Header with back-style circular control space, centered `ScreenTime` title, and no extra explanatory copy.
- Segmented control for `Semana` and `Dia`. `Dia` shows selected-day totals, hourly bars, and daily most-used rows. `Semana` shows 7-day totals, daily bars, and weekly most-used rows.
- Large summary card with selected date, large total duration, weekly bar chart, hourly bar chart, and category legend.
- Timestamp/status line under the summary card.
- `Mais Usados` section with a `Mostrar Categorias` toggle action that switches the list between app rows and category rows.
- Rows show icon fallback, label, progress bar, duration, and chevron-style affordance.

Use shared Astrea tokens for colors, typography, radius, spacing, cards, text labels, dividers, buttons where they fit. Avoid local `Theme.qml` duplication unless a small compatibility shim is needed.

## Error Handling

- If snapshot parsing fails, show a centered Astrea-styled error state.
- If the collector is not running or stale, keep data visible and show a warning status rather than replacing the page.
- If there is no usage today, render empty charts and a quiet `Sem dados ainda` list state.
- If `hyprctl` is unavailable, keep the collector behavior that records unknown/degraded samples without corrupting totals.

## Testing

Minimum verification:

- `python3 -m py_compile screentime.py`
- `./screentime.py snapshot --json --limit 5`
- `qmllint` on changed QML files where imports resolve
- `timeout 4s quickshell -p /home/agony/GitHub/Bench/ScreenTime/ScreenTimeApp.qml`

Manual check:

- The app opens with Astrea styling.
- The summary card renders without overlap at the target compact width.
- Empty today data does not break the charts or rows.
- Existing app/category classification still works.
