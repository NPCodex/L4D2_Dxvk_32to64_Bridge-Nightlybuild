# SPDX-License-Identifier: MIT
"""Create exact, result-local old-condition controls; never modify prepared source."""
import argparse
import hashlib
import json
from pathlib import Path


def generate(source, output, mode):
    output.mkdir(parents=True, exist_ok=True)
    if mode == "observer-unpinned":
        relative = "bridge/src/server/steam_window_diagnostics.h"
        before = b'''    HMODULE self=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&active),&self)){
      FreeLibrary(loaded);emit("L4D2_STEAM_WINDOW event=unsupported reason=observer-code-not-pinned");return;
    }
'''
        after = b""
    else:
        relative = "bridge/src/util/steam_workshop_status.h"
        before = b"return rva==0x199ea || rva==0x19aca || rva==0x6ea9a || rva==0x11a03;"
        after = b"return rva==0x199ea || rva==0x19aca || rva==0x6ea9a;"
        # Quoted includes resolve beside steam_web_diagnostics.h first, so copy
        # that header unchanged to ensure the modified guard is actually used.
        web = source / "bridge/src/util/steam_web_diagnostics.h"
        (output / web.name).write_bytes(web.read_bytes())
    path = source / relative
    original = path.read_bytes()
    normalized = original.replace(b"\r\n", b"\n")
    if normalized.count(before) != 1:
        raise ValueError(f"Unexpected {mode} production layout; refusing broad substitution")
    changed = normalized.replace(before, after, 1)
    if b"\r\n" in original:
        changed = changed.replace(b"\n", b"\r\n")
    (output / path.name).write_bytes(changed)
    receipt = {"mode": mode, "source": str(path.resolve()), "source_sha256": hashlib.sha256(original).hexdigest(),
               "control_sha256": hashlib.sha256(changed).hexdigest(), "replacement_count": 1,
               "removed": before.decode(), "replacement": after.decode(), "production_modified": False}
    (output / "control-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {mode}: one exact old-condition replacement")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("observer-unpinned", "invite-old-guard"), required=True)
    args = parser.parse_args()
    generate(args.source, args.output, args.mode)
