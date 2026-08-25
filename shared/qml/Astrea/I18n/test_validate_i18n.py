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


if __name__ == "__main__":
    unittest.main()
