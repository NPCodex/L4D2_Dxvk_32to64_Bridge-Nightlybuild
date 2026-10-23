# SPDX-License-Identifier: MIT
"""Create a ready-to-extract quick-tool update without including any game files."""
import argparse
import hashlib
import io
from pathlib import Path
import zipfile

from quick_tool_payload import quick_tool_files


def package(build, output):
    output = Path(output)
    checksum = Path(str(output) + ".sha256")
    if output.exists() or checksum.exists():
        raise FileExistsError("Keep the existing quick-tool ZIP/checksum")
    files = quick_tool_files(build)
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    data = buffer.getvalue()
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if archive.testzip() is not None or set(archive.namelist()) != set(files):
            raise ValueError("Quick-tool ZIP verification failed")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as target:
        target.write(data)
    with checksum.open("x", encoding="ascii", newline="\n") as target:
        target.write(hashlib.sha256(data).hexdigest() + "  " + output.name + "\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(package(args.build, args.output))
