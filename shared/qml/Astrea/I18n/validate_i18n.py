#!/usr/bin/env python3

from __future__ import annotations

import json
import re
from pathlib import Path

I18N_ROOT = Path(__file__).resolve().parent
ORBIT_ROOT = I18N_ROOT.parents[3]
CATALOG_ROOT = ORBIT_ROOT / "shared" / "qml" / "Astrea" / "I18n"
ACTIVE_PRODUCTION_QML_ROOTS = (
    ORBIT_ROOT / "apps" / "explorer" / "qml",
    ORBIT_ROOT / "shared" / "qml" / "Astrea",
)
BAD_PATTERNS = (
    r"aaao",
    r"a3",
    r"maosicas",
    r"paoblico",
    r"vadeos",
    r"seaaes",
    r"opaaes",
    r"ordenaaao",
    r"visualizaaao",
)
KEY_RE = re.compile(
    r'(?:messages\s*\[\s*|(?:\b|\.)(?:t|tr)\s*\(\s*)"([^"]+)"'
)
DYNAMIC_PREFIX_KEYS = ("settings.language.country.",)


def load(language: str) -> dict[str, str]:
    return json.loads(
        (CATALOG_ROOT / f"{language}.json").read_text(encoding="utf-8")
    )


def main():
    en = load("en_US")
    pt = load("pt_BR")
    errors: list[str] = []
    for key in set(en) | set(pt):
        lowered_key = key.lower()
        if any(re.search(pattern, lowered_key) for pattern in BAD_PATTERNS):
            errors.append(f"corrupted key: {key}")

    used: set[str] = set()
    for qml_root in ACTIVE_PRODUCTION_QML_ROOTS:
        for qml in qml_root.rglob("*.qml"):
            for match in KEY_RE.finditer(
                qml.read_text(encoding="utf-8", errors="ignore")
            ):
                used.add(match.group(1))

    for key in sorted(used):
        if key in DYNAMIC_PREFIX_KEYS:
            continue
        if key not in en:
            errors.append(f"missing in en_US: {key}")
        if key not in pt:
            errors.append(f"missing in pt_BR: {key}")

    if errors:
        print("\n".join(errors))
        raise SystemExit(1)

    print(f"i18n validation passed ({len(used)} referenced keys)")


if __name__ == "__main__":
    main()
