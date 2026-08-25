from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class QmlContractTests(unittest.TestCase):
    def test_main_window_exposes_preset_filters_and_offline_copy(self):
        source = (ROOT / "GamepadWindow.qml").read_text(encoding="utf-8")

        self.assertIn("presetFilter", source)
        self.assertIn("categoryCatalog", source)
        self.assertIn("Saved offline", source)
        self.assertIn("DUALSENSE_SECTION", source)

    def test_panels_expose_lightbar_presets_and_bridge_metadata(self):
        lightbar = (ROOT / "ui/panels/LightbarPanel.qml").read_text(encoding="utf-8")
        device = (ROOT / "ui/panels/DevicePanel.qml").read_text(encoding="utf-8")

        self.assertIn("colorPresets", lightbar)
        self.assertIn("setLightbarColor", lightbar)
        self.assertIn("backend.binary", device)
        self.assertIn("backend.configPath", device)


if __name__ == "__main__":
    unittest.main()
