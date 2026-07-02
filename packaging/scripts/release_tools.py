#!/usr/bin/env python3
"""Deterministic release validation and Homebrew formula generation."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import zipfile


PLATFORMS = {
    "linux-x86_64": ".tar.gz",
    "macos-arm64": ".tar.gz",
    "macos-x86_64": ".tar.gz",
}


def project_version(cmake_file: Path) -> str:
    contents = cmake_file.read_text(encoding="utf-8")
    match = re.search(
        r"project\s*\(\s*sherpa\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
        contents,
        re.IGNORECASE | re.MULTILINE,
    )
    if not match:
        raise ValueError(f"could not find Sherpa project version in {cmake_file}")
    return match.group(1)


def changelog_notes(changelog: Path, version: str) -> str:
    contents = changelog.read_text(encoding="utf-8")
    heading = re.compile(
        rf"^## \[{re.escape(version)}\](?: - [0-9]{{4}}-[0-9]{{2}}-[0-9]{{2}})?\s*$",
        re.MULTILINE,
    )
    match = heading.search(contents)
    if not match:
        raise ValueError(f"CHANGELOG.md has no release section for {version}")
    next_heading = re.search(r"^## \[", contents[match.end() :], re.MULTILINE)
    end = match.end() + next_heading.start() if next_heading else len(contents)
    notes = contents[match.end() : end].strip()
    if not notes:
        raise ValueError(f"CHANGELOG.md release section for {version} is empty")
    return notes + "\n"


def validate_release(
    root: Path,
    tag: str,
    notes_output: Path | None = None,
    github_environment: Path | None = None,
) -> str:
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise ValueError(f"release tag must have form vMAJOR.MINOR.PATCH: {tag}")
    version = project_version(root / "CMakeLists.txt")
    if tag != f"v{version}":
        raise ValueError(f"tag {tag} does not match CMake project version {version}")
    notes = changelog_notes(root / "CHANGELOG.md", version)
    if notes_output:
        notes_output.write_text(notes, encoding="utf-8", newline="\n")
    if github_environment:
        epoch = subprocess.check_output(
            ["git", "-C", str(root), "log", "-1", "--format=%ct"],
            text=True,
        ).strip()
        if not epoch.isdigit():
            raise ValueError("could not determine SOURCE_DATE_EPOCH from the release commit")
        with github_environment.open("a", encoding="utf-8", newline="\n") as stream:
            stream.write(f"SHERPA_PACKAGE_VERSION={version}\n")
            stream.write(f"SOURCE_DATE_EPOCH={epoch}\n")
    return version


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def archive(artifact_directory: Path, version: str, platform: str) -> Path:
    suffix = PLATFORMS[platform]
    path = artifact_directory / f"sherpa-{version}-{platform}{suffix}"
    if not path.is_file():
        raise ValueError(f"required release archive is missing: {path}")
    return path


def render_formula(repository: str, version: str, artifact_directory: Path) -> str:
    base_url = f"https://github.com/{repository}/releases/download/v{version}"
    archives = {
        platform: archive(artifact_directory, version, platform) for platform in PLATFORMS
    }
    checksums = {platform: sha256(path) for platform, path in archives.items()}

    return f'''class Sherpa < Formula
  desc "Local-first, evidence-backed codebase intelligence"
  homepage "https://github.com/{repository}"
  version "{version}"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "{base_url}/{archives["macos-arm64"].name}"
      sha256 "{checksums["macos-arm64"]}"
    else
      url "{base_url}/{archives["macos-x86_64"].name}"
      sha256 "{checksums["macos-x86_64"]}"
    end
  end

  on_linux do
    on_intel do
      url "{base_url}/{archives["linux-x86_64"].name}"
      sha256 "{checksums["linux-x86_64"]}"
    end
  end

  def install
    bin.install "bin/sherpa"
    bin.install "bin/sherpa-server"
  end

  test do
    assert_equal version.to_s, shell_output("#{{bin}}/sherpa --version").strip
  end
end
'''


def stage_wheel(install_directory: Path, package_directory: Path) -> None:
    binary_directory = package_directory / "sherpa_cli" / "bin"
    binary_directory.mkdir(parents=True, exist_ok=True)
    for existing in binary_directory.iterdir():
        if existing.name != "__init__.py":
            if existing.is_dir():
                shutil.rmtree(existing)
            else:
                existing.unlink()
    suffix = ".exe" if os.name == "nt" else ""
    for name in ("sherpa", "sherpa-server"):
        source = install_directory / "bin" / f"{name}{suffix}"
        if not source.is_file():
            raise ValueError(f"installed executable is missing: {source}")
        destination = binary_directory / source.name
        shutil.copy2(source, destination)
        destination.chmod(destination.stat().st_mode | 0o111)
    license_file = install_directory / "share" / "doc" / "sherpa" / "LICENSE"
    if not license_file.is_file():
        raise ValueError(f"installed license is missing: {license_file}")
    shutil.copy2(license_file, package_directory / "sherpa_cli" / "LICENSE")


def write_checksums(paths: list[Path], output: Path) -> None:
    files = sorted(
        (path for path in paths for path in ([path] if path.is_file() else path.glob("*"))),
        key=lambda path: path.name,
    )
    if not files:
        raise ValueError("no release artifacts were found for checksums")
    if len({path.name for path in files}) != len(files):
        raise ValueError("release artifact filenames must be unique")
    contents = "".join(f"{sha256(path)}  {path.name}\n" for path in files)
    output.write_text(contents, encoding="utf-8", newline="\n")


def archive_name(version: str, platform: str) -> str:
    suffix = ".zip" if platform.startswith("windows-") else ".tar.gz"
    return f"sherpa-{version}-{platform}{suffix}"


def normalized_mode(path: Path) -> int:
    return 0o755 if path.is_dir() or os.access(path, os.X_OK) else 0o644


def create_archive(
    install_directory: Path,
    output_directory: Path,
    version: str,
    platform: str,
    epoch: int,
) -> Path:
    if not (install_directory / "bin").is_dir():
        raise ValueError(f"invalid install tree: {install_directory}")
    output_directory.mkdir(parents=True, exist_ok=True)
    output = output_directory / archive_name(version, platform)
    root_name = output.name
    for suffix in (".tar.gz", ".zip"):
        if root_name.endswith(suffix):
            root_name = root_name[: -len(suffix)]
            break
    paths = [install_directory, *sorted(install_directory.rglob("*"))]

    if output.suffix == ".zip":
        earliest_zip_epoch = 315532800  # 1980-01-01 UTC
        timestamp = datetime.fromtimestamp(max(epoch, earliest_zip_epoch), timezone.utc)
        date_time = timestamp.timetuple()[:6]
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zip:
            for path in paths:
                relative = path.relative_to(install_directory)
                name = root_name if not relative.parts else f"{root_name}/{relative.as_posix()}"
                if path.is_dir():
                    name += "/"
                info = zipfile.ZipInfo(name, date_time=date_time)
                info.create_system = 3
                info.external_attr = (stat.S_IFDIR if path.is_dir() else stat.S_IFREG) << 16
                info.external_attr |= normalized_mode(path) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                zip.writestr(info, b"" if path.is_dir() else path.read_bytes())
    else:
        with output.open("wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch) as compressed:
                with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
                    for path in paths:
                        relative = path.relative_to(install_directory)
                        name = (
                            root_name
                            if not relative.parts
                            else f"{root_name}/{relative.as_posix()}"
                        )
                        info = tar.gettarinfo(path, arcname=name)
                        info.uid = 0
                        info.gid = 0
                        info.uname = "root"
                        info.gname = "root"
                        info.mtime = epoch
                        info.mode = normalized_mode(path)
                        if path.is_file():
                            with path.open("rb") as stream:
                                tar.addfile(info, stream)
                        else:
                            tar.addfile(info)
    return output


def command_validate(arguments: argparse.Namespace) -> None:
    github_environment = None
    if arguments.github_environment:
        value = os.environ.get("GITHUB_ENV")
        if not value:
            raise ValueError("GITHUB_ENV is not set")
        github_environment = Path(value)
    version = validate_release(
        arguments.root, arguments.tag, arguments.notes_output, github_environment
    )
    print(version)


def command_formula(arguments: argparse.Namespace) -> None:
    formula = render_formula(
        arguments.repository, arguments.version, arguments.artifact_directory
    )
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(formula, encoding="utf-8", newline="\n")


def command_stage_wheel(arguments: argparse.Namespace) -> None:
    stage_wheel(arguments.install_directory, arguments.package_directory)


def command_checksums(arguments: argparse.Namespace) -> None:
    write_checksums(arguments.paths, arguments.output)


def command_archive(arguments: argparse.Namespace) -> None:
    epoch_value = arguments.epoch or os.environ.get("SOURCE_DATE_EPOCH")
    if not epoch_value or not str(epoch_value).isdigit():
        raise ValueError("SOURCE_DATE_EPOCH or --epoch is required")
    create_archive(
        arguments.install_directory,
        arguments.output_directory,
        arguments.version,
        arguments.platform,
        int(epoch_value),
    )


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    subcommands = result.add_subparsers(required=True)

    validate = subcommands.add_parser("validate")
    validate.add_argument("--root", type=Path, required=True)
    validate.add_argument("--tag", required=True)
    validate.add_argument("--notes-output", type=Path)
    validate.add_argument("--github-environment", action="store_true")
    validate.set_defaults(function=command_validate)

    formula = subcommands.add_parser("formula")
    formula.add_argument("--repository", required=True)
    formula.add_argument("--version", required=True)
    formula.add_argument("--artifact-directory", type=Path, required=True)
    formula.add_argument("--output", type=Path, required=True)
    formula.set_defaults(function=command_formula)

    stage = subcommands.add_parser("stage-wheel")
    stage.add_argument("--install-directory", type=Path, required=True)
    stage.add_argument("--package-directory", type=Path, required=True)
    stage.set_defaults(function=command_stage_wheel)

    checksums = subcommands.add_parser("checksums")
    checksums.add_argument("--output", type=Path, required=True)
    checksums.add_argument("paths", type=Path, nargs="+")
    checksums.set_defaults(function=command_checksums)

    archive_command = subcommands.add_parser("archive")
    archive_command.add_argument("--install-directory", type=Path, required=True)
    archive_command.add_argument("--output-directory", type=Path, required=True)
    archive_command.add_argument("--version", required=True)
    archive_command.add_argument("--platform", required=True)
    archive_command.add_argument("--epoch")
    archive_command.set_defaults(function=command_archive)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        arguments.function(arguments)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
