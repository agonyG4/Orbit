#!/usr/bin/env python3
"""
Bench ScreenTime Legacy-Backend.

Tracks focused app usage on Hyprland through focus events, with a polling fallback.
State is stored under ~/.local/state/Bench/ScreenTime.
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import re
import select
import signal
import socket
import subprocess
import sys
import time
from datetime import date, datetime, time as datetime_time, timedelta, timezone
from functools import lru_cache
from pathlib import Path
from typing import Any


BACKEND_DIR = Path(__file__).resolve().parent
PROJECT_DIR = BACKEND_DIR.parent
RULES_PATH = PROJECT_DIR / "app_rules.json"
STATE_DIR = Path.home() / ".local" / "state" / "Bench" / "ScreenTime"
STATE_PATH = STATE_DIR / "usage.json"
EVENTS_PATH = STATE_DIR / "events.jsonl"
LOCK_PATH = STATE_DIR / "monitor.lock"
SERVICE_NAME = "astrea-screentimed-legacy.service"
LEGACY_SERVICE_NAMES = ("bench-screentime.service", "astrea-screentimed-rs.service")
SERVICE_UNIT_SOURCE = PROJECT_DIR / SERVICE_NAME
USER_UNIT_DIR = Path(os.environ.get("XDG_CONFIG_HOME") or (Path.home() / ".config")).expanduser() / "systemd" / "user"
ASTREA_ROOT = Path(os.environ.get("ASTREA_ROOT", Path.home() / ".local/share/Astrea")).expanduser()
ICON_RESOLVER_SCRIPT = ASTREA_ROOT / "Core" / "bridge" / "system" / "app_icons.py"
DEFAULT_INTERVAL = 15.0
MAX_SAMPLE_SECONDS = 60.0
STATE_SCHEMA_VERSION = 4
HEALTH_STALE_FLOOR_SECONDS = 90.0
EVENT_SOCKET_RETRY_SECONDS = 30.0
HYPRLAND_FOCUS_EVENT_NAMES = frozenset(
    {
        "activewindow",
        "activewindowv2",
        "closewindow",
        "focusedmon",
        "movewindow",
        "openwindow",
        "workspace",
        "workspacev2",
    }
)
APP_METADATA_KEYS = (
    "category",
    "class",
    "title",
    "last_title",
    "last_seen_at",
    "initialClass",
    "initialTitle",
    "pid",
    "address",
    "icon",
    "iconSource",
    "icon_path",
    "iconPath",
    "astreaIcon",
    "astreaIconName",
    "hideIconFallback",
)
MONTH_NAMES_PT = [
    "janeiro",
    "fevereiro",
    "marco",
    "abril",
    "maio",
    "junho",
    "julho",
    "agosto",
    "setembro",
    "outubro",
    "novembro",
    "dezembro",
]
WEEKDAY_SHORT_PT = ["Seg", "Ter", "Qua", "Qui", "Sex", "Sab", "Dom"]
SERVICE_ENV_KEYS = ("HYPRLAND_INSTANCE_SIGNATURE", "WAYLAND_DISPLAY", "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE")


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def today_key() -> str:
    return datetime.now().strftime("%Y-%m-%d")


def day_key_from_timestamp(timestamp: float) -> str:
    return datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d")


def next_local_midnight(timestamp: float) -> float:
    current = datetime.fromtimestamp(timestamp)
    next_day = datetime.combine(current.date() + timedelta(days=1), datetime_time.min)
    return next_day.timestamp()


def next_local_hour(timestamp: float) -> float:
    current = datetime.fromtimestamp(timestamp)
    next_hour = current.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1)
    return next_hour.timestamp()


def load_json(path: Path, fallback: dict[str, Any]) -> dict[str, Any]:
    if not path.exists():
        return fallback
    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else fallback
    except (OSError, json.JSONDecodeError):
        return fallback


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)
        f.write("\n")
    os.replace(tmp, path)


def load_rules(path: Path = RULES_PATH) -> dict[str, Any]:
    rules = load_json(path, {})
    categories = rules.get("categories", {})
    if not isinstance(categories, dict):
        categories = {}
    return {"categories": categories, "loaded_at": now_iso()}


def normalize(value: str | None) -> str:
    return (value or "").strip().lower()


def rule_aliases(app_id: str, raw_rule: Any) -> tuple[list[str], list[str], list[str]]:
    class_aliases = [normalize(app_id)]
    title_aliases: list[str] = []
    exact_titles: list[str] = []

    if isinstance(raw_rule, list):
        class_aliases.extend(normalize(alias) for alias in raw_rule)
    elif isinstance(raw_rule, dict):
        class_aliases.extend(normalize(alias) for alias in raw_rule.get("aliases", []))
        title_aliases.extend(normalize(alias) for alias in raw_rule.get("title_aliases", []))
        exact_titles.extend(normalize(alias) for alias in raw_rule.get("exact_titles", []))

    return class_aliases, title_aliases, exact_titles


def category_label(rules: dict[str, Any], category_id: str, fallback: str) -> str:
    category = rules.get("categories", {}).get(category_id, {})
    if isinstance(category, dict):
        return str(category.get("label", fallback))
    return fallback


def fallback_game_app_id(class_key: str, title_key: str) -> str:
    return class_key or title_key or "unknown"


def looks_like_windows_game(class_key: str, title_key: str) -> bool:
    for value in (class_key, title_key):
        value = value.strip()
        if not value:
            continue
        if value_looks_like_windows_game(value):
            return True
    return False


def value_looks_like_windows_game(value: str) -> bool:
    return bool(
        value
        and (
            ".exe" in value
            or "steam_app_" in value
            or value in {"wine", "steam_proton"}
            or "pressure-vessel" in value
        )
    )


def process_tokens_look_like_windows_game(tokens: list[str]) -> bool:
    return any(value_looks_like_windows_game(normalize(token)) for token in tokens)


def process_looks_like_windows_game(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        raw = Path(f"/proc/{pid}/cmdline").read_bytes()
    except OSError:
        return False
    tokens = [part.decode("utf-8", "replace") for part in raw.split(b"\0") if part]
    return process_tokens_look_like_windows_game(tokens)


def resolve_app(raw_class: str, title: str, rules: dict[str, Any]) -> tuple[str, str, str]:
    class_key = normalize(raw_class)
    title_key = normalize(title)

    for category_id, category in rules["categories"].items():
        apps = category.get("apps", {})
        if not isinstance(apps, dict):
            continue
        for app_id, raw_rule in apps.items():
            class_aliases, title_aliases, exact_titles = rule_aliases(str(app_id), raw_rule)

            for exact_title in exact_titles:
                if exact_title and title_key == exact_title:
                    return str(app_id), str(category_id), category.get("label", str(category_id))

            for alias in title_aliases:
                if alias and title_key and alias in title_key:
                    return str(app_id), str(category_id), category.get("label", str(category_id))

            for alias in class_aliases:
                if alias and (alias == class_key or alias in class_key):
                    return str(app_id), str(category_id), category.get("label", str(category_id))

    if looks_like_windows_game(class_key, title_key):
        return fallback_game_app_id(class_key, title_key), "games", category_label(rules, "games", "Games")

    app_id = class_key or "unknown"
    return app_id, "other", category_label(rules, "other", "Other")


def resolve_app_with_process(raw_class: str, title: str, pid: int, rules: dict[str, Any]) -> tuple[str, str, str]:
    app, category, label = resolve_app(raw_class, title, rules)
    if category != "other" or not process_looks_like_windows_game(pid):
        return app, category, label
    return fallback_game_app_id(normalize(raw_class), normalize(title)), "games", category_label(rules, "games", "Games")


def hyprctl_env() -> dict[str, str]:
    env = os.environ.copy()
    if env.get("HYPRLAND_INSTANCE_SIGNATURE"):
        return env

    runtime_dir = Path(env.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}"))
    hypr_dir = runtime_dir / "hypr"
    try:
        instances = [
            path
            for path in hypr_dir.iterdir()
            if path.is_dir() and (path / ".socket.sock").exists()
        ]
    except OSError:
        return env

    if instances:
        newest = max(instances, key=lambda path: path.stat().st_mtime)
        env["HYPRLAND_INSTANCE_SIGNATURE"] = newest.name
    return env


def hyprland_event_socket_path(env: dict[str, str] | None = None) -> Path | None:
    lookup_env = dict(env) if env is not None else hyprctl_env()
    runtime_dir = Path(lookup_env.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}"))
    signature = lookup_env.get("HYPRLAND_INSTANCE_SIGNATURE", "")
    if signature:
        socket_path = runtime_dir / "hypr" / signature / ".socket2.sock"
        if socket_path.exists():
            return socket_path

    hypr_dir = runtime_dir / "hypr"
    try:
        instances = [
            path
            for path in hypr_dir.iterdir()
            if path.is_dir() and (path / ".socket2.sock").exists()
        ]
    except OSError:
        return None

    if not instances:
        return None
    newest = max(instances, key=lambda path: path.stat().st_mtime)
    return newest / ".socket2.sock"


def hyprland_event_name(line: str) -> str:
    return line.split(">>", 1)[0].strip().lower()


def hyprland_event_payload(line: str) -> str:
    if ">>" not in line:
        return ""
    return line.split(">>", 1)[1].strip()


def normalize_window_address(value: Any) -> str:
    text = str(value or "").strip().lower()
    return text[2:] if text.startswith("0x") else text


def hyprland_event_changes_focus(line: str) -> bool:
    return hyprland_event_name(line) in HYPRLAND_FOCUS_EVENT_NAMES


def hyprland_event_requires_sample(line: str, previous_sample: dict[str, Any]) -> bool:
    if not hyprland_event_changes_focus(line):
        return False

    event_name = hyprland_event_name(line)
    payload = hyprland_event_payload(line)
    if event_name == "activewindowv2":
        next_address = normalize_window_address(payload.split(",", 1)[0])
        current_address = normalize_window_address(previous_sample.get("address", ""))
        return bool(next_address and next_address != current_address)
    if event_name == "activewindow":
        next_class = normalize(payload.split(",", 1)[0])
        current_class = normalize(str(previous_sample.get("class", "")))
        return bool(next_class and next_class != current_class)
    return True


class HyprlandEventStream:
    def __init__(self, socket_path: Path):
        self.socket_path = socket_path
        self._buffer = ""
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            self._socket.settimeout(2.0)
            self._socket.connect(str(socket_path))
            self._socket.setblocking(False)
        except OSError:
            self.close()
            raise

    def close(self) -> None:
        try:
            self._socket.close()
        except OSError:
            pass

    def read_line(self, timeout_seconds: float) -> str | None:
        deadline = time.monotonic() + max(0.0, timeout_seconds)
        while True:
            if "\n" in self._buffer:
                line, self._buffer = self._buffer.split("\n", 1)
                return line.strip()

            remaining = max(0.0, deadline - time.monotonic())
            if remaining <= 0:
                return None

            ready, _, _ = select.select([self._socket], [], [], remaining)
            if not ready:
                return None

            chunk = self._socket.recv(4096)
            if not chunk:
                raise OSError("Hyprland event socket closed")
            self._buffer += chunk.decode("utf-8", "replace")


def focused_window() -> dict[str, Any]:
    try:
        proc = subprocess.run(
            ["hyprctl", "activewindow", "-j"],
            check=False,
            capture_output=True,
            text=True,
            timeout=2.0,
            env=hyprctl_env(),
        )
    except (FileNotFoundError, subprocess.SubprocessError):
        return {"app": "unknown", "class": "", "title": "", "address": "", "ok": False, "error": "hyprctl unavailable"}

    if proc.returncode != 0 or not proc.stdout.strip():
        err = proc.stderr.strip() or f"hyprctl exited {proc.returncode}"
        return {"app": "unknown", "class": "", "title": "", "address": "", "ok": False, "error": err}

    try:
        window = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"app": "unknown", "class": "", "title": "", "address": "", "ok": False, "error": "invalid hyprctl json"}

    raw_class = window.get("class") or window.get("initialClass") or ""
    title = window.get("title") or window.get("initialTitle") or ""
    return {
        "class": str(raw_class),
        "title": str(title),
        "initialClass": str(window.get("initialClass") or raw_class or ""),
        "initialTitle": str(window.get("initialTitle") or title or ""),
        "pid": int(window.get("pid") or 0),
        "address": str(window.get("address") or ""),
        "workspace": window.get("workspace", {}).get("name", ""),
        "ok": bool(raw_class or title),
        "error": "",
    }


def empty_state() -> dict[str, Any]:
    return {
        "schema_version": STATE_SCHEMA_VERSION,
        "created_at": now_iso(),
        "updated_at": now_iso(),
        "total_seconds": 0.0,
        "active_seconds": 0.0,
        "unknown_seconds": 0.0,
        "sample_count": 0,
        "error_count": 0,
        "days": {},
        "weeks": {},
        "apps": {},
        "categories": {},
        "current": {},
        "health": {
            "running": False,
            "last_error": "",
            "last_sample_at": "",
        },
    }


def ensure_bucket(root: dict[str, Any], key: str, label: str | None = None) -> dict[str, Any]:
    bucket = root.setdefault(key, {"seconds": 0.0})
    if label:
        bucket["label"] = label
    return bucket


def ensure_hour_bucket(day_bucket: dict[str, Any], hour: int) -> dict[str, Any]:
    hourly = day_bucket.setdefault("hourly", {})
    key = str(hour)
    return hourly.setdefault(
        key,
        {
            "seconds": 0.0,
            "active_seconds": 0.0,
            "unknown_seconds": 0.0,
            "apps": {},
            "categories": {},
        },
    )


def week_start_for(value: date) -> date:
    return value - timedelta(days=value.weekday())


def week_end_for(start: date) -> date:
    return start + timedelta(days=6)


def week_key_for_date(value: date) -> str:
    return date_key(week_start_for(value))


def week_key_from_timestamp(timestamp: float) -> str:
    return week_key_for_date(datetime.fromtimestamp(timestamp).date())


def empty_week_bucket() -> dict[str, Any]:
    return {
        "seconds": 0.0,
        "active_seconds": 0.0,
        "unknown_seconds": 0.0,
        "apps": {},
        "categories": {},
    }


def ensure_week_bucket(state: dict[str, Any], week_key: str) -> dict[str, Any]:
    weeks = state.setdefault("weeks", {})
    if not isinstance(weeks, dict):
        weeks = {}
        state["weeks"] = weeks
    bucket = weeks.setdefault(week_key, empty_week_bucket())
    bucket.setdefault("apps", {})
    bucket.setdefault("categories", {})
    return bucket


def rebuild_weeks_from_days(state: dict[str, Any]) -> None:
    state["weeks"] = {}
    days = state.get("days", {})
    if not isinstance(days, dict):
        return

    for raw_day_key, day_bucket in sorted(days.items()):
        if not isinstance(day_bucket, dict):
            continue
        try:
            current_day = parse_day_key(str(raw_day_key))
        except ValueError:
            continue

        week_bucket = ensure_week_bucket(state, week_key_for_date(current_day))
        increment_bucket(week_bucket, float(day_bucket.get("seconds", 0.0)))
        week_bucket["active_seconds"] = round(float(week_bucket.get("active_seconds", 0.0)) + float(day_bucket.get("active_seconds", 0.0)), 3)
        week_bucket["unknown_seconds"] = round(float(week_bucket.get("unknown_seconds", 0.0)) + float(day_bucket.get("unknown_seconds", 0.0)), 3)
        merge_usage(week_bucket.setdefault("apps", {}), day_bucket.get("apps", {}))
        merge_usage(week_bucket.setdefault("categories", {}), day_bucket.get("categories", {}))


def migrate_state(state: dict[str, Any]) -> dict[str, Any]:
    previous_schema = int(state.get("schema_version", 0) or 0)
    fresh = empty_state()
    for key, value in fresh.items():
        state.setdefault(key, value)
    state["schema_version"] = STATE_SCHEMA_VERSION
    state.setdefault("health", fresh["health"])
    if previous_schema < 4 or not isinstance(state.get("weeks"), dict) or (not state.get("weeks") and state.get("days")):
        rebuild_weeks_from_days(state)
    total = float(state.get("total_seconds", 0.0))
    if total > 0 and float(state.get("active_seconds", 0.0)) == 0 and float(state.get("unknown_seconds", 0.0)) == 0:
        state["active_seconds"] = total
    for day_bucket in state.get("days", {}).values():
        day_total = float(day_bucket.get("seconds", 0.0))
        if day_total > 0 and float(day_bucket.get("active_seconds", 0.0)) == 0 and float(day_bucket.get("unknown_seconds", 0.0)) == 0:
            day_bucket["active_seconds"] = day_total
    return state


def parse_iso_datetime(value: str) -> datetime | None:
    text = str(value or "").strip()
    if not text:
        return None
    try:
        parsed = datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        return parsed.astimezone()
    return parsed


def pid_exists(pid: Any) -> bool:
    try:
        pid_int = int(pid)
    except (TypeError, ValueError):
        return False
    if pid_int <= 0:
        return False
    try:
        os.kill(pid_int, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def run_systemctl_user(args: list[str], tolerate_failure: bool = False) -> subprocess.CompletedProcess[str]:
    command = ["systemctl", "--user", *args]
    try:
        proc = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=8.0,
        )
    except (FileNotFoundError, subprocess.SubprocessError) as error:
        if tolerate_failure:
            return subprocess.CompletedProcess(command, 1, "", str(error))
        raise RuntimeError(str(error)) from error

    if proc.returncode != 0 and not tolerate_failure:
        message = proc.stderr.strip() or proc.stdout.strip() or f"systemctl exited {proc.returncode}"
        raise RuntimeError(message)
    return proc


def parse_systemctl_show(output: str) -> dict[str, str]:
    props: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        props[key] = value
    return props


def service_display_state(status: dict[str, Any]) -> str:
    if status.get("active") and status.get("enabled"):
        return "Ativo"
    if status.get("active"):
        return "Rodando"
    if status.get("enabled"):
        return "Habilitado"
    if status.get("installed"):
        return "Desativado"
    return "Nao instalado"


def unit_status(unit_name: str) -> dict[str, Any]:
    proc = run_systemctl_user(["show", unit_name], tolerate_failure=True)
    props = parse_systemctl_show(proc.stdout)
    load_state = props.get("LoadState", "not-found")
    active_state = props.get("ActiveState", "inactive")
    unit_file_state = props.get("UnitFileState", "disabled")
    fragment_path = props.get("FragmentPath", "")
    installed = load_state not in ("not-found", "bad") or bool(fragment_path)
    enabled = unit_file_state in ("enabled", "enabled-runtime")
    active = active_state == "active"
    status = {
        "unit": unit_name,
        "installed": installed,
        "enabled": enabled,
        "active": active,
        "load_state": load_state,
        "active_state": active_state,
        "sub_state": props.get("SubState", ""),
        "unit_file_state": unit_file_state,
        "fragment_path": fragment_path,
        "last_error": "" if proc.returncode == 0 else (proc.stderr.strip() or proc.stdout.strip()),
    }
    status["display"] = {"state": service_display_state(status)}
    return status


def service_status() -> dict[str, Any]:
    return unit_status(SERVICE_NAME)


def service_unit_link() -> Path:
    return USER_UNIT_DIR / SERVICE_NAME


def install_service_unit() -> None:
    USER_UNIT_DIR.mkdir(parents=True, exist_ok=True)
    link = service_unit_link()
    if link.is_symlink() or link.exists():
        try:
            if link.is_symlink() and link.resolve() == SERVICE_UNIT_SOURCE.resolve():
                return
        except OSError:
            pass
        link.unlink()
    link.symlink_to(SERVICE_UNIT_SOURCE)


def remove_legacy_service_units() -> None:
    for legacy_unit in LEGACY_SERVICE_NAMES:
        run_systemctl_user(["disable", "--now", legacy_unit], tolerate_failure=True)
        legacy_path = USER_UNIT_DIR / legacy_unit
        if legacy_path.is_symlink() or legacy_path.exists():
            legacy_path.unlink()


def import_service_environment() -> None:
    run_systemctl_user(["import-environment", *SERVICE_ENV_KEYS], tolerate_failure=True)


def service_action(action: str) -> dict[str, Any]:
    if action == "status":
        result = service_status()
        result["ok"] = True
        result["action"] = action
        return result

    if action in ("enable", "start", "restart"):
        install_service_unit()
        remove_legacy_service_units()
        import_service_environment()
        run_systemctl_user(["daemon-reload"])
    elif action in ("disable", "stop"):
        remove_legacy_service_units()

    if action == "enable":
        run_systemctl_user(["enable", "--now", SERVICE_NAME])
    elif action == "disable":
        run_systemctl_user(["disable", "--now", SERVICE_NAME], tolerate_failure=True)
        run_systemctl_user(["daemon-reload"], tolerate_failure=True)
    elif action == "start":
        run_systemctl_user(["start", SERVICE_NAME])
    elif action == "stop":
        run_systemctl_user(["stop", SERVICE_NAME], tolerate_failure=True)
    elif action == "restart":
        run_systemctl_user(["restart", SERVICE_NAME])
    else:
        raise ValueError(f"Unsupported service action: {action}")

    result = service_status()
    result["ok"] = True
    result["action"] = action
    return result


def effective_health(raw_health: dict[str, Any]) -> dict[str, Any]:
    health = dict(raw_health or {})
    running = bool(health.get("running", False))
    interval = max(1.0, float(health.get("interval_seconds", DEFAULT_INTERVAL) or DEFAULT_INTERVAL))
    stale_after = max(HEALTH_STALE_FLOOR_SECONDS, interval * 4)
    sample_at = parse_iso_datetime(str(health.get("last_sample_at", "")))
    now = datetime.now(sample_at.tzinfo or timezone.utc) if sample_at else datetime.now(timezone.utc)
    sample_age = max(0.0, (now - sample_at).total_seconds()) if sample_at else None
    stale = running and (sample_age is None or sample_age > stale_after)

    pid = health.get("pid")
    pid_alive = pid_exists(pid) if pid else False

    if stale:
        health["running"] = False
    else:
        health["running"] = running

    health["stale"] = bool(stale)
    health["pid_alive"] = bool(pid_alive)
    health["sample_age_seconds"] = round(sample_age, 3) if sample_age is not None else None
    if stale and not str(health.get("last_error", "")).strip():
        health["last_error"] = "collector stale"
    return health


def increment_bucket(bucket: dict[str, Any], seconds: float) -> None:
    bucket["seconds"] = round(float(bucket.get("seconds", 0.0)) + seconds, 3)


def update_app_bucket_metadata(bucket: dict[str, Any], sample: dict[str, Any]) -> None:
    bucket["category"] = sample.get("category", "")
    bucket["class"] = sample.get("class", "")
    bucket["title"] = sample.get("title", "")
    bucket["last_title"] = sample.get("title", "")
    bucket["initialClass"] = sample.get("initialClass", "")
    bucket["initialTitle"] = sample.get("initialTitle", "")
    bucket["pid"] = sample.get("pid", 0)
    bucket["address"] = sample.get("address", "")


def add_seconds_to_day(state: dict[str, Any], sample: dict[str, Any], seconds: float, timestamp: float) -> None:
    if seconds <= 0:
        return

    day = day_key_from_timestamp(timestamp)
    week_key = week_key_from_timestamp(timestamp)
    app = sample["app"]
    category = sample["category"]

    state["total_seconds"] = round(float(state.get("total_seconds", 0.0)) + seconds, 3)
    if sample.get("ok", False):
        state["active_seconds"] = round(float(state.get("active_seconds", 0.0)) + seconds, 3)
    else:
        state["unknown_seconds"] = round(float(state.get("unknown_seconds", 0.0)) + seconds, 3)

    day_bucket = ensure_bucket(state.setdefault("days", {}), day)
    increment_bucket(day_bucket, seconds)
    if sample.get("ok", False):
        day_bucket["active_seconds"] = round(float(day_bucket.get("active_seconds", 0.0)) + seconds, 3)
    else:
        day_bucket["unknown_seconds"] = round(float(day_bucket.get("unknown_seconds", 0.0)) + seconds, 3)

    day_apps = day_bucket.setdefault("apps", {})
    day_categories = day_bucket.setdefault("categories", {})
    hour_bucket = ensure_hour_bucket(day_bucket, datetime.fromtimestamp(timestamp).hour)
    increment_bucket(hour_bucket, seconds)
    if sample.get("ok", False):
        hour_bucket["active_seconds"] = round(float(hour_bucket.get("active_seconds", 0.0)) + seconds, 3)
    else:
        hour_bucket["unknown_seconds"] = round(float(hour_bucket.get("unknown_seconds", 0.0)) + seconds, 3)

    day_app_bucket = ensure_bucket(day_apps, app)
    increment_bucket(day_app_bucket, seconds)
    update_app_bucket_metadata(day_app_bucket, sample)
    cat_bucket = ensure_bucket(day_categories, category, sample["category_label"])
    increment_bucket(cat_bucket, seconds)

    hour_app_bucket = ensure_bucket(hour_bucket.setdefault("apps", {}), app)
    increment_bucket(hour_app_bucket, seconds)
    update_app_bucket_metadata(hour_app_bucket, sample)
    hour_cat_bucket = ensure_bucket(hour_bucket.setdefault("categories", {}), category, sample["category_label"])
    increment_bucket(hour_cat_bucket, seconds)

    app_bucket = ensure_bucket(state.setdefault("apps", {}), app)
    increment_bucket(app_bucket, seconds)
    update_app_bucket_metadata(app_bucket, sample)
    app_bucket["last_seen_at"] = now_iso()

    category_bucket = ensure_bucket(state.setdefault("categories", {}), category, sample["category_label"])
    increment_bucket(category_bucket, seconds)

    week_bucket = ensure_week_bucket(state, week_key)
    increment_bucket(week_bucket, seconds)
    if sample.get("ok", False):
        week_bucket["active_seconds"] = round(float(week_bucket.get("active_seconds", 0.0)) + seconds, 3)
    else:
        week_bucket["unknown_seconds"] = round(float(week_bucket.get("unknown_seconds", 0.0)) + seconds, 3)
    week_app_bucket = ensure_bucket(week_bucket.setdefault("apps", {}), app)
    increment_bucket(week_app_bucket, seconds)
    update_app_bucket_metadata(week_app_bucket, sample)
    week_category_bucket = ensure_bucket(week_bucket.setdefault("categories", {}), category, sample["category_label"])
    increment_bucket(week_category_bucket, seconds)


def add_elapsed(state: dict[str, Any], sample: dict[str, Any], start_ts: float, end_ts: float) -> None:
    if end_ts <= start_ts:
        return

    cursor = start_ts
    while cursor < end_ts:
        boundary = min(next_local_midnight(cursor), next_local_hour(cursor), end_ts)
        add_seconds_to_day(state, sample, boundary - cursor, cursor)
        cursor = boundary


def append_event(previous: dict[str, Any] | None, current: dict[str, Any]) -> None:
    EVENTS_PATH.parent.mkdir(parents=True, exist_ok=True)
    event = {
        "at": now_iso(),
        "from": previous,
        "to": current,
    }
    with EVENTS_PATH.open("a", encoding="utf-8") as f:
        f.write(json.dumps(event, sort_keys=True) + "\n")


def build_sample(rules: dict[str, Any]) -> dict[str, Any]:
    window = focused_window()
    app, category, category_label = resolve_app_with_process(
        window.get("class", ""),
        window.get("title", ""),
        int(window.get("pid", 0) or 0),
        rules,
    )
    return {
        "app": app,
        "category": category,
        "category_label": category_label,
        "class": window.get("class", ""),
        "title": window.get("title", ""),
        "initialClass": window.get("initialClass", ""),
        "initialTitle": window.get("initialTitle", ""),
        "pid": window.get("pid", 0),
        "address": window.get("address", ""),
        "workspace": window.get("workspace", ""),
        "ok": window.get("ok", False),
        "error": window.get("error", ""),
        "sampled_at": now_iso(),
    }


def rules_mtime(path: Path = RULES_PATH) -> float:
    try:
        return path.stat().st_mtime
    except OSError:
        return 0.0


def acquire_lock() -> Any:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    lock_file = LOCK_PATH.open("w", encoding="utf-8")
    try:
        fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        print("ScreenTime monitor is already running.", file=sys.stderr)
        sys.exit(2)
    lock_file.write(str(os.getpid()))
    lock_file.flush()
    return lock_file


def open_hyprland_event_stream() -> tuple[HyprlandEventStream | None, Path | None, str]:
    socket_path = hyprland_event_socket_path()
    if socket_path is None:
        return None, None, ""
    try:
        return HyprlandEventStream(socket_path), socket_path, ""
    except OSError as error:
        return None, socket_path, str(error)


def samples_same_window(previous: dict[str, Any], current: dict[str, Any]) -> bool:
    previous_address = normalize_window_address(previous.get("address", ""))
    current_address = normalize_window_address(current.get("address", ""))
    if previous_address or current_address:
        return previous_address == current_address
    return previous.get("app") == current.get("app") and normalize(str(previous.get("class", ""))) == normalize(str(current.get("class", "")))


def sample_changed(previous: dict[str, Any], current: dict[str, Any]) -> bool:
    return not samples_same_window(previous, current) or current.get("app") != previous.get("app")


def commit_elapsed_sample(
    state: dict[str, Any],
    sample: dict[str, Any],
    previous_wall: float,
    previous_monotonic: float,
    current_monotonic: float,
) -> None:
    elapsed = min(max(0.0, current_monotonic - previous_monotonic), MAX_SAMPLE_SECONDS)
    if elapsed <= 0:
        return
    add_elapsed(state, sample, previous_wall, previous_wall + elapsed)
    state["sample_count"] = int(state.get("sample_count", 0)) + 1


def monitor_health_payload(
    sample: dict[str, Any],
    interval: float,
    rules: dict[str, Any],
    collector_mode: str,
    last_sample_at: str,
    event_socket: Path | None,
    event_socket_error: str,
) -> dict[str, Any]:
    health = {
        "running": True,
        "last_error": sample.get("error", ""),
        "last_sample_at": last_sample_at,
        "interval_seconds": interval,
        "pid": os.getpid(),
        "rules_loaded_at": rules.get("loaded_at", ""),
        "collector_mode": collector_mode,
    }
    if event_socket is not None:
        health["event_socket"] = str(event_socket)
    if event_socket_error:
        health["event_socket_error"] = event_socket_error
    return health


def run_monitor(interval: float) -> None:
    interval = max(1.0, interval)
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    _lock_file = acquire_lock()
    state = migrate_state(load_json(STATE_PATH, empty_state()))
    rules = load_rules()
    loaded_rules_mtime = rules_mtime()
    stop = False

    def handle_stop(_signum: int, _frame: Any) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, handle_stop)
    signal.signal(signal.SIGTERM, handle_stop)

    event_stream, event_socket, event_socket_error = open_hyprland_event_stream()
    next_event_socket_retry = time.monotonic() + EVENT_SOCKET_RETRY_SECONDS
    previous_sample = build_sample(rules)
    previous_monotonic = time.monotonic()
    previous_wall = time.time()
    last_sample_at = previous_sample.get("sampled_at", "") or now_iso()
    collector_mode = "event" if event_stream is not None else "polling"
    state["current"] = previous_sample
    state["health"] = monitor_health_payload(
        previous_sample,
        interval,
        rules,
        collector_mode,
        last_sample_at,
        event_socket,
        event_socket_error,
    )
    atomic_write_json(STATE_PATH, state)
    append_event(None, previous_sample)
    next_flush_monotonic = previous_monotonic + interval

    try:
        while not stop:
            event_line = None
            waited_with_polling = False
            if event_stream is not None:
                timeout_seconds = max(0.0, next_flush_monotonic - time.monotonic())
                try:
                    event_line = event_stream.read_line(timeout_seconds)
                except OSError as error:
                    event_socket_error = str(error)
                    event_stream.close()
                    event_stream = None
                    event_socket = None
                    next_event_socket_retry = time.monotonic() + EVENT_SOCKET_RETRY_SECONDS
            else:
                waited_with_polling = True
                time.sleep(interval)
                if time.monotonic() >= next_event_socket_retry:
                    event_stream, event_socket, event_socket_error = open_hyprland_event_stream()
                    next_event_socket_retry = time.monotonic() + EVENT_SOCKET_RETRY_SECONDS

            current_rules_mtime = rules_mtime()
            rules_changed = current_rules_mtime != loaded_rules_mtime
            if rules_changed:
                rules = load_rules()
                loaded_rules_mtime = current_rules_mtime

            current_monotonic = time.monotonic()
            current_wall = time.time()
            focus_event = event_stream is not None and event_line is not None and hyprland_event_requires_sample(event_line, previous_sample)
            due_flush = current_monotonic >= next_flush_monotonic
            polling_tick = waited_with_polling or event_stream is None

            if event_line is not None and not focus_event and not due_flush and not rules_changed:
                continue

            should_resample = polling_tick or focus_event or rules_changed
            current_sample = build_sample(rules) if should_resample else previous_sample
            if focus_event and not due_flush and not rules_changed and samples_same_window(previous_sample, current_sample):
                continue
            commit_elapsed_sample(state, previous_sample, previous_wall, previous_monotonic, current_monotonic)

            if current_sample.get("error"):
                state["error_count"] = int(state.get("error_count", 0)) + 1
            if should_resample and sample_changed(previous_sample, current_sample):
                append_event(previous_sample, current_sample)

            last_sample_at = current_sample.get("sampled_at", "") if should_resample else now_iso()
            collector_mode = "event" if event_stream is not None else "polling"
            state["updated_at"] = now_iso()
            state["current"] = current_sample
            state["health"] = monitor_health_payload(
                current_sample,
                interval,
                rules,
                collector_mode,
                last_sample_at,
                event_socket,
                event_socket_error,
            )
            atomic_write_json(STATE_PATH, state)
            previous_sample = current_sample
            previous_monotonic = current_monotonic
            previous_wall = current_wall
            next_flush_monotonic = current_monotonic + interval
    finally:
        if event_stream is not None:
            event_stream.close()
        state["health"]["running"] = False
        state["updated_at"] = now_iso()
        atomic_write_json(STATE_PATH, state)


def fmt_seconds(seconds: float) -> str:
    seconds = int(round(seconds))
    hours, rem = divmod(seconds, 3600)
    minutes, secs = divmod(rem, 60)
    if hours:
        return f"{hours}h {minutes:02d}m"
    if minutes:
        return f"{minutes}m {secs:02d}s"
    return f"{secs}s"


def sorted_usage(items: dict[str, Any]) -> list[tuple[str, dict[str, Any]]]:
    return sorted(items.items(), key=lambda item: float(item[1].get("seconds", 0.0)), reverse=True)


def application_dirs() -> list[Path]:
    dirs = [Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")).expanduser() / "applications"]
    for entry in os.environ.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(":"):
        if entry:
            dirs.append(Path(entry).expanduser() / "applications")
    result = []
    seen = set()
    for path in dirs:
        key = str(path)
        if key not in seen:
            seen.add(key)
            result.append(path)
    return result


@lru_cache(maxsize=1)
def desktop_entries() -> tuple[dict[str, str], ...]:
    entries = []
    for app_dir in application_dirs():
        try:
            paths = sorted(app_dir.glob("*.desktop"))
        except OSError:
            continue
        for path in paths:
            entry = parse_desktop_entry(path)
            if entry:
                entries.append(entry)
    return tuple(entries)


def parse_desktop_entry(path: Path) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return {}

    data = {
        "id": path.name,
        "name": "",
        "exec": "",
        "icon": "",
        "noDisplay": "",
    }
    in_desktop_entry = False
    for line in lines:
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        if text.startswith("[") and text.endswith("]"):
            in_desktop_entry = text == "[Desktop Entry]"
            continue
        if not in_desktop_entry or "=" not in text:
            continue
        key, value = text.split("=", 1)
        if key in ("Name", "Exec", "Icon", "NoDisplay"):
            data_key = "noDisplay" if key == "NoDisplay" else key.lower()
            data[data_key] = value.strip()

    if data["noDisplay"].lower() == "true" or not data["icon"]:
        return {}
    return data


def desktop_icon_for_client(class_name: str, title: str) -> str:
    best_icon = ""
    best_score = 0
    cls = str(class_name or "").lower()
    text = str(title or "").lower()

    for entry in desktop_entries():
        entry_name = entry.get("name", "").lower()
        entry_id = entry.get("id", "").lower()
        entry_exec = entry.get("exec", "").lower()
        hay = f"{entry_name} {entry_id} {entry_exec}"
        score = 0

        if cls and (cls in hay or cls.removesuffix("-bin") in hay):
            score = 3
        if text and entry_name and (text in entry_name or entry_name in text):
            score = max(score, 4 if cls == "org.quickshell" else 2)
        if text and entry_name == text:
            score = max(score, 6)
        if score > 0 and "astrea-" in entry_id:
            score += 1

        if score > best_score:
            best_score = score
            best_icon = entry.get("icon", "")
    return best_icon


def client_needs_deep_icon(row: dict[str, Any]) -> bool:
    text = " ".join(
        str(row.get(key, ""))
        for key in ("class", "initialClass", "title", "last_title", "initialTitle")
    ).casefold()
    return (
        ".exe" in text
        or "wine" in text
        or "proton" in text
        or "pressure-vessel" in text
        or "steam_app_" in text
    )


def resolve_deep_icon(row: dict[str, Any]) -> str:
    if not ICON_RESOLVER_SCRIPT.exists():
        return ""

    class_name = str(row.get("class") or row.get("initialClass") or "")
    title = str(row.get("last_title") or row.get("title") or row.get("id") or "App")
    payload = {
        "name": title,
        "title": title,
        "pid": row.get("pid", 0),
        "application.process.id": row.get("pid", 0),
        "application.name": title,
        "application.process.binary": class_name,
        "application.id": class_name,
        "node.name": title,
        "window.class": class_name,
        "window.initial_class": row.get("initialClass", ""),
        "window.title": title,
        "window.initial_title": row.get("initialTitle", ""),
    }
    try:
        proc = subprocess.run(
            ["python3", str(ICON_RESOLVER_SCRIPT), "resolve", json.dumps(payload)],
            check=False,
            capture_output=True,
            text=True,
            timeout=2.0,
        )
    except (FileNotFoundError, subprocess.SubprocessError):
        return ""
    if proc.returncode != 0 or not proc.stdout.strip():
        return ""
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return ""
    icon_name = str(data.get("icon_name", ""))
    icon_path = str(data.get("icon", ""))
    if icon_path and icon_name != "audio-x-generic":
        return icon_path
    if icon_name and icon_name != "audio-x-generic":
        return icon_name
    return ""


def alt_tab_icon_name_for_client(class_name: str, title: str) -> str:
    raw_class = str(class_name or "").strip()
    cls = raw_class.lower()
    text = str(title or "").lower()

    if cls == "org.vinegarhq.sober":
        return "org.vinegarhq.Sober"
    if "zen" in cls:
        return "zen-browser"
    if "kitty" in cls:
        return "kitty"
    if "code" in cls or "cursor" in cls:
        return "visual-studio-code"
    if "spotify" in cls:
        return "spotify"
    if "discord" in cls:
        return "discord"

    steam_game = re.match(r"^steam_app_(\d+)$", cls)
    if steam_game:
        return "steam_icon_" + steam_game.group(1)
    if cls == "steam_app_default":
        return ""
    if "steam" in cls:
        return "steam"

    desktop_icon = desktop_icon_for_client(cls, text)
    if desktop_icon:
        return desktop_icon
    if "finder" in text:
        return "folder"
    if "settings" in text or "configura" in text:
        return "preferences-system"
    if "weather" in text or "clima" in text:
        return "weather-clear"
    if "screen" in text and "time" in text:
        return "preferences-system-time"
    if "org.quickshell" in cls:
        return "application-x-executable"
    return "" if "." in raw_class else cls


def annotate_app_icon(row: dict[str, Any]) -> None:
    if row.get("icon") or row.get("iconSource") or row.get("astreaIcon") or row.get("astreaIconName"):
        return

    icon = resolve_deep_icon(row) if client_needs_deep_icon(row) else ""
    if not icon:
        class_name = str(row.get("class") or row.get("initialClass") or row.get("id") or "")
        title = str(row.get("last_title") or row.get("title") or row.get("name") or row.get("id") or "")
        icon = alt_tab_icon_name_for_client(class_name, title)

    if icon:
        row["icon"] = icon
    elif client_needs_deep_icon(row):
        row["hideIconFallback"] = True


def usage_rows(
    items: dict[str, Any],
    limit: int,
    total_seconds: float | None = None,
    metadata_items: dict[str, Any] | None = None,
    include_app_icons: bool = False,
) -> list[dict[str, Any]]:
    rows = []
    for key, bucket in sorted_usage(items)[:limit]:
        row = dict(bucket)
        metadata_bucket = (metadata_items or {}).get(key, {})
        if isinstance(metadata_bucket, dict):
            for metadata_key in APP_METADATA_KEYS:
                if not row.get(metadata_key) and metadata_bucket.get(metadata_key):
                    row[metadata_key] = metadata_bucket[metadata_key]
        row["id"] = key
        row["seconds"] = round(float(bucket.get("seconds", 0.0)), 3)
        row["duration"] = fmt_seconds(row["seconds"])
        if total_seconds is not None:
            row["percent"] = round(row["seconds"] / max(1.0, total_seconds), 6)
        if include_app_icons:
            annotate_app_icon(row)
        rows.append(row)
    return rows


def parse_day_key(value: str) -> date:
    return date.fromisoformat(value)


def date_key(value: date) -> str:
    return value.strftime("%Y-%m-%d")


def formatted_day(value: date) -> str:
    label = f"{value.day} de {MONTH_NAMES_PT[value.month - 1]}"
    if date_key(value) == today_key():
        return f"Hoje, {label}"
    return label


def formatted_week_range(start: date) -> str:
    end = week_end_for(start)
    if start.month == end.month and start.year == end.year:
        label = f"{start.day}-{end.day} de {MONTH_NAMES_PT[start.month - 1]}"
    elif start.year == end.year:
        label = f"{start.day} de {MONTH_NAMES_PT[start.month - 1]} - {end.day} de {MONTH_NAMES_PT[end.month - 1]}"
    else:
        label = f"{start.day} de {MONTH_NAMES_PT[start.month - 1]} de {start.year} - {end.day} de {MONTH_NAMES_PT[end.month - 1]} de {end.year}"
    if week_key_for_date(date.today()) == date_key(start):
        return f"Esta semana, {label}"
    return label


def formatted_generated_at() -> str:
    return f"Atualizado hoje as {datetime.now().strftime('%H:%M')}"


def health_label(health: dict[str, Any]) -> str:
    if health.get("stale", False):
        return "Coletor sem atualizacao"
    if str(health.get("last_error", "")).strip():
        return "Coletor degradado"
    if health.get("running", False):
        return "Coletor ativo"
    return "Coletor parado"


def top_category_from(categories: dict[str, Any]) -> tuple[str, str]:
    if not categories:
        return "", ""
    category_id, bucket = sorted_usage(categories)[0]
    return category_id, str(bucket.get("label", category_id))


def hourly_rows(day_root: dict[str, Any]) -> list[dict[str, Any]]:
    hourly = day_root.get("hourly", {})
    if not isinstance(hourly, dict):
        hourly = {}

    rows = []
    for hour in range(24):
        bucket = hourly.get(str(hour), {})
        if not isinstance(bucket, dict):
            bucket = {}
        seconds = round(float(bucket.get("seconds", 0.0)), 3)
        categories = usage_rows(bucket.get("categories", {}), 8, seconds)
        top_category, top_category_label = top_category_from(bucket.get("categories", {}))
        rows.append(
            {
                "hour": hour,
                "label": f"{hour:02d}:00",
                "seconds": seconds,
                "duration": fmt_seconds(seconds),
                "active_seconds": round(float(bucket.get("active_seconds", 0.0)), 3),
                "unknown_seconds": round(float(bucket.get("unknown_seconds", 0.0)), 3),
                "categories": categories,
                "top_category": top_category,
                "top_category_label": top_category_label,
            }
        )
    return rows


def merge_usage(target: dict[str, Any], source: dict[str, Any]) -> None:
    for item_id, raw_bucket in source.items():
        if not isinstance(raw_bucket, dict):
            continue
        bucket = ensure_bucket(target, str(item_id), raw_bucket.get("label"))
        increment_bucket(bucket, float(raw_bucket.get("seconds", 0.0)))
        for key in APP_METADATA_KEYS:
            if raw_bucket.get(key):
                bucket[key] = raw_bucket[key]


def week_history(state: dict[str, Any], selected_week_start: date) -> list[dict[str, Any]]:
    days = state.get("days", {})
    weeks = state.get("weeks", {})
    starts: set[date] = {selected_week_start, week_start_for(date.today())}

    if isinstance(weeks, dict):
        for raw_week_key in weeks.keys():
            try:
                starts.add(week_start_for(parse_day_key(str(raw_week_key))))
            except ValueError:
                continue

    if isinstance(days, dict):
        for raw_day_key in days.keys():
            try:
                starts.add(week_start_for(parse_day_key(str(raw_day_key))))
            except ValueError:
                continue

    if not starts:
        starts.add(selected_week_start)

    first = min(starts)
    last = max(starts)
    rows = []
    cursor = first
    while cursor <= last:
        key = date_key(cursor)
        bucket = weeks.get(key, {}) if isinstance(weeks, dict) else {}
        if not isinstance(bucket, dict):
            bucket = {}
        seconds = round(float(bucket.get("seconds", 0.0)), 3)
        rows.append(
            {
                "start": key,
                "end": date_key(week_end_for(cursor)),
                "label": formatted_week_range(cursor),
                "seconds": seconds,
                "duration": fmt_seconds(seconds),
                "selected": cursor == selected_week_start,
                "has_data": seconds > 0,
            }
        )
        cursor += timedelta(days=7)
    return rows


def week_snapshot(state: dict[str, Any], selected_week_start: date, limit: int, history: list[dict[str, Any]] | None = None) -> dict[str, Any]:
    days = state.get("days", {})
    weeks = state.get("weeks", {})
    start = week_start_for(selected_week_start)
    end = week_end_for(start)
    start_key = date_key(start)
    week_root = weeks.get(start_key, {}) if isinstance(weeks, dict) else {}
    if not isinstance(week_root, dict):
        week_root = {}
    use_week_root = bool(float(week_root.get("seconds", 0.0)) or week_root.get("apps") or week_root.get("categories"))
    week_apps: dict[str, Any] = dict(week_root.get("apps", {}) or {}) if use_week_root else {}
    week_categories: dict[str, Any] = dict(week_root.get("categories", {}) or {}) if use_week_root else {}
    week_days = []

    for offset in range(7):
        current_day = start + timedelta(days=offset)
        key = date_key(current_day)
        day_root = days.get(key, {})
        if not isinstance(day_root, dict):
            day_root = {}
        seconds = round(float(day_root.get("seconds", 0.0)), 3)
        if not use_week_root:
            merge_usage(week_apps, day_root.get("apps", {}))
            merge_usage(week_categories, day_root.get("categories", {}))
        week_days.append(
            {
                "date": key,
                "label": WEEKDAY_SHORT_PT[current_day.weekday()],
                "short_label": WEEKDAY_SHORT_PT[current_day.weekday()],
                "seconds": seconds,
                "duration": fmt_seconds(seconds),
                "categories": usage_rows(day_root.get("categories", {}), limit, seconds),
            }
        )

    week_total = round(float(week_root.get("seconds", 0.0)) or sum(day_row["seconds"] for day_row in week_days), 3)
    history_rows = history if history is not None else week_history(state, start)
    selected_index = next((index for index, row in enumerate(history_rows) if row.get("start") == start_key), -1)
    return {
        "start": start_key,
        "end": date_key(end),
        "label": formatted_week_range(start),
        "seconds": week_total,
        "duration": fmt_seconds(week_total),
        "days": week_days,
        "apps": usage_rows(week_apps, limit, week_total, state.get("apps", {}), True),
        "categories": usage_rows(week_categories, limit, week_total),
        "history_index": selected_index,
        "history_count": len(history_rows),
        "previous_start": history_rows[selected_index - 1]["start"] if selected_index > 0 else "",
        "next_start": history_rows[selected_index + 1]["start"] if 0 <= selected_index < len(history_rows) - 1 else "",
    }


def snapshot(day: str | None, limit: int, week: str | None = None) -> dict[str, Any]:
    state = migrate_state(load_json(STATE_PATH, empty_state()))
    selected_day = day or today_key()
    selected_date = parse_day_key(selected_day)
    selected_week_start = week_start_for(parse_day_key(week)) if week else week_start_for(selected_date)
    week_rows = week_history(state, selected_week_start)
    day_root = state.get("days", {}).get(selected_day, {"seconds": 0.0, "apps": {}, "categories": {}})
    day_seconds = round(float(day_root.get("seconds", 0.0)), 3)
    day_apps = usage_rows(day_root.get("apps", {}), limit, day_seconds, state.get("apps", {}), True)
    day_categories = usage_rows(day_root.get("categories", {}), limit, day_seconds)
    health = state.get("health", {})
    health = effective_health(health)
    service = service_status()
    return {
        "schema_version": int(state.get("schema_version", STATE_SCHEMA_VERSION)),
        "generated_at": now_iso(),
        "state_path": str(STATE_PATH),
        "events_path": str(EVENTS_PATH),
        "rules_path": str(RULES_PATH),
        "selected_day": selected_day,
        "selected_week": date_key(selected_week_start),
        "current": state.get("current", {}),
        "health": health,
        "service": service,
        "display": {
            "selected_day": formatted_day(selected_date),
            "selected_week": formatted_week_range(selected_week_start),
            "generated_at": formatted_generated_at(),
            "health": health_label(health),
        },
        "totals": {
            "seconds": round(float(state.get("total_seconds", 0.0)), 3),
            "active_seconds": round(float(state.get("active_seconds", 0.0)), 3),
            "unknown_seconds": round(float(state.get("unknown_seconds", 0.0)), 3),
            "duration": fmt_seconds(float(state.get("total_seconds", 0.0))),
            "active_duration": fmt_seconds(float(state.get("active_seconds", 0.0))),
            "unknown_duration": fmt_seconds(float(state.get("unknown_seconds", 0.0))),
        },
        "day": {
            "seconds": day_seconds,
            "active_seconds": round(float(day_root.get("active_seconds", 0.0)), 3),
            "unknown_seconds": round(float(day_root.get("unknown_seconds", 0.0)), 3),
            "duration": fmt_seconds(float(day_root.get("seconds", 0.0))),
            "active_duration": fmt_seconds(float(day_root.get("active_seconds", 0.0))),
            "unknown_duration": fmt_seconds(float(day_root.get("unknown_seconds", 0.0))),
            "categories": day_categories,
            "apps": day_apps,
            "top_categories": day_categories,
            "top_apps": day_apps,
            "hourly": hourly_rows(day_root),
        },
        "week": week_snapshot(state, selected_week_start, limit, week_rows),
        "weeks": week_rows,
        "all_time": {
            "categories": usage_rows(state.get("categories", {}), limit),
            "apps": usage_rows(state.get("apps", {}), limit, include_app_icons=True),
        },
        "sample_count": int(state.get("sample_count", 0)),
        "error_count": int(state.get("error_count", 0)),
    }


def status_summary() -> dict[str, Any]:
    state = migrate_state(load_json(STATE_PATH, empty_state()))
    health = effective_health(state.get("health", {}))
    service = service_status()
    return {
        "schema_version": int(state.get("schema_version", STATE_SCHEMA_VERSION)),
        "state_path": str(STATE_PATH),
        "events_path": str(EVENTS_PATH),
        "rules_path": str(RULES_PATH),
        "lock_path": str(LOCK_PATH),
        "sample_count": int(state.get("sample_count", 0)),
        "error_count": int(state.get("error_count", 0)),
        "current": state.get("current", {}),
        "health": health,
        "service": service,
        "display": {
            "health": health_label(health),
            "service": service.get("display", {}).get("state", ""),
            "generated_at": formatted_generated_at(),
        },
    }


def print_snapshot_json(day: str | None, week: str | None, limit: int) -> None:
    print(json.dumps(snapshot(day, limit, week), indent=2, sort_keys=True))


def print_status(as_json: bool) -> None:
    status = status_summary()
    if as_json:
        print(json.dumps(status, indent=2, sort_keys=True))
        return

    health = status["health"]
    current = status.get("current", {})
    print("ScreenTime backend")
    print(f"Status: {status['display']['health']}")
    print(f"Schema: {status['schema_version']}")
    print(f"Samples: {status['sample_count']}  Errors: {status['error_count']}")
    print(f"PID: {health.get('pid', '-')}  PID visible: {health.get('pid_alive', False)}")
    print(f"Service: {status.get('service', {}).get('unit', SERVICE_NAME)}  {status.get('display', {}).get('service', '')}")
    print(f"Last sample: {health.get('last_sample_at', '-')}")
    if current:
        print(f"Focused: {current.get('app', 'unknown')} ({current.get('category', 'other')})")
    if health.get("last_error"):
        print(f"Last error: {health.get('last_error')}")
    print(f"State: {status['state_path']}")
    print(f"Events: {status['events_path']}")
    print(f"Rules: {status['rules_path']}")


def print_service(action: str, as_json: bool) -> int:
    try:
        result = service_action(action)
    except (RuntimeError, ValueError, OSError) as error:
        result = service_status()
        result["ok"] = False
        result["action"] = action
        result["last_error"] = str(error)
        if as_json:
            print(json.dumps(result, indent=2, sort_keys=True))
        else:
            print(f"{SERVICE_NAME}: {result.get('display', {}).get('state', 'Erro')}")
            print(f"Error: {error}")
        return 1

    if as_json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"{result['unit']}: {result.get('display', {}).get('state', '')}")
    return 0


def print_report(day: str | None, limit: int) -> None:
    state = migrate_state(load_json(STATE_PATH, empty_state()))
    if day:
        root = state.get("days", {}).get(day, {})
        title = f"ScreenTime {day}"
    else:
        root = state
        title = "ScreenTime total"

    print(title)
    print(f"Total: {fmt_seconds(float(root.get('seconds', state.get('total_seconds', 0.0))))}")
    current = state.get("current", {})
    if current:
        print(f"Focused now: {current.get('app', 'unknown')} ({current.get('category', 'other')})")

    print("\nCategories")
    for category_id, bucket in sorted_usage(root.get("categories", {}))[:limit]:
        label = bucket.get("label", category_id)
        print(f"  {label}: {fmt_seconds(float(bucket.get('seconds', 0.0)))}")

    print("\nApps")
    for app_id, bucket in sorted_usage(root.get("apps", {}))[:limit]:
        category = bucket.get("category", "")
        suffix = f" [{category}]" if category else ""
        print(f"  {app_id}{suffix}: {fmt_seconds(float(bucket.get('seconds', 0.0)))}")


def reset_state() -> None:
    atomic_write_json(STATE_PATH, empty_state())
    if EVENTS_PATH.exists():
        EVENTS_PATH.unlink()


def main() -> int:
    parser = argparse.ArgumentParser(description="Track focused app usage on Hyprland.")
    sub = parser.add_subparsers(dest="command", required=True)

    monitor = sub.add_parser("monitor", help="Run the sampler loop.")
    monitor.add_argument("--interval", type=float, default=DEFAULT_INTERVAL, help="Sample interval in seconds.")

    report = sub.add_parser("report", help="Print a human-readable usage report.")
    report.add_argument("--day", help="Report a specific local day, e.g. 2026-05-01.")
    report.add_argument("--limit", type=int, default=12)

    snapshot_parser = sub.add_parser("snapshot", help="Print a UI-friendly JSON snapshot.")
    snapshot_parser.add_argument("--day", help="Snapshot a specific local day, e.g. 2026-05-01.")
    snapshot_parser.add_argument("--week", help="Snapshot a calendar week by start date, e.g. 2026-06-01.")
    snapshot_parser.add_argument("--limit", type=int, default=12)
    snapshot_parser.add_argument("--json", action="store_true", help="Kept for QML call clarity; snapshot always prints JSON.")

    status_parser = sub.add_parser("status", help="Print collector health and backend paths.")
    status_parser.add_argument("--json", action="store_true", help="Print status as JSON.")

    service_parser = sub.add_parser("service", help="Control the Astrea ScreenTime user service.")
    service_parser.add_argument("action", choices=("status", "enable", "disable", "start", "stop", "restart"))
    service_parser.add_argument("--json", action="store_true", help="Print service state as JSON.")

    sub.add_parser("path", help="Print state file paths.")
    sub.add_parser("reset", help="Reset collected state.")

    args = parser.parse_args()
    if args.command == "monitor":
        run_monitor(max(1.0, args.interval))
    elif args.command == "report":
        print_report(args.day, args.limit)
    elif args.command == "snapshot":
        print_snapshot_json(args.day, args.week, args.limit)
    elif args.command == "status":
        print_status(args.json)
    elif args.command == "service":
        return print_service(args.action, args.json)
    elif args.command == "path":
        print(STATE_PATH)
        print(EVENTS_PATH)
        print(RULES_PATH)
    elif args.command == "reset":
        reset_state()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
