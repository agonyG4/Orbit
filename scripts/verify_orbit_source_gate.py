#!/usr/bin/env python3
"""Check the small set of invariants that keep Orbit source-oriented."""

from __future__ import annotations

from pathlib import Path, PurePosixPath
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
GENERATED_PARTS = {
    "target",
    "__pycache__",
    ".pytest_cache",
    "CMakeFiles",
    ".qt",
    ".rcc",
}
SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".qml", ".js", ".rs", ".py", ".sh", ".toml"}
PRODUCTION_ROOTS = (
    ROOT / "apps/explorer/src",
    ROOT / "apps/explorer/qml",
    ROOT / "apps/explorer/backend/src",
    ROOT / "apps/explorer/backend/scripts",
    ROOT / "shared/qml",
    ROOT / "services",
)
REQUIRED_PATHS = (
    "CMakeLists.txt",
    "Cargo.toml",
    "Cargo.lock",
    "CMakePresets.json",
    "apps/explorer/CMakeLists.txt",
    "apps/explorer/qml/Main.qml",
    "apps/explorer/qml/native/NativeBootstrap.qml",
    "apps/explorer/src/main.cpp",
    "apps/explorer/backend/Cargo.toml",
    "services/launch/Cargo.toml",
    "services/filechooser-portal/Cargo.toml",
    "services/session/astrea-services.sh",
    "shared/qml/Astrea/Components/qmldir",
    "shared/qml/Astrea/Files/qmldir",
    "shared/qml/Astrea/I18n/qmldir",
    "shared/qml/Astrea/I18n/test_i18n.py",
    "shared/qml/Astrea/I18n/test_validate_i18n.py",
    "shared/qml/Astrea/I18n/validate_i18n.py",
    "scripts/verify_explorer_clean_install.py",
)
FORBIDDEN_MARKERS = (
    "import Quickshell",
    "Process {",
    "explorer_helper.py",
    "wl-copy",
    "wl-paste",
    "/usr/bin/qs",
    "qs -p",
    "/home/agony",
    "waitForStarted(",
    "target/debug/explorer_backend",
    "target/release/explorer_backend",
    "Bench/",
    "source/AstreaOS",
)


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [line for line in result.stdout.splitlines() if line]


def source_files(root: Path):
    if not root.exists():
        return
    for path in root.rglob("*"):
        if path.is_file() and path.suffix in SOURCE_SUFFIXES:
            yield path


def main() -> int:
    violations: list[str] = []

    if (ROOT / "Bench").exists():
        violations.append("legacy migration directory must be absent: Bench/")
    if (ROOT / "Old").exists():
        violations.append("legacy directory must be lowercase: Old/")
    if not (ROOT / "old").is_dir():
        violations.append("legacy source directory is missing: old/")
    if (ROOT / "apps/explorer/native").exists():
        violations.append("Explorer native nesting must be absent: apps/explorer/native/")

    for relative in REQUIRED_PATHS:
        if not (ROOT / relative).exists():
            violations.append(f"missing required Orbit path: {relative}")

    for relative in tracked_files():
        path = PurePosixPath(relative)
        if any(part in GENERATED_PARTS or part.startswith("build") for part in path.parts):
            violations.append(f"generated path is tracked: {relative}")
        if path.name in {"CMakeCache.txt", "cmake_install.cmake"}:
            violations.append(f"generated CMake file is tracked: {relative}")

    for source_root in (ROOT / "apps", ROOT / "shared", ROOT / "services"):
        for path in source_root.rglob("*"):
            if path.is_symlink():
                violations.append(f"source-tree symlink is forbidden: {path.relative_to(ROOT)}")

    for root in PRODUCTION_ROOTS:
        for path in source_files(root):
            try:
                text = path.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError):
                continue
            for line_number, line in enumerate(text.splitlines(), 1):
                for marker in FORBIDDEN_MARKERS:
                    if marker in line:
                        violations.append(f"{path.relative_to(ROOT)}:{line_number}: {marker}")

    if violations:
        print("Orbit source gate: FAIL")
        print("\n".join(violations))
        return 1

    print("Orbit source gate: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
