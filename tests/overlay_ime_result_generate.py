"""Extract the real Presenter default-message boundary without creating a HWND."""
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
    for match in re.finditer(r"^  (?=\S)[^\n;{}]*\b(\w+)\([^;\n]*\)[^\n;{}]*\{", source, re.M):
        name = match.group(1)
        if name in methods:
            raise ValueError("Ambiguous production method: " + name)
        methods[name] = function(source, match.group(0)[:-1].rstrip())
    selected = {}

    def include(name):
        if name in selected:
            return
        if name not in methods:
            raise ValueError("Missing production method: " + name)
        selected[name] = methods[name]
        for callee in methods:
            if callee != name and re.search(r"\b" + re.escape(callee) + r"\s*\(", methods[name]):
                include(callee)
        for callee in re.findall(r"self->(\w+)\s*\(", methods[name]):
            if callee.startswith("m_") and re.search(r"\b" + re.escape(callee) + r"\b", members):
                continue
            if callee not in methods:
                raise ValueError("Unrecognized windowProc dependency: " + callee)

    include("windowProc")
    generated = "struct Presenter {\n" + members
    generated += '  explicit Presenter(bool diagnostics = false) : m_diagnostics(diagnostics, "fixture", [](const char*) {}) {}\n'
    generated += "\n".join(selected.values()) + "};\n"
    return generated, list(selected)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--dependencies", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source
    dependencies_dir = args.dependencies
    if source.is_dir():
        if dependencies_dir is None:
            dependencies_dir = source / "bridge/src/util"
        source = source / "bridge/src/server/overlay_presenter.h"
    elif dependencies_dir is None:
        parser.error("--dependencies is required when --source is a standalone header")
    raw = source.read_bytes()
    text = raw.decode("utf-8").replace("\r\n", "\n")
    generated, methods = extract(text)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "production_ime_result.h").write_text(generated, encoding="utf-8")
    dependencies = {}
    for name in ("overlay_input_diagnostics.h", "runtime_observation.h"):
        path = dependencies_dir / name
        payload = path.read_bytes()
        (args.output / name).write_bytes(payload)
        dependencies[name] = {"source": str(path.resolve()), "sha256": hashlib.sha256(payload).hexdigest()}
    manifest = {
        "source": str(source.resolve()),
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "extracted_sha256": hashlib.sha256(generated.encode()).hexdigest(),
        "production_methods": methods,
        "member_declarations": "exact declarations, public in fixture struct",
        "fixture_constructor": "initializes the real InputDiagnostics; state otherwise set directly",
        "production_dependencies": dependencies,
        "adapters": ["GetWindowLongPtrW maps the fixture HWND to its Presenter", "DefWindowProcW records exact parameters and returns a sentinel"],
        "real_apis": ["GetLastError", "SetLastError", "GetTickCount64"],
        "excluded": ["HWND creation", "Windows message dispatch", "Steam hook chain", "real IME composition or text delivery", "input injection", "game", "FPS"],
    }
    (args.output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
