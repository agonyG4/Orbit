#!/usr/bin/env python3
"""Deterministic production-source gate for the native Explorer migration."""

from pathlib import Path
import os
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
    "waitForStarted(",
    "target/debug/explorer_backend",
    "target/release/explorer_backend",
)

REQUIRED_FILES = (
    KIT_ROOT / "source/AstreaOS/src/bin/astrea-launch",
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.cpp",
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native/src/services/desktop_file_id.cpp",
)

REQUIRED_MARKERS = {
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native/CMakeLists.txt": (
        "astrea_explorer_rust_components",
        "ASTREA_EXPLORER_BACKEND_ARTIFACT",
        "ASTREA_EXPLORER_PORTAL_ARTIFACT",
        "ASTREA_EXPLORER_LAUNCH_PROGRAM",
    ),
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native/src/runtime/explorer_runtime_paths.h": (
        "resourceRootValid",
        "normalRuntimeReady",
        "portalRuntimeReady",
    ),
    KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native/src/services/mime_apps_service.cpp": (
        "QLockFile",
        "QSaveFile",
        "[Default Applications]",
        "[Added Associations]",
    ),
    KIT_ROOT / "source/AstreaOS/src/System/portal/src/main.rs": (
        "org.freedesktop.impl.portal.Request",
        "MAX_CONCURRENT_DIALOGS",
        "async fn open_file",
    ),
}


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
    for path in REQUIRED_FILES:
        if not path.is_file():
            violations.append(f"missing required production file: {path}")
    launch_provider = KIT_ROOT / "source/AstreaOS/src/bin/astrea-launch"
    if launch_provider.is_file() and not os.access(launch_provider, os.X_OK):
        violations.append(f"required launch provider is not executable: {launch_provider}")

    for path, markers in REQUIRED_MARKERS.items():
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            violations.append(f"could not read required production file: {path}")
            continue
        for marker in markers:
            if marker not in text:
                violations.append(f"{path}: missing required marker {marker}")

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
