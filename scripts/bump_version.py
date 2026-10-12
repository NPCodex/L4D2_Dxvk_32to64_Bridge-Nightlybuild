"""Advance the staged project version once per commit, including amend."""
import re
import subprocess
from pathlib import Path


def next_version(value, significant=False):
    if not re.fullmatch(r"[0-9]+\.[0-9]+(?:\.[0-9]+)?", value):
        raise ValueError("VERSION must use major.minor or major.minor.patch")
    parts = list(map(int, value.split(".")))
    major, minor = parts[:2]
    if significant:
        return f"{major}.{minor + 1}"
    patch = parts[2] if len(parts) == 3 else 0
    return f"{major}.{minor}.{patch + 1}"


def version_for_commit(previous, staged):
    patch_version = next_version(previous)
    minor_version = next_version(previous, significant=True)
    # major.minor and major.minor.0 denote the same explicit minor release.
    minor_full_version = minor_version + ".0"
    if staged not in (previous, patch_version, minor_version, minor_full_version):
        raise ValueError(f"Expected VERSION {previous}, {patch_version}, {minor_version} or {minor_full_version}, got {staged}")
    return staged if staged != previous else patch_version


def main():
    root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
    staged = subprocess.check_output(["git", "show", ":VERSION"], text=True).strip()
    current = (Path(root) / "VERSION").read_text().strip()
    if current != staged:
        raise RuntimeError("VERSION has unstaged changes; stage or restore it before committing")
    previous = subprocess.check_output(["git", "show", "HEAD:VERSION"], text=True).strip()
    # An explicitly staged next version is accepted without incrementing twice.
    # Small changes are automatic; stage the next minor version for a significant update.
    desired = version_for_commit(previous, staged)
    (Path(root) / "VERSION").write_text(desired + "\n", encoding="ascii", newline="\n")
    subprocess.run(["git", "add", "--", "VERSION"], check=True)
    print(f"Project version: v{desired}")


if __name__ == "__main__":
    main()
