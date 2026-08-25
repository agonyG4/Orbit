#!/usr/bin/env python3
import base64
import importlib.util
import json
import pathlib
import tempfile
import unittest
import urllib.parse


SCRIPT = pathlib.Path(__file__).with_name("gmail_bridge.py")
SPEC = importlib.util.spec_from_file_location("gmail_bridge", SCRIPT)
gmail_bridge = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gmail_bridge)


class GmailBridgeHelpersTest(unittest.TestCase):
    def test_extract_auth_code_requires_context_and_ignores_prices(self):
        self.assertEqual(
            gmail_bridge.extract_auth_code("Seu código de verificação Astrea é 482913."),
            "482913",
        )
        self.assertEqual(
            gmail_bridge.extract_auth_code("Use OTP AB12CD to finish sign in"),
            "AB12CD",
        )
        self.assertEqual(
            gmail_bridge.extract_auth_code("Pedido Crunchyroll recebido. Total R$ 24,90"),
            "",
        )

    def test_poll_new_mail_bootstraps_seen_ids_without_notifying_old_mail(self):
        gmail_module = gmail_bridge.poll_new_mail.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_fetch_summary = gmail_module["fetch_message_summary"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                state_path = pathlib.Path(temp_dir) / "notify-state.json"
                gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "me@example.com"}
                gmail_module["api_request"] = lambda method, path, token, query=None, body=None: {
                    "messages": [{"id": "old-1"}, {"id": "old-2"}]
                }

                def fail_fetch_summary(*_args, **_kwargs):
                    raise AssertionError("bootstrap must not fetch old message summaries")

                gmail_module["fetch_message_summary"] = fail_fetch_summary

                payload = gmail_bridge.poll_new_mail(
                    limit=5,
                    state_path=state_path,
                    desktop_notify=False,
                    copy_codes=False,
                    island_notify=False,
                )

                self.assertTrue(payload["bootstrapped"])
                self.assertEqual(payload["newCount"], 0)
                self.assertEqual(json.loads(state_path.read_text(encoding="utf-8"))["seenMessageIds"], ["old-1", "old-2"])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["fetch_message_summary"] = original_fetch_summary

    def test_poll_new_mail_detects_2fa_code_copies_and_emits_island_event(self):
        gmail_module = gmail_bridge.poll_new_mail.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_fetch_summary = gmail_module["fetch_message_summary"]
        original_copy = gmail_module["copy_text_to_clipboard"]
        original_notify = gmail_module["send_desktop_notification"]
        original_island_event = gmail_module["write_island_email_event"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                state_path = pathlib.Path(temp_dir) / "notify-state.json"
                state_path.write_text(json.dumps({"seenMessageIds": ["old-1"]}), encoding="utf-8")
                copied = []
                desktop_notifications = []
                island_events = []

                gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "me@example.com"}
                gmail_module["api_request"] = lambda method, path, token, query=None, body=None: {
                    "messages": [{"id": "new-2fa"}, {"id": "old-1"}]
                }
                gmail_module["fetch_message_summary"] = lambda message_id, token=None: {
                    "messageId": message_id,
                    "threadId": "thread-2fa",
                    "fromName": "Astrea",
                    "fromAddress": "security@astrea.test",
                    "subject": "Seu código de verificação",
                    "preview": "Use o código 482913 para entrar.",
                    "timestamp": "20:30",
                }
                gmail_module["copy_text_to_clipboard"] = lambda text: copied.append(text) or True
                gmail_module["send_desktop_notification"] = lambda title, body, urgency="normal", **kwargs: desktop_notifications.append((title, body, urgency, kwargs)) or True
                gmail_module["write_island_email_event"] = lambda event, path=None: island_events.append(event)

                payload = gmail_bridge.poll_new_mail(
                    limit=5,
                    state_path=state_path,
                    desktop_notify=True,
                    copy_codes=True,
                    island_notify=True,
                )

                self.assertFalse(payload["bootstrapped"])
                self.assertEqual(payload["newCount"], 1)
                self.assertEqual(payload["events"][0]["code"], "482913")
                self.assertTrue(payload["events"][0]["copied"])
                self.assertEqual(copied, ["482913"])
                self.assertIn("482913", desktop_notifications[0][1])
                self.assertEqual(desktop_notifications[0][3]["event_id"], "email:new-2fa")
                self.assertEqual(desktop_notifications[0][3]["thread_id"], "email:thread-2fa")
                self.assertEqual(desktop_notifications[0][3]["collapse_key"], "email:inbox")
                self.assertEqual(desktop_notifications[0][3]["interruption_level"], "time-sensitive")
                self.assertEqual(island_events[0]["code"], "482913")
                self.assertIn("new-2fa", json.loads(state_path.read_text(encoding="utf-8"))["seenMessageIds"])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["fetch_message_summary"] = original_fetch_summary
            gmail_module["copy_text_to_clipboard"] = original_copy
            gmail_module["send_desktop_notification"] = original_notify
            gmail_module["write_island_email_event"] = original_island_event

    def test_poll_new_mail_marks_seen_before_notification_side_effects(self):
        gmail_module = gmail_bridge.poll_new_mail.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_fetch_summary = gmail_module["fetch_message_summary"]
        original_notify = gmail_module["send_desktop_notification"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                state_path = pathlib.Path(temp_dir) / "notify-state.json"
                state_path.write_text(json.dumps({"seenMessageIds": ["old-1"]}), encoding="utf-8")

                gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "me@example.com"}
                gmail_module["api_request"] = lambda method, path, token, query=None, body=None: {
                    "messages": [{"id": "new-fail"}, {"id": "old-1"}]
                }
                gmail_module["fetch_message_summary"] = lambda message_id, token=None: {
                    "messageId": message_id,
                    "threadId": "thread-fail",
                    "fromName": "Astrea",
                    "fromAddress": "team@astrea.test",
                    "subject": "Status",
                    "preview": "A notification transport failed.",
                    "timestamp": "20:45",
                    "isRead": False,
                }

                def fail_notify(*_args, **_kwargs):
                    raise RuntimeError("notification transport failed")

                gmail_module["send_desktop_notification"] = fail_notify

                payload = gmail_bridge.poll_new_mail(
                    limit=5,
                    state_path=state_path,
                    desktop_notify=True,
                    copy_codes=False,
                    island_notify=False,
                )

                seen_ids = json.loads(state_path.read_text(encoding="utf-8"))["seenMessageIds"]
                self.assertEqual(payload["newCount"], 1)
                self.assertIn("new-fail", seen_ids)
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["fetch_message_summary"] = original_fetch_summary
            gmail_module["send_desktop_notification"] = original_notify

    def test_poll_new_mail_fetches_detail_when_login_summary_has_context_without_code(self):
        gmail_module = gmail_bridge.poll_new_mail.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_fetch_summary = gmail_module["fetch_message_summary"]
        original_fetch_message = gmail_module["fetch_message"]
        original_copy = gmail_module["copy_text_to_clipboard"]
        original_notify = gmail_module["send_desktop_notification"]
        original_island_event = gmail_module["write_island_email_event"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                state_path = pathlib.Path(temp_dir) / "notify-state.json"
                state_path.write_text(json.dumps({"seenMessageIds": ["old-1"]}), encoding="utf-8")
                copied = []
                island_events = []
                fetched_details = []

                gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "me@example.com"}
                gmail_module["api_request"] = lambda method, path, token, query=None, body=None: {
                    "messages": [{"id": "roblox-code"}, {"id": "old-1"}]
                }
                gmail_module["fetch_message_summary"] = lambda message_id, token=None: {
                    "messageId": message_id,
                    "threadId": "thread-roblox",
                    "fromName": "Roblox",
                    "fromAddress": "no-reply@roblox.com",
                    "subject": "Roblox verification code",
                    "preview": "Use this code to continue signing in.",
                    "timestamp": "21:10",
                }

                def fake_fetch_message(message_id, token=None, load_remote_images=False, force_html=False):
                    fetched_details.append((message_id, load_remote_images, force_html))
                    return {
                        "messageId": message_id,
                        "threadId": "thread-roblox",
                        "fromName": "Roblox",
                        "fromAddress": "no-reply@roblox.com",
                        "subject": "Roblox verification code",
                        "preview": "Use this code to continue signing in.",
                        "body": "Your Roblox verification code is 739421.",
                        "timestamp": "21:10",
                    }

                gmail_module["fetch_message"] = fake_fetch_message
                gmail_module["copy_text_to_clipboard"] = lambda text: copied.append(text) or True
                gmail_module["send_desktop_notification"] = lambda *_args, **_kwargs: True
                gmail_module["write_island_email_event"] = lambda event, path=None: island_events.append(event)

                payload = gmail_bridge.poll_new_mail(
                    limit=5,
                    state_path=state_path,
                    desktop_notify=True,
                    copy_codes=True,
                    island_notify=True,
                )

                self.assertEqual(payload["events"][0]["code"], "739421")
                self.assertEqual(copied, ["739421"])
                self.assertEqual(island_events[0]["code"], "739421")
                self.assertEqual(fetched_details, [("roblox-code", False, False)])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["fetch_message_summary"] = original_fetch_summary
            gmail_module["fetch_message"] = original_fetch_message
            gmail_module["copy_text_to_clipboard"] = original_copy
            gmail_module["send_desktop_notification"] = original_notify
            gmail_module["write_island_email_event"] = original_island_event

    def test_email_settings_persist_notification_preferences(self):
        gmail_module = gmail_bridge.email_settings_payload.__globals__
        original_settings_path = gmail_module["EMAIL_SETTINGS_PATH"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["EMAIL_SETTINGS_PATH"] = pathlib.Path(temp_dir) / "settings.json"

                updated = gmail_bridge.email_settings_payload({
                    "mailServiceEnabled": False,
                    "copyCodesEnabled": False,
                    "islandCodesEnabled": True,
                    "desktopNotificationsEnabled": False,
                })
                loaded = gmail_bridge.email_settings_payload()

                self.assertFalse(updated["settings"]["mailServiceEnabled"])
                self.assertFalse(loaded["settings"]["copyCodesEnabled"])
                self.assertTrue(loaded["settings"]["islandCodesEnabled"])
                self.assertFalse(loaded["settings"]["desktopNotificationsEnabled"])
        finally:
            gmail_module["EMAIL_SETTINGS_PATH"] = original_settings_path

    def test_email_settings_read_string_booleans_safely(self):
        gmail_module = gmail_bridge.email_settings_payload.__globals__
        original_settings_path = gmail_module["EMAIL_SETTINGS_PATH"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                settings_path = pathlib.Path(temp_dir) / "settings.json"
                gmail_module["EMAIL_SETTINGS_PATH"] = settings_path
                settings_path.write_text(
                    json.dumps({
                        "mailServiceEnabled": "false",
                        "copyCodesEnabled": "0",
                        "islandCodesEnabled": "true",
                        "desktopNotificationsEnabled": "invalid",
                    }),
                    encoding="utf-8",
                )

                loaded = gmail_bridge.email_settings_payload()

                self.assertFalse(loaded["settings"]["mailServiceEnabled"])
                self.assertFalse(loaded["settings"]["copyCodesEnabled"])
                self.assertTrue(loaded["settings"]["islandCodesEnabled"])
                self.assertTrue(loaded["settings"]["desktopNotificationsEnabled"])
        finally:
            gmail_module["EMAIL_SETTINGS_PATH"] = original_settings_path

    def test_build_query_combines_folder_filter_and_text(self):
        query = gmail_bridge.build_query("Inbox", "unread", "from:team Astrea")
        self.assertEqual(query, "in:inbox is:unread from:team Astrea")

    def test_build_query_supports_starred_and_archive(self):
        self.assertEqual(gmail_bridge.build_query("Starred", "all", ""), "is:starred")
        self.assertEqual(gmail_bridge.build_query("Archive", "starred", ""), "-in:inbox -in:trash -in:sent -in:drafts is:starred")

    def test_parse_headers_extracts_display_name_and_address(self):
        headers = {
            "From": "Nina Costa <nina@example.com>",
            "Subject": "Design Review",
            "Date": "Sun, 31 May 2026 09:42:00 -0300",
        }

        parsed = gmail_bridge.parse_headers(headers)

        self.assertEqual(parsed["fromName"], "Nina Costa")
        self.assertEqual(parsed["fromAddress"], "nina@example.com")
        self.assertEqual(parsed["subject"], "Design Review")
        self.assertTrue(parsed["timestamp"])

    def test_extract_plain_text_body_walks_nested_parts(self):
        body = "Hello from Gmail\n\nThis is plain text."
        encoded = base64.urlsafe_b64encode(body.encode("utf-8")).decode("ascii").rstrip("=")
        payload = {
            "mimeType": "multipart/alternative",
            "parts": [
                {"mimeType": "text/html", "body": {"data": "PGI-SFRNTDwvYj4"}},
                {
                    "mimeType": "multipart/mixed",
                    "parts": [{"mimeType": "text/plain", "body": {"data": encoded}}],
                },
            ],
        }

        self.assertEqual(gmail_bridge.extract_plain_text(payload), body)

    def test_normalize_message_exposes_image_attachment_data(self):
        image_bytes = b"\x89PNG\r\n\x1a\n"
        encoded_image = base64.urlsafe_b64encode(image_bytes).decode("ascii").rstrip("=")
        message = {
            "id": "msg-image",
            "labelIds": ["INBOX"],
            "snippet": "Image attached",
            "payload": {
                "mimeType": "multipart/mixed",
                "headers": [
                    {"name": "From", "value": "Nina Costa <nina@example.com>"},
                    {"name": "Subject", "value": "Screenshot"},
                ],
                "parts": [
                    {
                        "mimeType": "text/plain",
                        "body": {
                            "data": base64.urlsafe_b64encode(b"See attached.").decode("ascii").rstrip("=")
                        },
                    },
                    {
                        "filename": "screenshot.png",
                        "mimeType": "image/png",
                        "headers": [
                            {"name": "Content-Disposition", "value": "attachment; filename=screenshot.png"}
                        ],
                        "body": {"attachmentId": "att-1", "size": len(image_bytes)},
                    },
                ],
            },
        }

        normalized = gmail_bridge.normalize_message(message, attachment_loader=lambda attachment_id: encoded_image)

        self.assertEqual(len(normalized["attachments"]), 1)
        attachment = normalized["attachments"][0]
        self.assertEqual(attachment["name"], "screenshot.png")
        self.assertEqual(attachment["mimeType"], "image/png")
        self.assertEqual(attachment["dataUrl"], "data:image/png;base64," + base64.b64encode(image_bytes).decode("ascii"))

    def test_normalize_message_preserves_sanitized_html_body_and_cid_images(self):
        image_bytes = b"\x89PNG\r\n\x1a\n"
        encoded_image = base64.urlsafe_b64encode(image_bytes).decode("ascii").rstrip("=")
        html_body = """
            <html>
              <head><style>.hidden { display: none; }</style></head>
              <body onclick="steal()">
                <table bgcolor="#303030"><tr><td>
                  <h1>Day 1 Is GTC Keynote Day</h1>
                  <img src="cid:hero-image" width="600" onclick="bad()" />
                  <script>alert("nope")</script>
                </td></tr></table>
              </body>
            </html>
        """
        message = {
            "id": "msg-html",
            "labelIds": ["INBOX"],
            "snippet": "Day 1 Is GTC Keynote Day",
            "payload": {
                "mimeType": "multipart/related",
                "headers": [
                    {"name": "From", "value": "NVIDIA <news@nvidia.com>"},
                    {"name": "Subject", "value": "GTC Keynote"},
                ],
                "parts": [
                    {
                        "mimeType": "text/html",
                        "body": {
                            "data": base64.urlsafe_b64encode(html_body.encode("utf-8")).decode("ascii").rstrip("=")
                        },
                    },
                    {
                        "filename": "hero.png",
                        "mimeType": "image/png",
                        "headers": [{"name": "Content-ID", "value": "<hero-image>"}],
                        "body": {"attachmentId": "att-hero", "size": len(image_bytes)},
                    },
                ],
            },
        }

        normalized = gmail_bridge.normalize_message(message, attachment_loader=lambda attachment_id: encoded_image)

        self.assertIn("Day 1 Is GTC Keynote Day", normalized["htmlBody"])
        self.assertIn("data:image/png;base64," + base64.b64encode(image_bytes).decode("ascii"), normalized["htmlBody"])
        self.assertNotIn("<script", normalized["htmlBody"])
        self.assertNotIn("onclick", normalized["htmlBody"])
        self.assertNotIn("<style", normalized["htmlBody"])

    def test_sanitize_html_keeps_body_when_head_styles_are_malformed(self):
        raw_html = """
            <html>
              <head><style>@media screen { .x { color: red } }
              <body>
                <table><tr><td><h1>Day 1 Is GTC Keynote Day</h1><img src="https://example.com/hero.png"></td></tr></table>
              </body>
            </html>
        """

        sanitized = gmail_bridge.sanitize_html_email(raw_html, [])

        self.assertIn("Day 1 Is GTC Keynote Day", sanitized)
        self.assertIn("<table", sanitized)
        self.assertNotIn("https://example.com/hero.png", sanitized)
        self.assertNotIn("<style", sanitized)

    def test_sanitize_html_blocks_remote_images_without_loader(self):
        raw_html = """
            <body>
              <h1>Newsletter</h1>
              <img src="https://pixel.monitor1.returnpath.net/open.png" width="1" height="1">
              <img src="https://images.example.com/hero.png" width="640">
            </body>
        """

        sanitized = gmail_bridge.sanitize_html_email(raw_html, [])

        self.assertIn("Newsletter", sanitized)
        self.assertNotIn("pixel.monitor1.returnpath.net", sanitized)
        self.assertNotIn("images.example.com", sanitized)
        self.assertNotIn("<img", sanitized)

    def test_sanitize_html_strips_qml_unsafe_characters(self):
        raw_html = "<body><p>Ok\uffff &#65535; &#xFFFE; \x01 \u034f Done</p></body>"

        details = gmail_bridge.sanitize_html_email_details(raw_html, [])

        self.assertIn("Ok", details["html"])
        self.assertIn("Done", details["html"])
        self.assertNotIn("\uffff", details["html"])
        self.assertNotIn("&#65535", details["html"])
        self.assertNotIn("&#xFFFE", details["html"])
        self.assertNotIn("\x01", details["html"])
        self.assertNotIn("\u034f", details["html"])

    def test_upgrade_cached_message_strips_qml_unsafe_characters(self):
        cached = {
            "messageId": "unsafe",
            "subject": "Bad\uffff subject",
            "preview": "Preview\x01\u034f",
            "body": "Body &#65535; done",
            "htmlBody": "<p>Body\uffff &#xFFFE; done</p>",
            "htmlRenderMode": "html",
        }

        upgraded, changed = gmail_bridge.upgrade_cached_message(cached)

        self.assertTrue(changed)
        self.assertEqual(upgraded["subject"], "Bad subject")
        self.assertEqual(upgraded["preview"], "Preview")
        self.assertNotIn("&#65535", upgraded["body"])
        self.assertNotIn("\uffff", upgraded["htmlBody"])
        self.assertNotIn("&#xFFFE", upgraded["htmlBody"])

    def test_sanitize_html_inlines_and_centers_remote_images_with_loader(self):
        raw_html = '<body><img src="https://images.example.com/hero.png" width="900"></body>'

        sanitized = gmail_bridge.sanitize_html_email(
            raw_html,
            [],
            remote_image_loader=lambda url: "data:image/png;base64,abcd" if url.startswith("https://images.example.com") else "",
        )

        self.assertIn('<p align="center">', sanitized)
        self.assertIn('src="data:image/png;base64,abcd"', sanitized)
        self.assertIn('width="640"', sanitized)

    def test_sanitize_html_centers_renderable_email_layouts(self):
        raw_html = """
            <body>
                <table width="600"><tr><td><h1>Privacy Update</h1></td></tr></table>
            </body>
        """

        details = gmail_bridge.sanitize_html_email_details(raw_html, [])

        self.assertEqual(details["htmlRenderMode"], "html")
        self.assertFalse(details["htmlSuppressed"])
        self.assertTrue(details["html"].startswith('<div align="center">'))
        self.assertIn('<table width="600">', details["html"])

    def test_sanitize_html_uses_reader_mode_for_large_email_layouts(self):
        raw_html = "<body>" + ("<table><tr><td>Job match</td></tr></table>" * 70) + "</body>"

        details = gmail_bridge.sanitize_html_email_details(raw_html, [])

        self.assertEqual(details["htmlRenderMode"], "reader")
        self.assertTrue(details["htmlSuppressed"])
        self.assertIn("Job match", details["html"])
        self.assertGreaterEqual(details["htmlTableCount"], 70)

    def test_sanitize_html_can_force_original_layout_for_web_viewer(self):
        raw_html = "<body>" + ("<table><tr><td>Original layout</td></tr></table>" * 70) + "</body>"

        details = gmail_bridge.sanitize_html_email_details(raw_html, [], force_html=True)

        self.assertEqual(details["htmlRenderMode"], "html")
        self.assertFalse(details["htmlSuppressed"])
        self.assertIn("<table", details["html"])
        self.assertIn("Original layout", details["html"])

    def test_sanitize_html_uses_reader_mode_for_complex_transactional_layouts(self):
        pix_code = "00020101021226740014br.gov.bcb.pix2552pix.ebanx.com/qr/v2/ABC"
        raw_html = "<body>" + ("""
            <table width="640" bgcolor="#102040"><tr><td>
              <table><tr><td>Seu código Pix Crunchyroll de R$ 24,90 foi gerado!</td></tr></table>
              <table><tr><td><img src="https://images.example.com/pix-hero.png" width="640"></td></tr></table>
              <table><tr><td>Leia ou copie o código Pix</td></tr></table>
              <table><tr><td>""" + pix_code + """</td></tr></table>
              <table><tr><td>Detalhes do pagamento</td></tr></table>
            </td></tr></table>
        """ * 3) + "</body>"

        details = gmail_bridge.sanitize_html_email_details(raw_html, [])

        self.assertEqual(details["htmlRenderMode"], "reader")
        self.assertTrue(details["htmlSuppressed"])
        self.assertIn("Seu código Pix Crunchyroll", details["html"])
        self.assertIn(pix_code, details["html"])
        self.assertEqual(details["remoteImageCount"], 1)
        self.assertEqual(details["remoteImagesLoadedCount"], 0)

    def test_sanitize_html_loads_original_images_for_complex_layout_on_request(self):
        raw_html = """
            <body>
              <table width="640" bgcolor="#102040"><tr><td>
                <table><tr><td>Seu código Pix Crunchyroll de R$ 24,90 foi gerado!</td></tr></table>
                <table><tr><td><img src="https://images.example.com/pix-hero.png" width="640"></td></tr></table>
                <table><tr><td>Leia ou copie o código Pix</td></tr></table>
                <table><tr><td>00020101021226740014br.gov.bcb.pix</td></tr></table>
              </td></tr></table>
            </body>
        """

        details = gmail_bridge.sanitize_html_email_details(
            raw_html,
            [],
            remote_image_loader=lambda url: "file:///tmp/astrea-email/pix-hero.png",
        )

        self.assertEqual(details["htmlRenderMode"], "html")
        self.assertFalse(details["htmlSuppressed"])
        self.assertEqual(details["remoteImageCount"], 1)
        self.assertEqual(details["remoteImagesLoadedCount"], 1)
        self.assertIn('src="file:///tmp/astrea-email/pix-hero.png"', details["html"])

    def test_reader_mode_removes_tracking_urls_and_invisible_padding(self):
        tracking_url = "https://discount.grammarly.com/api/discounts/live?hash=84059faf3e9e6188596fac271da61fb5ddedf068&discount=eydhbGlhcyc6ICdoVGhPQm8nLCAnY2FtcGFpZ25JZCc6ICcyMDI2XzUwb2ZmYW55"
        raw_html = "<body>" + (f"""
            <table><tr><td>
                Grammarly Upgrade today for just $72/year.
                \u200c \u200c \u200c
                <p><a href="http://grammarly.com/">http://grammarly.com/</a></p>
                <p><a href="{tracking_url}">{tracking_url}</a></p>
                <h1>You’ve outgrown the basics</h1>
                <p>Your grammar's under control, but the real magic happens when you move past corrections.</p>
                <p><a href="{tracking_url}">Upgrade now</a></p>
            </td></tr></table>
        """ * 50) + "</body>"

        details = gmail_bridge.sanitize_html_email_details(raw_html, [])

        self.assertEqual(details["htmlRenderMode"], "reader")
        self.assertIn("You’ve outgrown the basics", details["html"])
        self.assertIn("Upgrade now", details["html"])
        self.assertNotIn(tracking_url, details["html"])
        self.assertNotIn("http://grammarly.com/", details["html"])
        self.assertNotIn("\u200c", details["html"])

    def test_reader_html_renders_escaped_lists_as_real_lists(self):
        reader_html = gmail_bridge.reader_text_to_html(
            """
            Novidades:
            <ul>

            <li>As atualizações aumentam a legibilidade.</li>

            <li>Você pode revogar essa permissão a qualquer momento.</li>
            </ul>
            """
        )

        self.assertIn("<ul>", reader_html)
        self.assertIn("<li>As atualizações aumentam a legibilidade.</li>", reader_html)
        self.assertNotIn("<ul></ul>", reader_html)
        self.assertNotIn("&lt;ul&gt;", reader_html)
        self.assertNotIn("&lt;li&gt;", reader_html)

    def test_upgrade_cached_reader_html_preserves_reader_mode_and_cleans_markup(self):
        cached = {
            "messageId": "spotify",
            "body": "Novidades:\n<ul>\n<li>As atualizações aumentam a legibilidade.</li>\n</ul>",
            "htmlBody": '<div align="center"><div align="left"><p>Novidades:</p><p>&lt;ul&gt;</p><p>&lt;li&gt;As atualizações aumentam a legibilidade.&lt;/li&gt;</p><p>&lt;/ul&gt;</p></div></div>',
            "htmlRenderMode": "reader",
            "htmlSuppressed": True,
        }

        upgraded, changed = gmail_bridge.upgrade_cached_message(cached)

        self.assertTrue(changed)
        self.assertEqual(upgraded["htmlRenderMode"], "reader")
        self.assertIn("<ul>", upgraded["htmlBody"])
        self.assertIn("<li>As atualizações aumentam a legibilidade.</li>", upgraded["htmlBody"])
        self.assertNotIn("<ul></ul>", upgraded["htmlBody"])
        self.assertNotIn("&lt;li&gt;", upgraded["htmlBody"])

    def test_normalize_message_counts_blocked_remote_images(self):
        html_body = """
            <body>
                <img src="https://pixel.monitor1.returnpath.net/open.png" width="1" height="1">
                <img src="https://jobs.example.com/company-logo.png" width="320">
            </body>
        """
        message = {
            "id": "msg-remote",
            "labelIds": ["INBOX"],
            "snippet": "Job match",
            "payload": {
                "mimeType": "text/html",
                "headers": [
                    {"name": "From", "value": "Jobs <jobs@example.com>"},
                    {"name": "Subject", "value": "New jobs"},
                ],
                "body": {
                    "data": base64.urlsafe_b64encode(html_body.encode("utf-8")).decode("ascii").rstrip("=")
                },
            },
        }

        normalized = gmail_bridge.normalize_message(message)

        self.assertEqual(normalized["remoteImageCount"], 1)
        self.assertFalse(normalized["remoteImagesLoaded"])
        self.assertNotIn("jobs.example.com", normalized["htmlBody"])
        self.assertNotIn("pixel.monitor1.returnpath.net", normalized["htmlBody"])

    def test_normalize_message_exposes_reader_mode_for_heavy_html(self):
        plain_body = "Readable job fallback"
        html_body = "<body>" + ("<table><tr><td>Heavy layout</td></tr></table>" * 70) + "</body>"
        message = {
            "id": "msg-heavy-html",
            "labelIds": ["INBOX"],
            "snippet": "Heavy job match",
            "payload": {
                "mimeType": "multipart/alternative",
                "headers": [
                    {"name": "From", "value": "Jobs <jobs@example.com>"},
                    {"name": "Subject", "value": "New jobs"},
                ],
                "parts": [
                    {
                        "mimeType": "text/plain",
                        "body": {
                            "data": base64.urlsafe_b64encode(plain_body.encode("utf-8")).decode("ascii").rstrip("=")
                        },
                    },
                    {
                        "mimeType": "text/html",
                        "body": {
                            "data": base64.urlsafe_b64encode(html_body.encode("utf-8")).decode("ascii").rstrip("=")
                        },
                    },
                ],
            },
        }

        normalized = gmail_bridge.normalize_message(message)

        self.assertEqual(normalized["body"], plain_body)
        self.assertIn("Heavy layout", normalized["htmlBody"])
        self.assertEqual(normalized["htmlRenderMode"], "reader")
        self.assertTrue(normalized["htmlSuppressed"])

    def test_normalize_message_loads_large_html_body_parts(self):
        html_body = "<table><tr><td><h1>Day 1 Is GTC Keynote Day</h1></td></tr></table>"
        encoded_html = base64.urlsafe_b64encode(html_body.encode("utf-8")).decode("ascii").rstrip("=")
        message = {
            "id": "msg-large-html",
            "labelIds": ["INBOX"],
            "snippet": "Day 1 Is GTC Keynote Day",
            "payload": {
                "mimeType": "multipart/alternative",
                "headers": [
                    {"name": "From", "value": "NVIDIA <news@nvidia.com>"},
                    {"name": "Subject", "value": "GTC Keynote"},
                ],
                "parts": [
                    {
                        "mimeType": "text/plain",
                        "body": {
                            "data": base64.urlsafe_b64encode(b"Plain fallback").decode("ascii").rstrip("=")
                        },
                    },
                    {
                        "mimeType": "text/html",
                        "body": {"attachmentId": "html-body", "size": len(html_body)},
                    },
                ],
            },
        }

        normalized = gmail_bridge.normalize_message(message, attachment_loader=lambda attachment_id: encoded_html)

        self.assertIn("Day 1 Is GTC Keynote Day", normalized["htmlBody"])
        self.assertEqual(normalized["body"], "Plain fallback")
        self.assertEqual(normalized["attachments"], [])

    def test_list_messages_follows_gmail_pages_until_limit(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)

                def minimal_message(message_id):
                    return {
                        "id": message_id,
                        "labelIds": ["INBOX"],
                        "snippet": message_id,
                        "payload": {
                            "headers": [
                                {"name": "From", "value": "Astrea <team@example.com>"},
                                {"name": "Subject", "value": message_id},
                            ],
                            "body": {
                                "data": base64.urlsafe_b64encode(message_id.encode("utf-8")).decode("ascii").rstrip("=")
                            },
                        },
                    }

                list_queries = []

                def fake_api_request(method, path, token, query=None, body=None):
                    if path == "/messages":
                        list_queries.append(dict(query or {}))
                        if query and query.get("pageToken") == "page-2":
                            return {"messages": [{"id": "m3"}], "resultSizeEstimate": 3}
                        return {
                            "messages": [{"id": "m1"}, {"id": "m2"}],
                            "nextPageToken": "page-2",
                            "resultSizeEstimate": 3,
                        }
                    if path.startswith("/messages/"):
                        return minimal_message(path.rsplit("/", 1)[-1])
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                payload = gmail_bridge.list_messages("Inbox", "all", "", 3, force_refresh=True)

                self.assertEqual([message["messageId"] for message in payload["messages"]], ["m1", "m2", "m3"])
                self.assertEqual(len(list_queries), 2)
                self.assertEqual(list_queries[1]["pageToken"], "page-2")
                self.assertEqual(payload["resultSizeEstimate"], 3)
                self.assertEqual(payload["nextPageToken"], "")
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_list_messages_fetches_metadata_summaries_without_full_body(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "summary@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                message_queries = []

                def fake_api_request(method, path, token, query=None, body=None):
                    if path == "/messages":
                        return {"messages": [{"id": "summary-1"}], "resultSizeEstimate": 1}
                    if path == "/messages/summary-1":
                        message_queries.append(dict(query or {}))
                        self.assertEqual(query.get("format"), "metadata")
                        return {
                            "id": "summary-1",
                            "threadId": "thread-1",
                            "labelIds": ["INBOX", "UNREAD"],
                            "snippet": "Summary preview",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "Summary only"},
                                    {"name": "Date", "value": "Sun, 31 May 2026 09:42:00 -0300"},
                                ],
                                "body": {
                                    "data": base64.urlsafe_b64encode(b"Body must not be decoded").decode("ascii").rstrip("=")
                                },
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                payload = gmail_bridge.list_messages("Inbox", "all", "", 10, force_refresh=True)

                self.assertEqual(len(message_queries), 1)
                self.assertEqual(payload["messages"][0]["messageId"], "summary-1")
                self.assertEqual(payload["messages"][0]["threadId"], "thread-1")
                self.assertEqual(payload["messages"][0]["preview"], "Summary preview")
                self.assertNotIn("body", payload["messages"][0])
                self.assertNotIn("htmlBody", payload["messages"][0])
                self.assertNotIn("attachments", payload["messages"][0])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_list_messages_saves_and_reuses_cached_page(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "cache@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                api_calls = []

                def fake_api_request(method, path, token, query=None, body=None):
                    api_calls.append((method, path))
                    if path == "/messages":
                        return {"messages": [{"id": "cached-message"}], "resultSizeEstimate": 1}
                    if path == "/messages/cached-message":
                        return {
                            "id": "cached-message",
                            "labelIds": ["INBOX"],
                            "snippet": "Cached body",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "Cached"},
                                ],
                                "body": {
                                    "data": base64.urlsafe_b64encode(b"Cached body").decode("ascii").rstrip("=")
                                },
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                live_payload = gmail_bridge.list_messages("Inbox", "all", "", 10, force_refresh=True)
                self.assertFalse(live_payload["cached"])
                self.assertEqual([message["messageId"] for message in live_payload["messages"]], ["cached-message"])
                self.assertEqual(len(api_calls), 2)

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Cache was bypassed by unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fail_api_request
                cached_payload = gmail_bridge.list_messages("Inbox", "all", "", 10)

                self.assertTrue(cached_payload["cached"])
                self.assertEqual([message["messageId"] for message in cached_payload["messages"]], ["cached-message"])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_exact_cache_hit_does_not_scan_cache_directory(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        original_glob = pathlib.Path.glob
        try:
            token = {"access_token": "token", "account": "cache@example.com"}
            gmail_module["ensure_token"] = lambda: token
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                gmail_bridge.write_cache(
                    token,
                    "Inbox",
                    "all",
                    "",
                    10,
                    "",
                    {
                        "ok": True,
                        "provider": "gmail",
                        "folder": "Inbox",
                        "filter": "all",
                        "query": "",
                        "pageToken": "",
                        "messages": [{
                            "messageId": "cached-message",
                            "folder": "Inbox",
                            "fromName": "Astrea",
                            "fromAddress": "team@example.com",
                            "subject": "Cached",
                            "preview": "Cached preview",
                            "timestamp": "",
                            "tag": "Gmail",
                            "starred": False,
                            "isRead": True,
                            "importance": "normal",
                        }],
                        "resultSizeEstimate": 1,
                        "nextPageToken": "",
                    },
                )

                def fail_glob(self, pattern):
                    raise AssertionError(f"Exact cache hit scanned cache directory: {pattern}")

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Exact cache hit made a Gmail API request: {method} {path}")

                pathlib.Path.glob = fail_glob
                gmail_module["api_request"] = fail_api_request
                cached_payload = gmail_bridge.list_messages("Inbox", "all", "", 100)

                self.assertTrue(cached_payload["cached"])
                self.assertEqual([message["messageId"] for message in cached_payload["messages"]], ["cached-message"])
        finally:
            pathlib.Path.glob = original_glob
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_cached_pages_are_reused_across_different_limits(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "cache@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)

                def fake_api_request(method, path, token, query=None, body=None):
                    if path == "/messages":
                        return {"messages": [{"id": "cached-message"}], "resultSizeEstimate": 1}
                    if path == "/messages/cached-message":
                        return {
                            "id": "cached-message",
                            "labelIds": ["INBOX"],
                            "snippet": "Cached body",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "Cached"},
                                ],
                                "body": {
                                    "data": base64.urlsafe_b64encode(b"Cached body").decode("ascii").rstrip("=")
                                },
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request
                gmail_bridge.list_messages("Inbox", "all", "", 2, force_refresh=True)

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Limit-only cache miss made a Gmail API request: {method} {path}")

                gmail_module["api_request"] = fail_api_request
                cached_payload = gmail_bridge.list_messages("Inbox", "all", "", 100)

                self.assertTrue(cached_payload["cached"])
                self.assertEqual([message["messageId"] for message in cached_payload["messages"]], ["cached-message"])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_force_refresh_reuses_cached_summaries(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            token = {"access_token": "token", "account": "cache@example.com"}
            gmail_module["ensure_token"] = lambda: token
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                cached_message = {
                    "messageId": "cached-message",
                    "folder": "Inbox",
                    "fromName": "Astrea",
                    "fromAddress": "team@example.com",
                    "subject": "Cached body",
                    "preview": "Cached preview",
                    "body": "Cached body that should not be downloaded again",
                    "htmlBody": "",
                    "timestamp": "",
                    "tag": "Gmail",
                    "starred": False,
                    "isRead": True,
                    "importance": "normal",
                    "attachments": [],
                    "remoteImageCount": 0,
                    "remoteImagesLoadedCount": 0,
                    "remoteImagesLoaded": False,
                }
                gmail_bridge.write_cache(
                    token,
                    "Inbox",
                    "all",
                    "",
                    10,
                    "",
                    {
                        "ok": True,
                        "provider": "gmail",
                        "folder": "Inbox",
                        "filter": "all",
                        "query": "",
                        "pageToken": "",
                        "messages": [cached_message],
                        "resultSizeEstimate": 1,
                        "nextPageToken": "",
                    },
                )

                def fake_api_request(method, path, token, query=None, body=None):
                    if path == "/messages":
                        return {
                            "messages": [{"id": "cached-message"}, {"id": "new-message"}],
                            "resultSizeEstimate": 2,
                        }
                    if path == "/messages/cached-message":
                        self.fail("Force refresh downloaded a cached message body again")
                    if path == "/messages/new-message":
                        return {
                            "id": "new-message",
                            "labelIds": ["INBOX"],
                            "snippet": "New body",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "New body"},
                                ],
                                "body": {
                                    "data": base64.urlsafe_b64encode(b"New body").decode("ascii").rstrip("=")
                                },
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                payload = gmail_bridge.list_messages("Inbox", "all", "", 10, force_refresh=True)

                self.assertEqual([message["messageId"] for message in payload["messages"]], ["cached-message", "new-message"])
                self.assertNotIn("body", payload["messages"][0])
                self.assertEqual(payload["messages"][1]["preview"], "New body")
                self.assertNotIn("body", payload["messages"][1])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_list_messages_never_returns_cached_detail_body(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            token = {"access_token": "token", "account": "cache@example.com"}
            gmail_module["ensure_token"] = lambda: token
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                gmail_bridge.write_cache(
                    token,
                    "Inbox",
                    "all",
                    "",
                    10,
                    "",
                    {
                        "ok": True,
                        "provider": "gmail",
                        "folder": "Inbox",
                        "filter": "all",
                        "query": "",
                        "pageToken": "",
                        "messages": [{
                            "messageId": "detail-in-list",
                            "threadId": "thread",
                            "folder": "Inbox",
                            "fromName": "Astrea",
                            "fromAddress": "team@example.com",
                            "subject": "Cached detail",
                            "preview": "Short summary",
                            "body": "Large body must not go through list JSON",
                            "htmlBody": "<p>Large html must not go through list JSON</p>",
                            "attachments": [{"id": "a1", "dataUrl": "data:image/png;base64,abc"}],
                            "timestamp": "",
                            "tag": "Gmail",
                            "starred": False,
                            "isRead": True,
                            "importance": "normal",
                            "detailLoaded": True,
                        }],
                        "resultSizeEstimate": 1,
                        "nextPageToken": "",
                    },
                )

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Cached list made a Gmail API request: {method} {path}")

                gmail_module["api_request"] = fail_api_request
                payload = gmail_bridge.list_messages("Inbox", "all", "", 10)

                message = payload["messages"][0]
                self.assertEqual(message["messageId"], "detail-in-list")
                self.assertFalse(message["detailLoaded"])
                self.assertNotIn("body", message)
                self.assertNotIn("htmlBody", message)
                self.assertNotIn("attachments", message)
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_list_messages_reuses_summary_index_across_folders(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "summary-index@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                metadata_calls = []

                def fake_api_request(method, path, token, query=None, body=None):
                    if path == "/messages":
                        return {"messages": [{"id": "shared-message"}], "resultSizeEstimate": 1}
                    if path == "/messages/shared-message":
                        metadata_calls.append(dict(query or {}))
                        return {
                            "id": "shared-message",
                            "threadId": "thread-shared",
                            "labelIds": ["INBOX"],
                            "snippet": "Reusable summary",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "Shared"},
                                ],
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                inbox_payload = gmail_bridge.list_messages("Inbox", "all", "", 10, force_refresh=True)
                all_payload = gmail_bridge.list_messages("All", "all", "", 10, force_refresh=True)

                self.assertEqual([message["messageId"] for message in inbox_payload["messages"]], ["shared-message"])
                self.assertEqual([message["messageId"] for message in all_payload["messages"]], ["shared-message"])
                self.assertEqual(len(metadata_calls), 1)
                self.assertEqual(metadata_calls[0]["format"], "metadata")
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_get_message_reuses_cached_detail_without_network(self):
        gmail_module = gmail_bridge.get_message.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "detail@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                calls = []

                def fake_api_request(method, path, token, query=None, body=None):
                    calls.append((method, path, dict(query or {})))
                    if path == "/messages/detail-message":
                        return {
                            "id": "detail-message",
                            "threadId": "thread-detail",
                            "labelIds": ["INBOX"],
                            "snippet": "Detailed preview",
                            "payload": {
                                "headers": [
                                    {"name": "From", "value": "Astrea <team@example.com>"},
                                    {"name": "Subject", "value": "Detailed"},
                                ],
                                "body": {
                                    "data": base64.urlsafe_b64encode(b"Detailed body").decode("ascii").rstrip("=")
                                },
                            },
                        }
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request
                live_payload = gmail_bridge.get_message("detail-message", load_remote_images=False, force_html=False)
                self.assertEqual(live_payload["message"]["body"], "Detailed body")
                self.assertFalse(live_payload.get("cached", False))

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Cached get made a Gmail API request: {method} {path}")

                gmail_module["api_request"] = fail_api_request
                cached_payload = gmail_bridge.get_message("detail-message", load_remote_images=False, force_html=False)

                self.assertTrue(cached_payload["cached"])
                self.assertEqual(cached_payload["message"]["body"], "Detailed body")
                self.assertEqual(len(calls), 1)
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_cache_only_returns_empty_payload_without_network_on_miss(self):
        gmail_module = gmail_bridge.list_messages.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "cache@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)

                def fail_api_request(method, path, token, query=None, body=None):
                    self.fail(f"Cache-only mode made a Gmail API request: {method} {path}")

                gmail_module["api_request"] = fail_api_request
                payload = gmail_bridge.list_messages("Inbox", "all", "", 100, cache_only=True)

                self.assertTrue(payload["ok"])
                self.assertTrue(payload["cacheMiss"])
                self.assertEqual(payload["messages"], [])
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_modify_message_returns_patch_for_image_message_without_loading_attachment(self):
        gmail_module = gmail_bridge.modify_message.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token"}
            calls = []

            def fake_api_request(method, path, token, query=None, body=None):
                calls.append((method, path))
                if method == "POST":
                    return {}
                if method == "GET":
                    self.fail(f"Modify fetched message data after label action: {path}")
                self.fail(f"Unexpected Gmail API request: {method} {path}")

            gmail_module["api_request"] = fake_api_request

            payload = gmail_bridge.modify_message("msg-image", "read")

            self.assertEqual(payload["message"], {"isRead": True, "messageId": "msg-image"})
            self.assertNotIn(("GET", "/messages/msg-image"), calls)
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request

    def test_modify_plan_maps_user_actions_to_gmail_labels(self):
        self.assertEqual(
            gmail_bridge.modify_plan("read"),
            {"endpoint": "modify", "body": {"removeLabelIds": ["UNREAD"]}},
        )
        self.assertEqual(
            gmail_bridge.modify_plan("unread"),
            {"endpoint": "modify", "body": {"addLabelIds": ["UNREAD"]}},
        )
        self.assertEqual(
            gmail_bridge.modify_plan("archive"),
            {"endpoint": "modify", "body": {"removeLabelIds": ["INBOX"]}},
        )
        self.assertEqual(gmail_bridge.modify_plan("trash"), {"endpoint": "trash", "body": {}})
        self.assertEqual(gmail_bridge.modify_plan("inbox"), {"endpoint": "untrash", "body": {}})

    def test_modify_message_returns_patch_without_fetching_full_message(self):
        gmail_module = gmail_bridge.modify_message.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_api_request = gmail_module["api_request"]
        original_cache_dir = gmail_module["CACHE_DIR"]
        try:
            gmail_module["ensure_token"] = lambda: {"access_token": "token", "account": "patch@example.com"}
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                calls = []

                def fake_api_request(method, path, token, query=None, body=None):
                    calls.append((method, path, dict(query or {}), body))
                    if method == "POST" and path == "/messages/msg-1/modify":
                        return {}
                    if method == "GET":
                        self.fail(f"Modify fetched full message after label patch: {path}")
                    self.fail(f"Unexpected Gmail API request: {method} {path}")

                gmail_module["api_request"] = fake_api_request

                payload = gmail_bridge.modify_message("msg-1", "read")

                self.assertTrue(payload["ok"])
                self.assertEqual(payload["messageId"], "msg-1")
                self.assertEqual(payload["message"], {"messageId": "msg-1", "isRead": True})
                self.assertEqual(calls[0][0:2], ("POST", "/messages/msg-1/modify"))
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["api_request"] = original_api_request
            gmail_module["CACHE_DIR"] = original_cache_dir

    def test_sanitize_html_blocks_unsafe_url_schemes(self):
        raw_html = """
            <body>
              <a href="file:///etc/passwd">file</a>
              <a href="qrc:/secret">qrc</a>
              <a href="ftp://example.com/file">ftp</a>
              <a href="/relative/path">relative</a>
              <a href="mailto:team@example.com">mail</a>
              <a href="https://example.com">web</a>
              <img src="file:///tmp/image.png">
              <img src="qrc:/image.png">
              <img src="data:text/html;base64,abcd">
              <img src="data:image/svg+xml;base64,abcd">
            </body>
        """

        sanitized = gmail_bridge.sanitize_html_email(raw_html, [])

        self.assertNotIn("file://", sanitized)
        self.assertNotIn("qrc:", sanitized)
        self.assertNotIn("ftp://", sanitized)
        self.assertNotIn("/relative/path", sanitized)
        self.assertNotIn("data:text/html", sanitized)
        self.assertNotIn("data:image/svg+xml", sanitized)
        self.assertIn('href="mailto:team@example.com"', sanitized)
        self.assertIn('href="https://example.com"', sanitized)
        self.assertNotIn("<img", sanitized)

    def test_extract_email_links_keeps_safe_links_and_blocks_unsafe(self):
        raw_html = """
            <body>
              <a href="https://example.com/pay?token=123">Pay now</a>
              <a href="//docs.example.com/help">Protocol relative docs</a>
              <a href="mailto:team@example.com">Email support</a>
              <a href="javascript:alert(1)">bad js</a>
              <a href="file:///etc/passwd">bad file</a>
              <a href="qrc:/secret">bad qrc</a>
              <a href="/relative/path">bad relative</a>
            </body>
        """

        links = gmail_bridge.extract_email_links(raw_html)

        self.assertEqual(
            links,
            [
                {"url": "https://example.com/pay?token=123", "label": "Pay now"},
                {"url": "https://docs.example.com/help", "label": "Protocol relative docs"},
                {"url": "mailto:team@example.com", "label": "Email support"},
            ],
        )

    def test_normalize_message_exposes_safe_links(self):
        html_body = """
            <body>
              <p>Confirm payment</p>
              <a href="https://example.com/receipt">View receipt</a>
              <a href="file:///etc/passwd">Unsafe</a>
            </body>
        """
        message = {
            "id": "msg-links",
            "labelIds": ["INBOX"],
            "snippet": "Confirm payment",
            "payload": {
                "mimeType": "text/html",
                "headers": [
                    {"name": "From", "value": "Billing <billing@example.com>"},
                    {"name": "Subject", "value": "Receipt"},
                ],
                "body": {
                    "data": base64.urlsafe_b64encode(html_body.encode("utf-8")).decode("ascii").rstrip("=")
                },
            },
        }

        normalized = gmail_bridge.normalize_message(message)

        self.assertEqual(normalized["links"], [{"url": "https://example.com/receipt", "label": "View receipt"}])
        self.assertEqual(normalized["linkCount"], 1)

    def test_remote_image_safety_blocks_local_and_private_addresses(self):
        self.assertFalse(gmail_bridge.is_safe_remote_image_url("http://127.0.0.1/open.png"))
        self.assertFalse(gmail_bridge.is_safe_remote_image_url("http://localhost/open.png"))

        def private_resolver(host, port, type=None):
            return [(None, None, None, "", ("192.168.1.20", port))]

        def public_resolver(host, port, type=None):
            return [(None, None, None, "", ("93.184.216.34", port))]

        self.assertFalse(gmail_bridge.is_safe_remote_image_url("https://images.example.com/hero.png", private_resolver))
        self.assertTrue(gmail_bridge.is_safe_remote_image_url("https://images.example.com/hero.png", public_resolver))

    def test_remote_image_loader_caches_file_urls_without_base64(self):
        gmail_module = gmail_bridge.remote_image_loader_for.__globals__
        original_cache_dir = gmail_module["CACHE_DIR"]
        original_urlopen = gmail_module["urllib"].request.urlopen
        original_safe_url = gmail_module["is_safe_remote_image_url"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["CACHE_DIR"] = pathlib.Path(temp_dir)
                gmail_module["is_safe_remote_image_url"] = lambda url: True
                image_bytes = b"\x89PNG\r\n\x1a\ncached"
                calls = []

                class Headers:
                    def get_content_type(self):
                        return "image/png"

                class Response:
                    headers = Headers()

                    def __enter__(self):
                        return self

                    def __exit__(self, exc_type, exc, tb):
                        return False

                    def read(self, size=-1):
                        return image_bytes

                def fake_urlopen(request, timeout=0):
                    calls.append(timeout)
                    return Response()

                gmail_module["urllib"].request.urlopen = fake_urlopen

                loader = gmail_bridge.remote_image_loader_for({"account": "remote@example.com"})
                src = loader("https://images.example.com/hero.png")

                self.assertTrue(src.startswith("file://"))
                self.assertFalse(src.startswith("data:"))
                self.assertLessEqual(calls[0], 1.5)
                cached_path = pathlib.Path(urllib.parse.unquote(urllib.parse.urlparse(src).path))
                self.assertEqual(cached_path.read_bytes(), image_bytes)
        finally:
            gmail_module["CACHE_DIR"] = original_cache_dir
            gmail_module["urllib"].request.urlopen = original_urlopen
            gmail_module["is_safe_remote_image_url"] = original_safe_url

    def test_render_message_for_viewer_writes_safe_html_document(self):
        gmail_module = gmail_bridge.render_message_for_viewer.__globals__
        original_fetch_message = gmail_module["fetch_message"]
        original_viewer_dir = gmail_module["VIEWER_DIR"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                gmail_module["VIEWER_DIR"] = pathlib.Path(temp_dir)

                def fake_fetch_message(message_id, load_remote_images=False, force_html=False):
                    self.assertTrue(load_remote_images)
                    self.assertTrue(force_html)
                    return {
                        "messageId": message_id,
                        "subject": "Pix receipt",
                        "fromName": "EB/Crunchyroll",
                        "timestamp": "17:52",
                        "htmlBody": '<div><img src="file:///tmp/pix.png"><p>Confirmed</p></div>',
                        "remoteImageCount": 1,
                        "remoteImagesLoadedCount": 1,
                    }

                gmail_module["fetch_message"] = fake_fetch_message

                payload = gmail_bridge.render_message_for_viewer("msg-view", load_remote_images=True)

                html_path = pathlib.Path(payload["htmlPath"])
                self.assertTrue(html_path.exists())
                rendered = html_path.read_text(encoding="utf-8")
                self.assertIn("Pix receipt", rendered)
                self.assertIn("file:///tmp/pix.png", rendered)
                self.assertNotIn("javascriptEnabled", rendered)
                self.assertEqual(payload["remoteImagesLoadedCount"], 1)
        finally:
            gmail_module["fetch_message"] = original_fetch_message
            gmail_module["VIEWER_DIR"] = original_viewer_dir

    def test_render_message_preview_image_launches_snapshot_helper(self):
        gmail_module = gmail_bridge.render_message_for_viewer.__globals__
        original_which = gmail_module["shutil"].which
        original_run = gmail_module["subprocess"].run
        original_viewer_dir = gmail_module["VIEWER_DIR"]
        original_electron_js = gmail_module["WEB_ELECTRON_SNAPSHOT_JS"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                viewer_dir = pathlib.Path(temp_dir) / "viewer"
                electron_js = pathlib.Path(temp_dir) / "web_mail_snapshot_electron.js"
                electron_js.write_text("console.log('snapshot')\n", encoding="utf-8")
                calls = []

                def fake_run(args, **kwargs):
                    calls.append((args, kwargs))
                    output_path = pathlib.Path(args[args.index("--output") + 1])
                    links_output_path = pathlib.Path(args[args.index("--links-output") + 1])
                    output_path.parent.mkdir(parents=True, exist_ok=True)
                    output_path.write_bytes(
                        b"\x89PNG\r\n\x1a\n"
                        b"\x00\x00\x00\rIHDR"
                        b"\x00\x00\x03\x34\x00\x00\x04\x00"
                    )
                    links_output_path.write_text(
                        json.dumps([
                            {
                                "url": "https://example.com/watch",
                                "label": "Watch now",
                                "x": 240,
                                "y": 180,
                                "width": 198,
                                "height": 42,
                            }
                        ]),
                        encoding="utf-8",
                    )

                    class Completed:
                        stdout = ""
                        stderr = ""

                    return Completed()

                gmail_module["VIEWER_DIR"] = viewer_dir
                gmail_module["WEB_ELECTRON_SNAPSHOT_JS"] = electron_js
                gmail_module["shutil"].which = lambda name: "/usr/bin/electron39" if name == "electron39" else None
                gmail_module["subprocess"].run = fake_run

                preview = gmail_bridge.render_message_preview_image(
                    {
                        "messageId": "msg-preview",
                        "subject": "Nubank",
                        "htmlBody": "<table><tr><td>Pix Automático</td></tr></table>",
                        "htmlRenderMode": "html",
                    }
                )

                self.assertTrue(preview["webPreviewUrl"].startswith("file://"))
                self.assertEqual(preview["webPreviewWidth"], 820)
                self.assertEqual(preview["webPreviewHeight"], 1024)
                self.assertEqual(calls[0][0][0], "/usr/bin/electron39")
                self.assertEqual(calls[0][0][1], "--no-sandbox")
                self.assertEqual(calls[0][0][2], str(electron_js))
                self.assertIn("--html", calls[0][0])
                self.assertIn("--output", calls[0][0])
                self.assertIn("--links-output", calls[0][0])
                self.assertIn("--width", calls[0][0])
                self.assertEqual(
                    preview["webPreviewLinks"],
                    [
                        {
                            "url": "https://example.com/watch",
                            "label": "Watch now",
                            "x": 240,
                            "y": 180,
                            "width": 198,
                            "height": 42,
                        }
                    ],
                )
                self.assertTrue(preview["webPreviewLinksReady"])
                self.assertTrue(pathlib.Path(urllib.parse.urlparse(preview["webPreviewUrl"]).path).exists())
        finally:
            gmail_module["shutil"].which = original_which
            gmail_module["subprocess"].run = original_run
            gmail_module["VIEWER_DIR"] = original_viewer_dir
            gmail_module["WEB_ELECTRON_SNAPSHOT_JS"] = original_electron_js

    def test_preview_link_rects_parse_qml_console_marker(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            missing_path = pathlib.Path(temp_dir) / "missing.links.json"
            stdout = "qml: __ASTREA_WEB_PREVIEW_LINKS__" + json.dumps([
                {"url": "https://example.com", "label": "Example", "x": 10, "y": 20, "width": 30, "height": 40}
            ])

            links = gmail_bridge.read_preview_link_rects(missing_path, stdout)

            self.assertEqual(
                links,
                [{"url": "https://example.com", "label": "Example", "x": 10, "y": 20, "width": 30, "height": 40}],
            )

    def test_get_message_original_html_does_not_render_web_preview_inline(self):
        gmail_module = gmail_bridge.get_message.__globals__
        original_fetch_message = gmail_module["fetch_message"]
        original_render_preview = gmail_module["render_message_preview_image"]
        try:
            gmail_module["fetch_message"] = lambda message_id, token=None, load_remote_images=False, force_html=False: {
                "messageId": message_id,
                "htmlBody": "<table><tr><td>Nubank</td></tr></table>",
                "htmlRenderMode": "html",
            }

            def fail_if_previewed(message):
                raise AssertionError("get_message must not render a web preview synchronously")

            gmail_module["render_message_preview_image"] = fail_if_previewed

            payload = gmail_bridge.get_message("msg-web-preview", force_html=True)

            self.assertNotIn("webPreviewUrl", payload["message"])
            self.assertEqual(payload["message"]["htmlRenderMode"], "html")
        finally:
            gmail_module["fetch_message"] = original_fetch_message
            gmail_module["render_message_preview_image"] = original_render_preview

    def test_preview_message_returns_web_preview_patch(self):
        gmail_module = gmail_bridge.get_message.__globals__
        original_ensure_token = gmail_module["ensure_token"]
        original_fetch_message = gmail_module["fetch_message"]
        original_render_preview = gmail_module["render_message_preview_image"]
        original_update_cache = gmail_module["update_cached_message_patch"]
        try:
            calls = []
            cache_updates = []

            def fake_fetch_message(message_id, token=None, load_remote_images=False, force_html=False):
                calls.append((message_id, token, load_remote_images, force_html))
                return {
                    "messageId": message_id,
                    "htmlBody": "<table><tr><td>Nubank</td></tr></table>",
                    "htmlRenderMode": "html",
                    "remoteImageCount": 2,
                    "remoteImagesLoadedCount": 2,
                    "remoteImagesLoaded": True,
                }

            gmail_module["ensure_token"] = lambda: {"account": "preview@example.com"}
            gmail_module["fetch_message"] = fake_fetch_message
            gmail_module["render_message_preview_image"] = lambda message: {
                "webPreviewUrl": "file:///tmp/astrea-email-preview.png",
                "webPreviewWidth": 820,
                "webPreviewHeight": 1200,
                "webPreviewLinks": [{"url": "https://example.com", "label": "Example", "x": 10, "y": 20, "width": 30, "height": 40}],
                "webPreviewLinksReady": True,
            }
            gmail_module["update_cached_message_patch"] = lambda token, message_id, patch: cache_updates.append((token, message_id, patch))

            payload = gmail_bridge.preview_message("msg-web-preview", load_remote_images=True)

            self.assertEqual(payload["action"], "preview")
            self.assertEqual(payload["messageId"], "msg-web-preview")
            self.assertEqual(calls, [("msg-web-preview", {"account": "preview@example.com"}, True, True)])
            self.assertEqual(payload["message"]["webPreviewUrl"], "file:///tmp/astrea-email-preview.png")
            self.assertEqual(payload["message"]["webPreviewHeight"], 1200)
            self.assertEqual(payload["message"]["webPreviewLinks"][0]["url"], "https://example.com")
            self.assertTrue(payload["message"]["webPreviewLinksReady"])
            self.assertEqual(payload["message"]["remoteImagesLoadedCount"], 2)
            self.assertNotIn("htmlBody", payload["message"])
            self.assertEqual(cache_updates[0][1], "msg-web-preview")
        finally:
            gmail_module["ensure_token"] = original_ensure_token
            gmail_module["fetch_message"] = original_fetch_message
            gmail_module["render_message_preview_image"] = original_render_preview
            gmail_module["update_cached_message_patch"] = original_update_cache

    def test_open_message_viewer_launches_qml6_with_generated_html(self):
        gmail_module = gmail_bridge.open_message_viewer.__globals__
        original_render = gmail_module["render_message_for_viewer"]
        original_which = gmail_module["shutil"].which
        original_popen = gmail_module["subprocess"].Popen
        original_viewer_qml = gmail_module["WEB_VIEWER_QML"]
        original_viewer_pid = gmail_module["WEB_VIEWER_PID"]
        try:
            with tempfile.TemporaryDirectory() as temp_dir:
                html_path = pathlib.Path(temp_dir) / "message.html"
                html_path.write_text("<html></html>", encoding="utf-8")
                viewer_qml = pathlib.Path(temp_dir) / "WebMailViewer.qml"
                viewer_qml.write_text("import QtQuick\nItem {}\n", encoding="utf-8")
                viewer_pid = pathlib.Path(temp_dir) / "web-preview.pid"
                launched = []

                gmail_module["WEB_VIEWER_QML"] = viewer_qml
                gmail_module["WEB_VIEWER_PID"] = viewer_pid
                gmail_module["render_message_for_viewer"] = lambda message_id, load_remote_images=True: {
                    "ok": True,
                    "provider": "gmail",
                    "messageId": message_id,
                    "htmlPath": str(html_path),
                }
                gmail_module["shutil"].which = lambda name: "/usr/bin/qml6" if name == "qml6" else None

                class FakePopen:
                    def __init__(self, args, **kwargs):
                        self.pid = 12345
                        launched.append((args, kwargs))

                gmail_module["subprocess"].Popen = FakePopen

                payload = gmail_bridge.open_message_viewer(
                    "msg-view",
                    geometry={"x": 11, "y": 22, "width": 333, "height": 444},
                )

                self.assertEqual(payload["message"], "Web preview updated")
                self.assertEqual(
                    launched[0][0],
                    [
                        "/usr/bin/qml6",
                        str(viewer_qml),
                        "--x",
                        "11",
                        "--y",
                        "22",
                        "--width",
                        "333",
                        "--height",
                        "444",
                        str(html_path),
                    ],
                )
                self.assertTrue(launched[0][1]["start_new_session"])
                self.assertEqual(viewer_pid.read_text(encoding="utf-8"), "12345")
        finally:
            gmail_module["render_message_for_viewer"] = original_render
            gmail_module["shutil"].which = original_which
            gmail_module["subprocess"].Popen = original_popen
            gmail_module["WEB_VIEWER_QML"] = original_viewer_qml
            gmail_module["WEB_VIEWER_PID"] = original_viewer_pid

    def test_create_send_payload_is_base64url_mime(self):
        payload = gmail_bridge.create_send_payload("a@example.com", "Subject", "Body")
        raw = payload["raw"]
        decoded = base64.urlsafe_b64decode(raw + "=" * (-len(raw) % 4)).decode("utf-8")

        self.assertNotIn("+", raw)
        self.assertNotIn("/", raw)
        self.assertIn("To: a@example.com", decoded)
        self.assertIn("Subject: Subject", decoded)
        self.assertIn("Body", decoded)


if __name__ == "__main__":
    unittest.main()
