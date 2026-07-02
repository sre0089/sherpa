import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "release_tools.py"
SPEC = importlib.util.spec_from_file_location("release_tools", SCRIPT)
release_tools = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(release_tools)


class ReleaseToolsTest(unittest.TestCase):
    def test_release_validation_matches_cmake_and_changelog(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeLists.txt").write_text(
                "project(\n  sherpa\n  VERSION 1.2.3\n)\n", encoding="utf-8"
            )
            (root / "CHANGELOG.md").write_text(
                "# Changelog\n\n## [1.2.3] - 2026-07-02\n\n- Stable release.\n",
                encoding="utf-8",
            )
            notes = root / "notes.md"

            version = release_tools.validate_release(root, "v1.2.3", notes)

            self.assertEqual(version, "1.2.3")
            self.assertEqual(notes.read_text(encoding="utf-8"), "- Stable release.\n")

    def test_release_validation_rejects_mismatched_tag(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeLists.txt").write_text(
                "project(sherpa VERSION 1.2.3 LANGUAGES CXX)\n", encoding="utf-8"
            )
            (root / "CHANGELOG.md").write_text(
                "## [1.2.3]\n\n- Release.\n", encoding="utf-8"
            )

            with self.assertRaisesRegex(ValueError, "does not match"):
                release_tools.validate_release(root, "v1.2.4")

    def test_formula_uses_platform_archives_and_exact_checksums(self):
        with tempfile.TemporaryDirectory() as temporary:
            artifacts = Path(temporary)
            for platform, suffix in release_tools.PLATFORMS.items():
                (artifacts / f"sherpa-1.2.3-{platform}{suffix}").write_bytes(
                    platform.encode("utf-8")
                )

            formula = release_tools.render_formula("owner/sherpa", "1.2.3", artifacts)

            self.assertIn("class Sherpa < Formula", formula)
            self.assertIn("releases/download/v1.2.3", formula)
            self.assertIn("sherpa-1.2.3-macos-arm64.tar.gz", formula)
            self.assertIn(
                release_tools.sha256(
                    artifacts / "sherpa-1.2.3-linux-x86_64.tar.gz"
                ),
                formula,
            )
            self.assertIn('bin.install "bin/sherpa-server"', formula)

    def test_checksums_are_sorted_and_use_filenames(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            artifacts.mkdir()
            (artifacts / "second.zip").write_bytes(b"second")
            (artifacts / "first.tar.gz").write_bytes(b"first")
            output = root / "SHA256SUMS"

            release_tools.write_checksums([artifacts], output)

            lines = output.read_text(encoding="utf-8").splitlines()
            self.assertTrue(lines[0].endswith("  first.tar.gz"))
            self.assertTrue(lines[1].endswith("  second.zip"))

    def test_wheel_staging_requires_and_copies_both_executables(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            install = root / "install"
            package = root / "python"
            (install / "bin").mkdir(parents=True)
            (install / "share" / "doc" / "sherpa").mkdir(parents=True)
            (install / "share" / "doc" / "sherpa" / "LICENSE").write_text(
                "MIT\n", encoding="utf-8"
            )
            suffix = ".exe" if release_tools.os.name == "nt" else ""
            for name in ("sherpa", "sherpa-server"):
                (install / "bin" / f"{name}{suffix}").write_bytes(name.encode())

            release_tools.stage_wheel(install, package)

            self.assertTrue((package / "sherpa_cli" / "bin" / f"sherpa{suffix}").is_file())
            self.assertTrue(
                (package / "sherpa_cli" / "bin" / f"sherpa-server{suffix}").is_file()
            )
            self.assertEqual(
                (package / "sherpa_cli" / "LICENSE").read_text(encoding="utf-8"),
                "MIT\n",
            )

    def test_native_archives_are_byte_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            install = root / "install"
            (install / "bin").mkdir(parents=True)
            executable = install / "bin" / "sherpa"
            executable.write_bytes(b"native")
            executable.chmod(0o755)

            first = release_tools.create_archive(
                install, root / "first", "1.2.3", "linux-x86_64", 1_700_000_000
            )
            second = release_tools.create_archive(
                install, root / "second", "1.2.3", "linux-x86_64", 1_700_000_000
            )

            self.assertEqual(first.read_bytes(), second.read_bytes())
            with tarfile.open(first) as archive:
                member = archive.getmember(
                    "sherpa-1.2.3-linux-x86_64/bin/sherpa"
                )
                self.assertEqual(member.mode, 0o755)
                self.assertEqual(member.mtime, 1_700_000_000)

    def test_windows_archives_are_byte_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            install = root / "install"
            (install / "bin").mkdir(parents=True)
            (install / "bin" / "sherpa.exe").write_bytes(b"native")

            first = release_tools.create_archive(
                install, root / "first", "1.2.3", "windows-x86_64", 1_700_000_000
            )
            second = release_tools.create_archive(
                install, root / "second", "1.2.3", "windows-x86_64", 1_700_000_000
            )

            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as archive:
                self.assertEqual(
                    archive.read("sherpa-1.2.3-windows-x86_64/bin/sherpa.exe"),
                    b"native",
                )


if __name__ == "__main__":
    unittest.main()
