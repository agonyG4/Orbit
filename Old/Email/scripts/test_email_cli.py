#!/usr/bin/env python3
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
BACKEND = ROOT / "backend"
sys.path.insert(0, str(BACKEND))


class EmailCliTest(unittest.TestCase):
    def test_status_is_public_json_contract_without_credentials(self):
        from astrea_email import cli, gmail

        with tempfile.TemporaryDirectory() as temp_dir:
            gmail.DEFAULT_CLIENT_SECRET = pathlib.Path(temp_dir) / "gmail_client_secret.json"
            gmail.TOKEN_PATH = pathlib.Path(temp_dir) / "gmail_token.json"
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["status"])

        payload = json.loads(output.getvalue())
        self.assertEqual(code, 0)
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["provider"], "gmail")
        self.assertFalse(payload["configured"])
        self.assertFalse(payload["authenticated"])
        self.assertEqual(payload["messages"], [])

    def test_gmail_provider_prefix_remains_supported(self):
        from astrea_email import cli, gmail

        with tempfile.TemporaryDirectory() as temp_dir:
            gmail.DEFAULT_CLIENT_SECRET = pathlib.Path(temp_dir) / "gmail_client_secret.json"
            gmail.TOKEN_PATH = pathlib.Path(temp_dir) / "gmail_token.json"
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["gmail", "status"])

        payload = json.loads(output.getvalue())
        self.assertEqual(code, 0)
        self.assertEqual(payload["provider"], "gmail")

    def test_view_command_uses_webengine_viewer_contract(self):
        from astrea_email import cli, gmail

        original_open_message_viewer = gmail.open_message_viewer
        try:
            calls = []

            def fake_open_message_viewer(message_id, load_remote_images=True, geometry=None):
                calls.append((message_id, load_remote_images, geometry))
                return {
                    "ok": True,
                    "provider": "gmail",
                    "messageId": message_id,
                    "htmlPath": "/tmp/message.html",
                    "message": "Opened original message",
                }

            gmail.open_message_viewer = fake_open_message_viewer
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main([
                    "view",
                    "--id",
                    "msg-view",
                    "--no-images",
                    "--x",
                    "11",
                    "--y",
                    "22",
                    "--width",
                    "333",
                    "--height",
                    "444",
                ])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["messageId"], "msg-view")
            self.assertEqual(
                calls,
                [("msg-view", False, {"x": 11, "y": 22, "width": 333, "height": 444})],
            )
        finally:
            gmail.open_message_viewer = original_open_message_viewer

    def test_get_original_forces_html_detail_contract(self):
        from astrea_email import cli, gmail

        original_get_message = gmail.get_message
        try:
            calls = []

            def fake_get_message(message_id, load_remote_images=False, force_html=False):
                calls.append((message_id, load_remote_images, force_html))
                return {
                    "ok": True,
                    "provider": "gmail",
                    "messageId": message_id,
                    "message": {"messageId": message_id, "htmlRenderMode": "html"},
                }

            gmail.get_message = fake_get_message
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["get", "--id", "msg-original", "--images", "--original"])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["messageId"], "msg-original")
            self.assertEqual(calls, [("msg-original", True, True)])
        finally:
            gmail.get_message = original_get_message

    def test_preview_command_renders_original_html_snapshot_contract(self):
        from astrea_email import cli, gmail

        original_preview_message = gmail.preview_message
        try:
            calls = []

            def fake_preview_message(message_id, load_remote_images=False):
                calls.append((message_id, load_remote_images))
                return {
                    "ok": True,
                    "provider": "gmail",
                    "action": "preview",
                    "messageId": message_id,
                    "message": {
                        "messageId": message_id,
                        "webPreviewUrl": "file:///tmp/preview.png",
                        "webPreviewWidth": 820,
                        "webPreviewHeight": 1200,
                    },
                }

            gmail.preview_message = fake_preview_message
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["preview", "--id", "msg-preview", "--images"])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["action"], "preview")
            self.assertEqual(payload["messageId"], "msg-preview")
            self.assertEqual(calls, [("msg-preview", True)])
        finally:
            gmail.preview_message = original_preview_message

    def test_notify_command_polls_new_mail_contract(self):
        from astrea_email import cli, gmail

        original_poll_new_mail = gmail.poll_new_mail
        try:
            calls = []

            def fake_poll_new_mail(limit=10, state_path=None, desktop_notify=True, copy_codes=True, island_notify=True):
                calls.append((limit, state_path, desktop_notify, copy_codes, island_notify))
                return {
                    "ok": True,
                    "provider": "gmail",
                    "action": "notify",
                    "newCount": 1,
                    "events": [{"messageId": "new-1", "code": "482913"}],
                }

            gmail.poll_new_mail = fake_poll_new_mail
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main([
                    "notify",
                    "--limit",
                    "7",
                    "--state-path",
                    "/tmp/astrea-email-notify.json",
                    "--no-desktop",
                    "--no-clipboard",
                    "--no-island",
                ])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["action"], "notify")
            self.assertEqual(payload["newCount"], 1)
            self.assertEqual(calls, [(7, pathlib.Path("/tmp/astrea-email-notify.json"), False, False, False)])
        finally:
            gmail.poll_new_mail = original_poll_new_mail

    def test_links_command_returns_message_links_contract(self):
        from astrea_email import cli, gmail

        original_message_links = getattr(gmail, "message_links", None)
        try:
            calls = []

            def fake_message_links(message_id):
                calls.append(message_id)
                return {
                    "ok": True,
                    "provider": "gmail",
                    "action": "links",
                    "messageId": message_id,
                    "links": [{"url": "https://example.com", "label": "Example"}],
                }

            gmail.message_links = fake_message_links
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["links", "--id", "msg-links"])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["action"], "links")
            self.assertEqual(payload["messageId"], "msg-links")
            self.assertEqual(payload["links"][0]["url"], "https://example.com")
            self.assertEqual(calls, ["msg-links"])
        finally:
            if original_message_links is None:
                delattr(gmail, "message_links")
            else:
                gmail.message_links = original_message_links

    def test_settings_command_updates_notification_preferences_contract(self):
        from astrea_email import cli, gmail

        original_email_settings_payload = getattr(gmail, "email_settings_payload", None)
        try:
            calls = []

            def fake_email_settings_payload(updates=None):
                calls.append(dict(updates or {}))
                return {
                    "ok": True,
                    "provider": "gmail",
                    "action": "settings",
                    "settings": {
                        "mailServiceEnabled": updates.get("mailServiceEnabled", True) if updates else True,
                        "copyCodesEnabled": True,
                        "islandCodesEnabled": True,
                        "desktopNotificationsEnabled": True,
                    },
                }

            gmail.email_settings_payload = fake_email_settings_payload
            output = io.StringIO()

            with contextlib.redirect_stdout(output):
                code = cli.main(["settings", "--set", "mailServiceEnabled=false"])

            payload = json.loads(output.getvalue())
            self.assertEqual(code, 0)
            self.assertEqual(payload["action"], "settings")
            self.assertFalse(payload["settings"]["mailServiceEnabled"])
            self.assertEqual(calls, [{"mailServiceEnabled": False}])
        finally:
            if original_email_settings_payload is None:
                delattr(gmail, "email_settings_payload")
            else:
                gmail.email_settings_payload = original_email_settings_payload


if __name__ == "__main__":
    unittest.main()
