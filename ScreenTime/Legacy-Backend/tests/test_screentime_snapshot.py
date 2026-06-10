import os
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

MODULE_PATH = Path(__file__).resolve().parents[1] / "screentime.py"
SPEC = importlib.util.spec_from_file_location("legacy_screentime", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"Unable to load {MODULE_PATH}")
screentime = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = screentime
SPEC.loader.exec_module(screentime)


class SnapshotContractTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self.tmp.name)
        self.old_state_path = screentime.STATE_PATH
        self.old_events_path = screentime.EVENTS_PATH
        self.old_rules_path = screentime.RULES_PATH
        self.old_service_unit_source = getattr(screentime, "SERVICE_UNIT_SOURCE", None)
        self.old_user_unit_dir = getattr(screentime, "USER_UNIT_DIR", None)
        screentime.STATE_PATH = self.tmp_path / "usage.json"
        screentime.EVENTS_PATH = self.tmp_path / "events.jsonl"
        screentime.RULES_PATH = self.tmp_path / "app_rules.json"
        if self.old_service_unit_source is not None:
            screentime.SERVICE_UNIT_SOURCE = self.tmp_path / "astrea-screentimed-legacy.service"
            screentime.SERVICE_UNIT_SOURCE.write_text("[Unit]\nDescription=Astrea ScreenTime\n", encoding="utf-8")
        if self.old_user_unit_dir is not None:
            screentime.USER_UNIT_DIR = self.tmp_path / "systemd" / "user"

    def tearDown(self):
        screentime.STATE_PATH = self.old_state_path
        screentime.EVENTS_PATH = self.old_events_path
        screentime.RULES_PATH = self.old_rules_path
        if self.old_service_unit_source is not None:
            screentime.SERVICE_UNIT_SOURCE = self.old_service_unit_source
        if self.old_user_unit_dir is not None:
            screentime.USER_UNIT_DIR = self.old_user_unit_dir
        self.tmp.cleanup()

    def write_state(self):
        state = screentime.empty_state()
        state["created_at"] = "2026-06-01T09:00:00+00:00"
        state["updated_at"] = "2026-06-04T22:25:00+00:00"
        state["sample_count"] = 4
        state["error_count"] = 0
        state["health"] = {
            "running": True,
            "last_error": "",
            "last_sample_at": "2026-06-04T22:24:55+00:00",
            "pid": os.getpid(),
        }
        state["current"] = {
            "app": "terminal",
            "category": "development",
            "category_label": "Development",
            "class": "kitty",
            "title": "agony",
            "ok": True,
        }

        samples = [
            ("2026-06-03T10:15:00", "terminal", "development", "Development", 1800),
            ("2026-06-04T09:00:00", "terminal", "development", "Development", 3600),
            ("2026-06-04T10:00:00", "zen", "browser", "Browser", 1800),
            ("2026-06-04T10:30:00", "unknown", "other", "Other", 300),
        ]
        for at, app, category, label, seconds in samples:
            sample = {
                "app": app,
                "category": category,
                "category_label": label,
                "class": app,
                "title": app,
                "ok": app != "unknown",
            }
            ts = screentime.datetime.fromisoformat(at).timestamp()
            screentime.add_seconds_to_day(state, sample, seconds, ts)

        screentime.atomic_write_json(screentime.STATE_PATH, state)

    def test_snapshot_exposes_hourly_weekly_and_display_contract(self):
        self.write_state()

        data = screentime.snapshot("2026-06-04", 5)

        self.assertIn("hourly", data["day"])
        self.assertEqual(24, len(data["day"]["hourly"]))
        self.assertEqual(3600, data["day"]["hourly"][9]["seconds"])
        self.assertEqual(2100, data["day"]["hourly"][10]["seconds"])
        self.assertEqual("1h 00m", data["day"]["hourly"][9]["duration"])
        self.assertEqual("development", data["day"]["hourly"][9]["top_category"])

        self.assertIn("top_apps", data["day"])
        self.assertEqual("terminal", data["day"]["top_apps"][0]["id"])
        self.assertEqual("terminal", data["day"]["top_apps"][0]["last_title"])
        self.assertAlmostEqual(3600 / 5700, data["day"]["top_apps"][0]["percent"], places=4)
        self.assertEqual("development", data["day"]["top_categories"][0]["id"])

        self.assertIn("week", data)
        self.assertEqual(7, len(data["week"]["days"]))
        self.assertEqual("2026-06-01", data["week"]["start"])
        self.assertEqual("2026-06-07", data["week"]["end"])
        self.assertEqual("2026-06-04", data["week"]["days"][3]["date"])
        self.assertEqual(5700, data["week"]["days"][3]["seconds"])
        self.assertEqual("terminal", data["week"]["apps"][0]["id"])
        self.assertEqual("development", data["week"]["categories"][0]["id"])

        self.assertIn("display", data)
        self.assertEqual(4, data["schema_version"])
        self.assertEqual("Hoje, 4 de junho", data["display"]["selected_day"])
        self.assertTrue(data["display"]["generated_at"].startswith("Atualizado"))
        self.assertEqual("Coletor ativo", data["display"]["health"])

    def test_snapshot_marks_stale_or_dead_collector_in_effective_health(self):
        state = screentime.empty_state()
        state["schema_version"] = 2
        state["health"] = {
            "running": True,
            "last_error": "",
            "last_sample_at": "2000-01-01T00:00:00+00:00",
            "interval_seconds": 5,
            "pid": 99999999,
        }
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        data = screentime.snapshot("2026-06-04", 5)

        self.assertFalse(data["health"]["running"])
        self.assertTrue(data["health"]["stale"])
        self.assertFalse(data["health"]["pid_alive"])
        self.assertGreater(data["health"]["sample_age_seconds"], 60)
        self.assertEqual("Coletor sem atualizacao", data["display"]["health"])

    def test_snapshot_hydrates_app_metadata_from_global_bucket(self):
        state = screentime.empty_state()
        state["days"]["2026-06-04"] = {
            "seconds": 120.0,
            "active_seconds": 120.0,
            "unknown_seconds": 0.0,
            "apps": {
                "terminal": {
                    "category": "development",
                    "class": "kitty",
                    "seconds": 120.0,
                },
            },
            "categories": {
                "development": {
                    "label": "Development",
                    "seconds": 120.0,
                },
            },
        }
        state["apps"]["terminal"] = {
            "category": "development",
            "class": "kitty",
            "last_seen_at": "2026-06-04T12:00:00+00:00",
            "last_title": "agony terminal",
            "seconds": 300.0,
        }
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        data = screentime.snapshot("2026-06-04", 5)

        self.assertEqual("agony terminal", data["day"]["top_apps"][0]["last_title"])
        self.assertEqual("agony terminal", data["week"]["apps"][0]["last_title"])

    def test_snapshot_adds_alt_tab_icon_metadata_to_app_rows(self):
        state = screentime.empty_state()
        state["days"]["2026-06-04"] = {
            "seconds": 120.0,
            "active_seconds": 120.0,
            "unknown_seconds": 0.0,
            "apps": {
                "screentime": {
                    "category": "utilities",
                    "class": "org.quickshell",
                    "last_title": "ScreenTime",
                    "seconds": 120.0,
                },
            },
            "categories": {
                "utilities": {
                    "label": "Utilities",
                    "seconds": 120.0,
                },
            },
        }
        state["apps"]["screentime"] = {
            "category": "utilities",
            "class": "org.quickshell",
            "last_title": "ScreenTime",
            "seconds": 120.0,
        }
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        data = screentime.snapshot("2026-06-04", 5)

        self.assertEqual("preferences-system-time", data["day"]["top_apps"][0]["icon"])
        self.assertEqual("preferences-system-time", data["week"]["apps"][0]["icon"])

    def test_snapshot_exposes_week_history_and_selected_calendar_week(self):
        state = screentime.empty_state()
        state["schema_version"] = 3
        state.pop("weeks", None)

        samples = [
            ("2026-05-29T14:00:00", "terminal", "development", "Development", 600),
            ("2026-06-04T09:00:00", "zen", "browser", "Browser", 3600),
        ]
        for at, app, category, label, seconds in samples:
            sample = {
                "app": app,
                "category": category,
                "category_label": label,
                "class": app,
                "title": app,
                "ok": True,
            }
            ts = screentime.datetime.fromisoformat(at).timestamp()
            screentime.add_seconds_to_day(state, sample, seconds, ts)

        self.assertIn("weeks", state)
        self.assertEqual(600, state["weeks"]["2026-05-25"]["seconds"])
        self.assertEqual(3600, state["weeks"]["2026-06-01"]["seconds"])
        state.pop("weeks", None)
        migrated = screentime.migrate_state(state)
        self.assertIn("weeks", migrated)
        self.assertEqual(600, migrated["weeks"]["2026-05-25"]["seconds"])
        self.assertEqual(3600, migrated["weeks"]["2026-06-01"]["seconds"])
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        data = screentime.snapshot("2026-06-04", 5, "2026-05-25")

        self.assertEqual(4, data["schema_version"])
        self.assertEqual("2026-05-25", data["selected_week"])
        self.assertEqual("2026-05-25", data["week"]["start"])
        self.assertEqual("2026-05-31", data["week"]["end"])
        self.assertEqual(600, data["week"]["seconds"])
        self.assertEqual("terminal", data["week"]["apps"][0]["id"])
        self.assertEqual(["2026-05-25", "2026-06-01"], [row["start"] for row in data["weeks"]])
        self.assertEqual("", data["week"]["previous_start"])
        self.assertEqual("2026-06-01", data["week"]["next_start"])
        self.assertTrue(data["display"]["selected_week"])

    def test_status_summary_exposes_backend_paths_and_health(self):
        state = screentime.empty_state()
        state["health"] = {
            "running": True,
            "last_error": "",
            "last_sample_at": "2000-01-01T00:00:00+00:00",
            "interval_seconds": 5,
            "pid": 99999999,
        }
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        summary = screentime.status_summary()

        self.assertEqual(str(screentime.STATE_PATH), summary["state_path"])
        self.assertEqual(str(screentime.EVENTS_PATH), summary["events_path"])
        self.assertEqual(str(screentime.RULES_PATH), summary["rules_path"])
        self.assertEqual(4, summary["schema_version"])
        self.assertFalse(summary["health"]["running"])
        self.assertTrue(summary["health"]["stale"])
        self.assertEqual("Coletor sem atualizacao", summary["display"]["health"])

    def test_resolve_app_classifies_windows_executables_as_games(self):
        rules = {
            "categories": {
                "games": {"label": "Games", "apps": {}},
                "other": {"label": "Other", "apps": {}},
            }
        }

        self.assertEqual(
            ("r.e.p.o.exe", "games", "Games"),
            screentime.resolve_app("R.E.P.O.exe", "R.E.P.O.", rules),
        )
        self.assertEqual(
            ("steam_app_3241660", "games", "Games"),
            screentime.resolve_app("steam_app_3241660", "R.E.P.O.", rules),
        )
        self.assertTrue(
            screentime.process_tokens_look_like_windows_game(
                ["pressure-vessel", "/home/agony/.steam/steamapps/common/Repo/R.E.P.O.exe"]
            )
        )
        self.assertFalse(
            screentime.process_tokens_look_like_windows_game(["/usr/bin/java", "notes.jar"])
        )

    def test_recent_samples_keep_collector_active_when_pid_check_is_unavailable(self):
        state = screentime.empty_state()
        state["health"] = {
            "running": True,
            "last_error": "",
            "last_sample_at": screentime.now_iso(),
            "interval_seconds": 5,
            "pid": 99999999,
        }
        screentime.atomic_write_json(screentime.STATE_PATH, state)

        summary = screentime.status_summary()

        self.assertTrue(summary["health"]["running"])
        self.assertFalse(summary["health"]["stale"])
        self.assertFalse(summary["health"]["pid_alive"])
        self.assertEqual("Coletor ativo", summary["display"]["health"])

    def test_hyprland_event_socket_path_uses_signature_runtime(self):
        socket_dir = self.tmp_path / "hypr" / "sig-a"
        socket_dir.mkdir(parents=True)
        socket_path = socket_dir / ".socket2.sock"
        socket_path.touch()

        path = screentime.hyprland_event_socket_path(
            {
                "XDG_RUNTIME_DIR": str(self.tmp_path),
                "HYPRLAND_INSTANCE_SIGNATURE": "sig-a",
            }
        )

        self.assertEqual(socket_path, path)

    def test_hyprland_event_filter_tracks_focus_related_events(self):
        self.assertTrue(screentime.hyprland_event_changes_focus("activewindow>>kitty,agony"))
        self.assertTrue(screentime.hyprland_event_changes_focus("activewindowv2>>0x123"))
        self.assertTrue(screentime.hyprland_event_changes_focus("workspace>>2"))
        self.assertFalse(screentime.hyprland_event_changes_focus("screencast>>1"))
        self.assertFalse(screentime.hyprland_event_changes_focus(""))

    def test_hyprland_event_resample_ignores_same_window_title_noise(self):
        sample = {"address": "0xabc", "class": "kitty"}

        self.assertFalse(screentime.hyprland_event_requires_sample("activewindowv2>>abc", sample))
        self.assertFalse(screentime.hyprland_event_requires_sample("activewindow>>kitty,spinner", sample))
        self.assertFalse(screentime.hyprland_event_requires_sample("windowtitle>>abc", sample))
        self.assertTrue(screentime.hyprland_event_requires_sample("activewindowv2>>def", sample))
        self.assertTrue(screentime.hyprland_event_requires_sample("activewindow>>zen,ChatGPT", sample))
        self.assertTrue(screentime.hyprland_event_requires_sample("closewindow>>abc", sample))

    def test_sample_changed_ignores_title_only_changes(self):
        previous = {"address": "0xabc", "app": "terminal", "class": "kitty", "title": "old"}
        current = {"address": "abc", "app": "terminal", "class": "kitty", "title": "new"}
        next_app = {"address": "def", "app": "zen", "class": "zen", "title": "ChatGPT"}

        self.assertTrue(screentime.samples_same_window(previous, current))
        self.assertFalse(screentime.sample_changed(previous, current))
        self.assertFalse(screentime.samples_same_window(previous, next_app))
        self.assertTrue(screentime.sample_changed(previous, next_app))

    def test_service_status_uses_astrea_unit_name(self):
        def fake_run(args, **_kwargs):
            self.assertEqual(["systemctl", "--user", "show", "astrea-screentimed-legacy.service"], args[:4])
            return subprocess.CompletedProcess(
                args,
                0,
                stdout="\n".join(
                    [
                        "LoadState=loaded",
                        "ActiveState=active",
                        "SubState=running",
                        "UnitFileState=enabled",
                        f"FragmentPath={screentime.USER_UNIT_DIR / 'astrea-screentimed-legacy.service'}",
                    ]
                ),
                stderr="",
            )

        with patch.object(screentime.subprocess, "run", fake_run):
            status = screentime.service_status()

        self.assertEqual("astrea-screentimed-legacy.service", status["unit"])
        self.assertTrue(status["installed"])
        self.assertTrue(status["active"])
        self.assertTrue(status["enabled"])
        self.assertEqual("Ativo", status["display"]["state"])

    def test_service_enable_installs_astrea_unit_and_disables_legacy_unit(self):
        calls = []

        def fake_run(args, **_kwargs):
            calls.append(args)
            if args[:3] == ["systemctl", "--user", "show"]:
                return subprocess.CompletedProcess(
                    args,
                    0,
                    stdout="\n".join(
                        [
                            "LoadState=loaded",
                            "ActiveState=active",
                            "SubState=running",
                            "UnitFileState=enabled",
                            f"FragmentPath={screentime.USER_UNIT_DIR / 'astrea-screentimed-legacy.service'}",
                        ]
                    ),
                    stderr="",
                )
            return subprocess.CompletedProcess(args, 0, stdout="", stderr="")

        legacy_link = screentime.USER_UNIT_DIR / "bench-screentime.service"
        screentime.USER_UNIT_DIR.mkdir(parents=True)
        legacy_link.symlink_to(screentime.SERVICE_UNIT_SOURCE)

        with patch.object(screentime.subprocess, "run", fake_run):
            result = screentime.service_action("enable")

        unit_link = screentime.USER_UNIT_DIR / "astrea-screentimed-legacy.service"
        self.assertTrue(unit_link.is_symlink())
        self.assertFalse(legacy_link.exists())
        self.assertEqual("astrea-screentimed-legacy.service", result["unit"])
        self.assertTrue(result["enabled"])
        self.assertIn(["systemctl", "--user", "disable", "--now", "bench-screentime.service"], calls)
        self.assertIn(["systemctl", "--user", "enable", "--now", "astrea-screentimed-legacy.service"], calls)


if __name__ == "__main__":
    unittest.main()
