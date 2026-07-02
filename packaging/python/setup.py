import os
from pathlib import Path

from setuptools import setup
from wheel.bdist_wheel import bdist_wheel


class PlatformWheel(bdist_wheel):
    def finalize_options(self):
        super().finalize_options()
        self.root_is_pure = False

    def get_tag(self):
        platform = os.environ.get("SHERPA_WHEEL_PLATFORM")
        if not platform:
            raise RuntimeError("SHERPA_WHEEL_PLATFORM is required")
        return "py3", "none", platform


version = os.environ.get("SHERPA_PACKAGE_VERSION")
if not version:
    raise RuntimeError("SHERPA_PACKAGE_VERSION is required")

repository_root = Path(__file__).resolve().parents[2]

setup(
    name="sherpa-code",
    version=version,
    description="Local-first, evidence-backed codebase intelligence CLI",
    long_description=(repository_root / "README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    url="https://github.com/sre0089/sherpa",
    project_urls={
        "Issues": "https://github.com/sre0089/sherpa/issues",
        "Source": "https://github.com/sre0089/sherpa",
    },
    license="MIT",
    python_requires=">=3.9",
    packages=["sherpa_cli", "sherpa_cli.bin"],
    package_data={"sherpa_cli": ["bin/*", "LICENSE"]},
    include_package_data=True,
    entry_points={
        "console_scripts": [
            "sherpa=sherpa_cli:main",
            "sherpa-server=sherpa_cli:server_main",
        ]
    },
    cmdclass={"bdist_wheel": PlatformWheel},
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Environment :: Console",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Topic :: Software Development",
    ],
)
