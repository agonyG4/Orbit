import json
import tempfile
import unittest
from pathlib import Path

import dualsense_manager as manager


class ConfigTests(unittest.TestCase):
    def test_read_config_deep_merges_nested_defaults(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            original_path = manager.CONFIG_PATH
            manager.CONFIG_PATH = str(Path(temp_dir) / "dualsense.json")
            try:
                Path(manager.CONFIG_PATH).write_text(
                    json.dumps({
                        "trigger": {
                            "side": "right",
                            "mode": "weapon"
                        },
                        "lightbar": {
                            "red": 255
                        }
                    }),
                    encoding="utf-8",
                )

                config = manager.read_config()

                self.assertEqual(config["trigger"]["side"], "right")
                self.assertEqual(config["trigger"]["mode"], "weapon")
                self.assertEqual(config["trigger"]["preset_id"], "custom")
                self.assertEqual(config["trigger"]["strength"], 5)
                self.assertEqual(config["lightbar"]["red"], 255)
                self.assertEqual(config["lightbar"]["green"], 122)
                self.assertEqual(config["audio"]["volume"], 160)
            finally:
                manager.CONFIG_PATH = original_path


if __name__ == "__main__":
    unittest.main()
