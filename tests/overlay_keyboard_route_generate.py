"""Extract the production Presenter hook and its routing helpers, unchanged.

The executable models Win32 event delivery; it does not inject keyboard input,
create windows, start Steam/the game, or establish IME composition support.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from native_source import function


def extract(source):
    marker = "class Presenter {"
    if source.count(marker) != 1:
        raise ValueError("Expected one production Presenter")
    begin = source.index(marker) + len(marker)
    end = source.index("  template<class T> static T proc(", begin)
    members = source[begin:end]
    methods = {}
    # Production routing methods are ordinary single-line signatures. Reject a
    # future unrecognized helper instead of silently supplying a test version.
    for match in re.finditer(r"^  (?=\S)[^\n;{}]*\b(\w+)\([^;\n]*\)[^\n;{}]*\{", source, re.M):
        name = match.group(1)
        if name in methods:
            raise ValueError("Ambiguous production method: " + name)
        signature = match.group(0)[:-1].rstrip()
        methods[name] = function(source, signature)
    selected = {}

    def include(name):
        if name in selected or name == "capture":
            return
        if name not in methods:
            raise ValueError("Missing production routing helper: " + name)
        selected[name] = methods[name]
        for callee in methods:
            if callee != name and re.search(r"\b" + re.escape(callee) + r"\s*\(", methods[name]):
                include(callee)
        for callee in re.findall(r"self->(\w+)\s*\(", methods[name]):
            if callee.startswith("m_") and re.search(r"\b" + re.escape(callee) + r"\b", members):
                continue
            if callee not in methods and callee != "capture":
                raise ValueError("Unrecognized hook dependency: " + callee)

    include("keyHook")
    if "nativeFocus" in methods:
        include("nativeFocus")
    # Struct exposes original private members solely for fixture setup. Member
    # declarations and all included function bodies remain byte-for-byte equal
    # after newline normalization. capture is outside the routing contract.
    text = "#define OVERLAY_ROUTE_HAS_NATIVE_FOCUS " + str(int("nativeFocus" in selected)) + "\n"
    text += "struct Presenter {\n" + members
    text += "  void capture(bool active) { ++testCaptureCalls; m_capture.store(active); }\n"
    text += "  unsigned testCaptureCalls = 0;\n"
    text += "\n".join(selected.values()) + "};\n"
    return text, list(selected)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    path = args.source / "bridge/src/server/overlay_presenter.h"
    raw = path.read_bytes()
    source = raw.decode("utf-8").replace("\r\n", "\n")
    generated, methods = extract(source)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "production_overlay_keyboard_route.h").write_text(generated, encoding="utf-8")
    manifest = {
        "source": str(path.resolve()),
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "extracted_sha256": hashlib.sha256(generated.encode()).hexdigest(),
        "production_methods": methods,
        "member_declarations": "exact production declarations; public in fixture struct",
        "adapters": ["Win32 foreground, focus, owner and parent queries", "Win32 keyboard state",
                     "CallNextHookEx and PostMessage routing recorder", "capture transition recorder"],
        "excluded": ["actual Windows hook chain", "real OS input queue", "Steam",
                     "IME composition", "game", "FPS"],
    }
    (args.output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
