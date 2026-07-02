"""Launchers for the native Sherpa executables bundled in the platform wheel."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


def _executable(name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    executable = Path(__file__).resolve().parent / "bin" / f"{name}{suffix}"
    if not executable.is_file():
        raise RuntimeError(f"bundled Sherpa executable is missing: {executable}")
    return executable


def _run(name: str) -> None:
    command = [str(_executable(name)), *sys.argv[1:]]
    if os.name != "nt":
        os.execv(command[0], command)
    raise SystemExit(subprocess.call(command))


def main() -> None:
    _run("sherpa")


def server_main() -> None:
    _run("sherpa-server")
