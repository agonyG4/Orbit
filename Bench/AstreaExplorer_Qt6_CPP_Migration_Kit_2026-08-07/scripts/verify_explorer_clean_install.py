#!/usr/bin/env python3
"""Build, install, and smoke-test the Explorer from a clean prefix."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


KIT_ROOT = Path(__file__).resolve().parents[1]
NATIVE_ROOT = KIT_ROOT / "source/AstreaOS/src/Apps/Explorer/native"


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    completed = subprocess.run(command, cwd=cwd, env=env, text=True)
    if completed.returncode:
        raise SystemExit(completed.returncode)


def required_files(prefix: Path) -> list[Path]:
    runtime = prefix / "share/Astrea"
    return [
        prefix / "bin/astrea-explorer",
        runtime / "bin/astrea-launch",
        runtime / "bin/astrea-filechooser-portal",
        runtime / "Core/bridge/apps/explorer_backend",
        runtime / "Apps/Explorer/Main.qml",
        runtime / "Apps/Explorer/qmldir",
        runtime / "Apps/Explorer/PortalDialog.qml",
        runtime / "System/services/astrea-services.sh",
    ]


def assert_clean_prefix(prefix: Path) -> None:
    missing = [path for path in required_files(prefix) if not path.is_file()]
    if missing:
        raise SystemExit("clean install missing required files:\n" + "\n".join(map(str, missing)))

    executable_files = [
        prefix / "bin/astrea-explorer",
        prefix / "share/Astrea/bin/astrea-launch",
        prefix / "share/Astrea/bin/astrea-filechooser-portal",
        prefix / "share/Astrea/Core/bridge/apps/explorer_backend",
        prefix / "share/Astrea/System/services/astrea-services.sh",
    ]
    for path in executable_files:
        if not os.access(path, os.X_OK):
            raise SystemExit(f"installed required executable is not executable: {path}")

    forbidden_parts = {"build", "target", "__pycache__", "QuickshellComponents"}
    forbidden_suffixes = {".pyc", ".pyo"}
    for path in prefix.rglob("*"):
        if any(part in forbidden_parts or part.startswith("build") for part in path.parts):
            raise SystemExit(f"build/cache directory leaked into install prefix: {path}")
        if path.is_file() and path.suffix in forbidden_suffixes:
            raise SystemExit(f"compiled Python artifact leaked into install prefix: {path}")


def smoke(binary: Path, runtime: Path, *, portal: bool = False) -> None:
    environment = os.environ.copy()
    environment.pop("QML2_IMPORT_PATH", None)
    environment["ASTREA_ROOT"] = str(runtime)
    environment["QT_QPA_PLATFORM"] = "offscreen"
    command = [str(binary)]
    if portal:
        command.append("--portal")
    command.append("--self-test")
    completed = subprocess.run(
        command,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=30,
    )
    if completed.returncode:
        print(completed.stdout, file=sys.stderr)
        raise SystemExit(f"installed {'portal ' if portal else ''}startup smoke failed")


def main() -> int:
    cmake = shutil.which("cmake")
    if cmake is None:
        raise SystemExit("cmake is required")
    with tempfile.TemporaryDirectory(prefix="astrea-explorer-clean-install-") as temporary:
        root = Path(temporary)
        build = root / "build"
        prefix = root / "prefix"
        run([cmake, "-S", str(NATIVE_ROOT), "-B", str(build), "-G", "Unix Makefiles",
             "-DCMAKE_BUILD_TYPE=Debug", f"-DCMAKE_INSTALL_PREFIX={prefix}"])
        run([cmake, "--build", str(build), "--target", "astrea-explorer", "-j2"])
        run([cmake, "--install", str(build)])

        assert_clean_prefix(prefix)
        runtime = prefix / "share/Astrea"
        binary = prefix / "bin/astrea-explorer"
        smoke(binary, runtime)
        smoke(binary, runtime, portal=True)

    print("clean Explorer install: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
