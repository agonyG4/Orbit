#!/usr/bin/env python3
"""Build, install, and smoke-test Explorer from the Orbit repository root."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[1]


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
        runtime / "Astrea/Components/qmldir",
        runtime / "Astrea/Files/qmldir",
        runtime / "Astrea/I18n/qmldir",
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

    forbidden_parts = {
        "target",
        "__pycache__",
        ".pytest_cache",
        "CMakeFiles",
        ".qt",
        ".rcc",
        "QuickshellComponents",
    }
    for path in prefix.rglob("*"):
        if any(part in forbidden_parts or part.startswith("build") for part in path.parts):
            raise SystemExit(f"build/cache directory leaked into install prefix: {path}")
        if path.is_file() and path.suffix in {".pyc", ".pyo"}:
            raise SystemExit(f"compiled Python artifact leaked into install prefix: {path}")


def smoke(binary: Path, isolated_root: Path, *, portal: bool = False) -> None:
    environment = os.environ.copy()
    for name in (
        "ASTREA_ROOT",
        "ASTREA_ORBIT_DEVELOPMENT_RUNTIME_ROOT",
        "ASTREA_EXPLORER_BIN",
        "ASTREA_EXPLORER_START_PATH",
        "ASTREA_EXPLORER_REMOTE_PREFIXES",
        "ASTREA_FILE_DIALOG_OPTIONS",
        "ASTREA_FILE_DIALOG_RESULT_FILE",
        "QML2_IMPORT_PATH",
        "QML_IMPORT_PATH",
    ):
        environment.pop(name, None)

    home = isolated_root / "home"
    config = isolated_root / "xdg-config"
    data = isolated_root / "xdg-data"
    state = isolated_root / "xdg-state"
    cache = isolated_root / "xdg-cache"
    runtime_dir = isolated_root / "xdg-runtime"
    applications = data / "applications"
    for directory in (home, config, data, state, cache, runtime_dir, applications):
        directory.mkdir(parents=True, exist_ok=True)
    runtime_dir.chmod(0o700)
    environment.update(
        {
            "HOME": str(home),
            "XDG_CONFIG_HOME": str(config),
            "XDG_CONFIG_DIRS": str(config / "system-config"),
            "XDG_DATA_HOME": str(data),
            "XDG_DATA_DIRS": str(data / "system-data"),
            "XDG_STATE_HOME": str(state),
            "XDG_CACHE_HOME": str(cache),
            "XDG_RUNTIME_DIR": str(runtime_dir),
            "XDG_CURRENT_DESKTOP": "AstreaTest",
            "QT_QPA_PLATFORM": "offscreen",
        }
    )
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

    with tempfile.TemporaryDirectory(prefix="orbit-clean-install-") as temporary:
        root = Path(temporary)
        build = root / "build"
        prefix = root / "prefix"
        run(
            [
                cmake,
                "-S",
                str(REPO_ROOT),
                "-B",
                str(build),
                "-G",
                "Unix Makefiles",
                "-DCMAKE_BUILD_TYPE=Debug",
                f"-DCMAKE_INSTALL_PREFIX={prefix}",
            ]
        )
        run([cmake, "--build", str(build), "--target", "astrea-explorer", "-j2"])
        run([cmake, "--install", str(build)])

        assert_clean_prefix(prefix)
        binary = prefix / "bin/astrea-explorer"
        smoke(binary, root)
        smoke(binary, root, portal=True)

    print("clean Explorer install: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
