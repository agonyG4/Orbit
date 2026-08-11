#!/usr/bin/env python3
"""Deterministic production-source gate for the native Explorer migration."""

from pathlib import Path
import sys


KIT_ROOT = Path(__file__).resolve().parents[1]
PRODUCTION_ROOTS = [
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer",
    KIT_ROOT / "source/AstreaOS/src/System/portal",
    KIT_ROOT / "source/AstreaOS/config/hypr/system/programs.conf",
]
EXCLUDED_PARTS = {"build", "build-final-debug", "build-final-release", "target", "tests", "docs", "__pycache__"}
EXCLUDED_NAMES = {"FINAL_NATIVE_MIGRATION_JOURNAL.md", "TRANSITIONAL_QML_STATE.md"}
FORBIDDEN = (
    "import Quickshell",
    "Process {",
    "explorer_helper.py",
    "wl-copy",
    "wl-paste",
    "/usr/bin/qs",
    "qs -p",
    "/home/agony",
)


def source_files(root: Path):
    if root.is_file():
        yield root
        return
    for path in root.rglob("*"):
        if not path.is_file() or path.name in EXCLUDED_NAMES:
            continue
        if any(part in EXCLUDED_PARTS or part.startswith("build") for part in path.parts):
            continue
        yield path


def main() -> int:
    violations = []
    for root in PRODUCTION_ROOTS:
        for path in source_files(root):
            try:
                text = path.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError):
                continue
            for line_number, line in enumerate(text.splitlines(), 1):
                for marker in FORBIDDEN:
                    if marker in line:
                        violations.append(f"{path}:{line_number}: {marker}")
    if violations:
        print("native migration gate: FAIL")
        print("\n".join(violations))
        return 1
    print("native migration gate: PASS")
    print("production Explorer/portal sources contain no forbidden runtime dependency markers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
