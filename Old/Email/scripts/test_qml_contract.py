#!/usr/bin/env python3
import pathlib
import re
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]


class QmlContractTests(unittest.TestCase):
    def read_qml(self, relative_path):
        return (PROJECT_ROOT / relative_path).read_text(encoding="utf-8")

    def test_message_detail_opens_original_html_natively(self):
        main_qml = self.read_qml("Main.qml")
        match = re.search(
            r"function\s+requestMessageDetail\s*\([^)]*\)\s*\{(?P<body>.*?)\n    \}",
            main_qml,
            re.S,
        )
        self.assertIsNotNone(match)
        self.assertIn("gmail.get(messageId, false, true)", match.group("body"))
        self.assertIn('current.htmlRenderMode === "reader"', match.group("body"))
        self.assertIn("root.requestMessagePreview(messageId, false)", match.group("body"))

    def test_message_detail_queues_async_web_preview(self):
        main_qml = self.read_qml("Main.qml")
        self.assertIn("function requestMessagePreview(messageId, loadImages, force)", main_qml)
        self.assertIn("gmail.preview(messageId, loadImages === true)", main_qml)
        self.assertIn("onPreviewReady", main_qml)

        client_qml = self.read_qml("services/EmailCliClient.qml")
        self.assertIn("signal previewReady(var payload)", client_qml)
        self.assertIn("function preview(messageId, loadImages)", client_qml)
        self.assertIn('"preview"', client_qml)

    def test_new_mail_notifications_are_polled(self):
        main_qml = self.read_qml("Main.qml")
        self.assertIn("id: emailNotifyPoll", main_qml)
        self.assertIn("root.mailServiceEnabled", main_qml)
        self.assertIn("function desktopNotificationAllowedForPoll()", main_qml)
        self.assertIn("gmail.notify(20, root.desktopNotificationAllowedForPoll(), root.copyCodesEnabled, root.islandCodesEnabled)", main_qml)
        self.assertIn("onNotifyReady", main_qml)
        self.assertIn("mail.prependMessages(payload.events || [],", main_qml)
        notify_match = re.search(
            r"onNotifyReady:\s*payload\s*=>\s*\{(?P<body>.*?)\n        \}",
            main_qml,
            re.S,
        )
        self.assertIsNotNone(notify_match)
        self.assertIn("networkRefreshDelay.restart()", notify_match.group("body"))
        self.assertNotIn("root.refreshMailboxFromNetwork(true)", notify_match.group("body"))

        store_qml = self.read_qml("state/MailStore.qml")
        self.assertIn("function prependMessages(messages, status)", store_qml)

        client_qml = self.read_qml("services/EmailCliClient.qml")
        self.assertIn("signal notifyReady(var payload)", client_qml)
        self.assertIn("function notify(limit, desktopNotify, copyCodes, islandNotify)", client_qml)
        self.assertIn('"--no-clipboard"', client_qml)
        self.assertIn('"--no-island"', client_qml)
        self.assertIn('"notify"', client_qml)

    def test_email_queue_coalesces_duplicate_list_refreshes(self):
        client_qml = self.read_qml("services/EmailCliClient.qml")

        self.assertIn("function _listRequestKey(args)", client_qml)
        self.assertIn('if (action === "list")', client_qml)
        self.assertIn("item.action === \"list\" && _listRequestKey(item.args) === requestKey", client_qml)

    def test_mail_store_indexes_messages_and_uses_id_sets(self):
        store_qml = self.read_qml("state/MailStore.qml")

        self.assertIn("property var messageIndex", store_qml)
        self.assertIn("messageIndex[messageId]", store_qml)
        self.assertIn("function appendUniqueId(ids, idSet, messageId)", store_qml)
        self.assertIn("const idSet = ({})", store_qml)

    def test_main_window_is_freely_resizable(self):
        main_qml = self.read_qml("Main.qml")
        self.assertNotIn("maximumWidth:", main_qml)
        self.assertNotIn("maximumHeight:", main_qml)
        self.assertIn("component ResizeHandle: MouseArea", main_qml)
        self.assertIn("root.startSystemResize(edges)", main_qml)
        self.assertIn("Qt.LeftEdge | Qt.TopEdge", main_qml)
        self.assertIn("Qt.RightEdge | Qt.BottomEdge", main_qml)

    def test_mail_settings_panel_exposes_notification_toggles(self):
        setup_qml = self.read_qml("components/SetupPanel.qml")
        self.assertIn("property bool mailServiceEnabled", setup_qml)
        self.assertIn("property bool copyCodesEnabled", setup_qml)
        self.assertIn("property bool islandCodesEnabled", setup_qml)
        self.assertIn("property bool desktopNotificationsEnabled", setup_qml)
        self.assertIn("signal settingToggled(string key, bool value)", setup_qml)
        self.assertIn("Astrea.ToggleSwitch", setup_qml)
        self.assertIn("Mail settings", setup_qml)
        self.assertIn("Service and automation", setup_qml)
        self.assertIn("Mail service", setup_qml)
        self.assertIn("Copy security codes", setup_qml)
        self.assertIn("Dynamic Island", setup_qml)
        self.assertIn("Gmail account", setup_qml)
        self.assertLess(setup_qml.index("Service and automation"), setup_qml.index("Gmail account"))

    def test_main_loads_and_saves_mail_settings(self):
        main_qml = self.read_qml("Main.qml")
        client_qml = self.read_qml("services/EmailCliClient.qml")
        self.assertIn("property bool mailServiceEnabled", main_qml)
        self.assertIn("function applyEmailSettings(settings)", main_qml)
        self.assertIn("function setEmailSetting(key, value)", main_qml)
        self.assertIn("onSettingsReady", main_qml)
        self.assertIn("gmail.refreshSettings()", main_qml)
        self.assertIn("gmail.setSetting(key, value)", main_qml)
        self.assertIn("signal settingsReady(var payload)", client_qml)
        self.assertIn("function refreshSettings()", client_qml)
        self.assertIn("function setSetting(key, value)", client_qml)

    def test_message_detail_has_no_show_original_action(self):
        detail_qml = self.read_qml("components/MessageDetailPane.qml")
        self.assertNotIn("Show original", detail_qml)
        self.assertNotIn("openOriginalRequested", detail_qml)

    def test_message_detail_prefers_web_preview_snapshot(self):
        detail_qml = self.read_qml("components/MessageDetailPane.qml")
        self.assertIn("function hasWebPreview()", detail_qml)
        self.assertIn("id: webPreviewFrame", detail_qml)
        self.assertIn("id: webPreviewImage", detail_qml)
        self.assertIn("source: pane.message.webPreviewUrl || \"\"", detail_qml)
        self.assertIn("text: pane.fallbackBodyText()", detail_qml)
        self.assertIn("if (pane.hasWebPreview())", detail_qml)
        self.assertIn("font-size:1px", detail_qml)
        self.assertIn("function waitingForWebPreview()", detail_qml)
        self.assertIn("TextEdit.PlainText", detail_qml)

    def test_message_detail_overlays_clickable_web_preview_links(self):
        detail_qml = self.read_qml("components/MessageDetailPane.qml")
        self.assertIn("function webPreviewLinkRects()", detail_qml)
        self.assertIn("model: pane.webPreviewLinkRects()", detail_qml)
        self.assertIn("acceptedButtons: Qt.LeftButton | Qt.RightButton", detail_qml)
        self.assertIn("pane.openExternalLink(modelData.url || \"\")", detail_qml)
        self.assertIn("pane.copyLink(previewLinkMenu.linkUrl)", detail_qml)
        self.assertIn("cursorShape: Qt.PointingHandCursor", detail_qml)

    def test_mail_store_preserves_web_preview_fields(self):
        store_qml = self.read_qml("state/MailStore.qml")
        self.assertIn("webPreviewUrl: item.webPreviewUrl || \"\"", store_qml)
        self.assertIn("webPreviewWidth", store_qml)
        self.assertIn("webPreviewHeight", store_qml)
        self.assertIn("webPreviewLinks: decodeLinks(item.webPreviewLinksJson)", store_qml)
        self.assertIn("webPreviewLinksJson", store_qml)
        self.assertIn("webPreviewLinksReady", store_qml)

    def test_main_refreshes_old_web_preview_without_link_map(self):
        main_qml = self.read_qml("Main.qml")
        self.assertIn("const previewLinksReady = !!current.webPreviewLinksReady", main_qml)
        self.assertIn("if (!force && String(current.webPreviewUrl || \"\") !== \"\" && previewLinksReady)", main_qml)

    def test_message_detail_supports_copyable_text_and_safe_links(self):
        detail_qml = self.read_qml("components/MessageDetailPane.qml")
        self.assertIn("TextEdit {", detail_qml)
        self.assertIn("id: bodyText", detail_qml)
        self.assertIn("readOnly: true", detail_qml)
        self.assertIn("selectByMouse: true", detail_qml)
        self.assertIn("persistentSelection: true", detail_qml)
        self.assertIn("function copySelectedText()", detail_qml)
        self.assertIn("function copyAllText()", detail_qml)
        self.assertIn("function copyLink(link)", detail_qml)
        self.assertIn("function openExternalLink(link)", detail_qml)
        self.assertIn("function safeExternalLink(link)", detail_qml)
        self.assertIn("function linkList()", detail_qml)

    def test_mail_store_preserves_message_links(self):
        store_qml = self.read_qml("state/MailStore.qml")
        self.assertIn("function encodeLinks(value)", store_qml)
        self.assertIn("function decodeLinks(value)", store_qml)
        self.assertIn("links: decodeLinks(item.linksJson)", store_qml)
        self.assertIn("linksJson: item.linksJson || encodeLinks(item.links)", store_qml)
        self.assertIn("linkCount", store_qml)

    def test_mail_store_keeps_index_across_folder_pages(self):
        store_qml = self.read_qml("state/MailStore.qml")
        main_qml = self.read_qml("Main.qml")
        replace_match = re.search(
            r"function\s+replaceMessages\s*\([^)]*\)\s*\{(?P<body>.*?)\n    \}",
            store_qml,
            re.S,
        )
        self.assertIsNotNone(replace_match)
        self.assertIn("property var visibleMessageIds", store_qml)
        self.assertIn("property bool visibleMessageIdsExplicit", store_qml)
        self.assertIn("function upsertMessage(item, forceFull)", store_qml)
        self.assertIn("if (visibleMessageIdsExplicit)", store_qml)
        self.assertIn("visibleMessageIds = ids", replace_match.group("body"))
        self.assertIn("visibleMessageIdsExplicit = true", replace_match.group("body"))
        self.assertNotIn("mailModel.clear()", replace_match.group("body"))
        self.assertIn("const loaded = mail.visibleMessages.count", main_qml)
        self.assertIn("root.mailResultSizeEstimate > mail.visibleMessages.count", main_qml)


if __name__ == "__main__":
    unittest.main()
