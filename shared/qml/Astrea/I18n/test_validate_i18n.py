#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path


I18N_ROOT = Path(__file__).resolve().parent
REPOSITORY_ROOT = I18N_ROOT.parents[3]
VALIDATOR = I18N_ROOT / "validate_i18n.py"


class I18nValidatorTests(unittest.TestCase):
    def test_validator_accepts_canonical_orbit_sources(self) -> None:
        result = subprocess.run(
            [sys.executable, str(VALIDATOR)],
            cwd=REPOSITORY_ROOT,
            check=False,
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout, r"i18n validation passed \(\d+ referenced keys\)")

    def test_validator_uses_only_canonical_orbit_source_roots(self) -> None:
        source = VALIDATOR.read_text(encoding="utf-8")
        retired_roots = (
            "System" + "/i18n",
            "source" + "/AstreaOS",
            "Bench" + "/",
        )
        for retired_root in retired_roots:
            with self.subTest(retired_root=retired_root):
                self.assertNotIn(retired_root, source)
        self.assertIn('ORBIT_ROOT / "shared" / "qml" / "Astrea" / "I18N"'.lower(), source.lower())
        self.assertIn('ORBIT_ROOT / "apps" / "explorer" / "qml"'.lower(), source.lower())


if __name__ == "__main__":
    unittest.main()
