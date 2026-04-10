#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERSION_FILE = ROOT / "VERSION"
CHANGELOG_FILE = ROOT / "CHANGELOG.md"

SEMVER_RE = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)

RELEVANT_PREFIXES = (
    "include/",
    "src/",
    "scripts/",
    "guides/",
    "docs-site/docs/",
)
RELEVANT_FILES = {
    "README.md",
    "platformio.ini",
    "platformio_profile.h",
}
RELEASE_METADATA_FILES = {
    "CHANGELOG.md",
    "VERSION",
}


def fail(message: str) -> None:
    print(f"release-metadata check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        fail(f"missing required file: {path.relative_to(ROOT)}")
    except OSError as exc:
        fail(f"could not read {path.relative_to(ROOT)}: {exc}")


def changed_files() -> set[str]:
    base = os.getenv("CI_MERGE_REQUEST_DIFF_BASE_SHA") or os.getenv(
        "CI_COMMIT_BEFORE_SHA"
    )
    head = os.getenv("CI_COMMIT_SHA") or "HEAD"
    if not base or set(base) == {"0"}:
        return set()

    try:
        subprocess.run(
            ["git", "cat-file", "-e", f"{base}^{{commit}}"],
            cwd=ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        print(
            f"release-metadata check: diff base {base} is not available locally, skipping changed-file enforcement",
            file=sys.stderr,
        )
        return set()

    result = subprocess.run(
        ["git", "diff", "--name-only", f"{base}..{head}"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return {line.strip() for line in result.stdout.splitlines() if line.strip()}


def requires_changelog_update(paths: set[str]) -> bool:
    content_paths = paths - RELEASE_METADATA_FILES
    return any(
        path in RELEVANT_FILES or path.startswith(RELEVANT_PREFIXES)
        for path in content_paths
    )


def main() -> None:
    version = read_text(VERSION_FILE).strip()
    if not version:
        fail("VERSION must not be empty")
    if not SEMVER_RE.fullmatch(version):
        fail(f"VERSION must contain a valid Semantic Version, got {version!r}")

    changelog = read_text(CHANGELOG_FILE)
    if "## [Unreleased]" not in changelog:
        fail("CHANGELOG.md must contain an [Unreleased] section")

    version_heading = re.compile(
        rf"^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}$", re.MULTILINE
    )
    if not version_heading.search(changelog):
        fail(f"CHANGELOG.md must contain a release heading for version {version}")

    files = changed_files()
    if files and requires_changelog_update(files) and "CHANGELOG.md" not in files:
        fail("relevant changes require an update to CHANGELOG.md")

    print(f"release-metadata check passed for version {version}")


if __name__ == "__main__":
    main()
