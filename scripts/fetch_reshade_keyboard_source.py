#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fetch the fixed official ReShade 6.8.0 input sources used by native tests.

No URL, repository, ref, version or digest override is accepted. Every byte is
verified before any source file is replaced. A fully verified cache is offline.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import tempfile
from urllib.error import HTTPError
from urllib.request import HTTPRedirectHandler, Request, build_opener

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM_COMMIT = "18deaa52de0c425a78b329e9cb3c497281cd00ec"
UPSTREAM_REPOSITORY = "https://github.com/crosire/reshade"
SOURCE_BASE = "https://raw.githubusercontent.com/crosire/reshade/" + UPSTREAM_COMMIT + "/"
PINNED_FILES = {
    "source/input.cpp": "d91f6b8e28843983cd94281c4efc29446f0d87395ccea10088ea0ca441e4b0cb",
    "source/input_windows.cpp": "4cd50ec5522c60856ec9da356eeec7a250cc5c84d9f5bc47b176d9b886a9f377",
    "source/runtime_gui.cpp": "07eb37496e76eae7044f2d75bc2061f2e0d8c6aec63a445d5126d01d105b880f",
}
MAX_SOURCE_BYTES = 512 * 1024


class NoRedirects(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        raise HTTPError(request.full_url, code, "Fixed source URL must not redirect", headers, fp)


def _download(relative):
    if relative not in PINNED_FILES:
        raise ValueError("Unapproved ReShade source path")
    request = Request(SOURCE_BASE + relative, headers={"User-Agent": "L4D2Bridge-native-regression"})
    with build_opener(NoRedirects()).open(request, timeout=30) as response:
        data = response.read(MAX_SOURCE_BYTES + 1)
    if len(data) > MAX_SOURCE_BYTES:
        raise ValueError("ReShade source exceeds the fixed size bound: " + relative)
    return data


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _replace(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix="." + path.name + ".", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def fetch(output):
    output = Path(output).resolve()
    verified = {}
    records = []
    for relative, expected in PINNED_FILES.items():
        path = output / relative
        cached = path.read_bytes() if path.is_file() else None
        cache_hit = cached is not None and _digest(cached) == expected
        data = cached if cache_hit else _download(relative)
        actual = _digest(data)
        if actual != expected:
            raise ValueError("ReShade source SHA256 mismatch: " + relative)
        verified[relative] = data
        records.append({"path": relative, "url": SOURCE_BASE + relative,
                        "sha256": actual, "bytes": len(data), "cache_hit": cache_hit})
    # No partial replacement if any remote response fails validation.
    for relative, data in verified.items():
        path = output / relative
        if not path.is_file() or path.read_bytes() != data:
            _replace(path, data)
    receipt = {"schema": 1, "repository": UPSTREAM_REPOSITORY,
               "commit": UPSTREAM_COMMIT, "version": "6.8.0",
               "checked_utc": datetime.now(timezone.utc).isoformat(),
               "files": records, "all_bytes_verified": True,
               "scope": "Official source for tests only; no ReShade binary included in player package"}
    _replace(output / "source-receipt.json", (json.dumps(receipt, indent=2) + "\n").encode())
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".deps/reshade-keyboard-source")
    args = parser.parse_args()
    result = fetch(args.output)
    print("RESHade_SOURCE_VERIFIED commit=" + result["commit"] + " files=" + str(len(result["files"])))


if __name__ == "__main__":
    main()
