# SPDX-License-Identifier: MIT
"""Record exact prepared source after native validation; reject source drift.

The receipt is a CI artifact, never a player package or a gameplay certificate.
The source comparison ignores build products through the upstream ignore rules,
but validates every tracked/patched input with a fresh index.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from build_identity import protocol_digest, recipe_digest
from prepare_bridge import ROOT, _check_index, _matches_tree, _render_patch


def record(source, output):
    base = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    rendered = _render_patch()
    with tempfile.TemporaryDirectory(prefix="bridge-validation-") as directory:
        patch = Path(directory) / "rendered.patch"
        patch.write_bytes(rendered)
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(directory) / "index"))
        for arguments in (("read-tree", base), ("apply", "--cached", str(patch))):
            subprocess.run(["git", *arguments], cwd=source, env=env, check=True)
        expected = subprocess.check_output(["git", "write-tree"], cwd=source, env=env, text=True).strip()
        _check_index(source, base, expected)
        if not _matches_tree(source, expected, env):
            raise RuntimeError("Prepared source drifted from the rendered patch; no validation receipt written")
        names = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", "-z", expected, "--", "bridge"], cwd=source)
        files = {name.decode("utf-8"): hashlib.sha256((source / name.decode("utf-8")).read_bytes()).hexdigest()
                 for name in names.split(b"\0") if name}
    raw = (ROOT / "patches/l4d2-bridge.patch").read_bytes()
    version = (ROOT / "VERSION").read_text().strip()
    receipt = {
        "schema": 1, "scope": "Exact prepared source after CI native suites; no gameplay claim",
        "recipe_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "recipe_digest": recipe_digest(ROOT), "version": version, "nvidia_commit": base,
        "patch_sha256": hashlib.sha256(raw).hexdigest(),
        "rendered_patch_sha256": hashlib.sha256(rendered).hexdigest(),
        "protocol_source_sha256": protocol_digest(raw, version),
        "expected_git_tree": expected, "source_matches_rendered_patch": True,
        "bridge_source_files": files,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Verified {len(files)} Bridge source files; receipt: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=ROOT / ".deps/dxvk-remix")
    parser.add_argument("--output", type=Path, default=ROOT / ".deps/build-validation.json")
    args = parser.parse_args()
    record(args.source.resolve(), args.output.resolve())
