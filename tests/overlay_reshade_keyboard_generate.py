"""Extract production Presenter and pinned ReShade input boundaries for a no-UI fixture.

The Win32 queue adapter is a model; the extracted policy bodies are not rewritten.
ReShade code remains in the caller-provided, hash-bound upstream checkout.
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
from fetch_reshade_keyboard_source import PINNED_FILES as PINNED, UPSTREAM_COMMIT


def presenter_extract(source):
    begin = source.index("class Presenter {") + len("class Presenter {")
    end = source.index("  template<class T> static T proc(", begin)
    members = source[begin:end]
    methods = {}
    for match in re.finditer(r"^  (?=\S)[^\n;{}]*\b(\w+)\([^;\n]*\)[^\n;{}]*\{", source, re.M):
        name = match.group(1)
        if name in methods:
            raise ValueError("Ambiguous Presenter method: " + name)
        methods[name] = function(source, match.group(0)[:-1].rstrip())
    selected = {}

    def include(name):
        if name in selected:
            return
        selected[name] = methods[name]
        for callee in methods:
            if callee != name and re.search(r"\b" + re.escape(callee) + r"\s*\(", methods[name]):
                include(callee)

    for root in ("keyHook", "applyMode", "onOverlay", "capture", "steamCapture"):
        include(root)
    return "struct Presenter {\n" + members + "\n".join(selected.values()) + "};\n", list(selected)


def upstream_extract(files):
    inp, win, gui = (files[p] for p in PINNED)
    snippets = []
    for sig in (
        "bool reshade::input::is_key_down(unsigned int keycode) const",
        "bool reshade::input::is_key_pressed(unsigned int keycode) const",
        "bool reshade::input::is_key_pressed(unsigned int keycode, bool ctrl, bool shift, bool alt, bool force_modifiers) const",
        "bool reshade::input::is_key_repeated(unsigned int keycode) const",
    ):
        snippets.append(function(inp, sig))
    next_frame = function(win, "void reshade::input::next_frame()")
    snippets.append(next_frame)
    keyboard = win[win.index("\tcase WM_KEYDOWN:"):win.index("\tcase WM_LBUTTONDOWN:")]
    snippets.append("void reshade::input::consume(const MSG &details) {\n"
                    "bool is_keyboard_message = true; auto *input = this;\n"
                    "switch (details.message) {\n" + keyboard +
                    "default: break; } (void)is_keyboard_message;\n}\n")
    wrapper = function(win, "static LRESULT CALLBACK handle_windows_hook(int nCode, WPARAM wParam, LPARAM lParam)")
    snippets.append("template <DWORD hook_id, bool(*is_blocking_input)(reshade::input::window_handle)>\n" + wrapper)
    draw = function(gui, "void reshade::runtime::draw_gui()")
    start = draw.index("\tif (_input != nullptr)")
    end = draw.index("\n\tif (_input_gamepad", start)
    decision = draw[start:end]
    return "\n".join(snippets), decision


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", type=Path, required=True)
    ap.add_argument("--reshade-source", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    path = args.source / "bridge/src/server/overlay_presenter.h"
    raw = path.read_bytes()
    presenter, methods = presenter_extract(raw.decode("utf-8").replace("\r\n", "\n"))
    upstream = {}
    for relative, expected in PINNED.items():
        data = (args.reshade_source / relative).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError("Pinned ReShade 6.8.0 source differs: " + relative)
        upstream[relative] = data.decode("utf-8").replace("\r\n", "\n")
    reshade, decision = upstream_extract(upstream)
    args.output.mkdir(parents=True, exist_ok=True)
    outputs = {"production_overlay_reshade_keyboard.h": presenter,
               "production_reshade_input.h": reshade,
               "production_reshade_gui_keyboard.h": decision}
    for name, text in outputs.items():
        (args.output / name).write_bytes(text.encode("utf-8"))
    manifest = {
        "source": str(path.resolve()), "source_sha256": hashlib.sha256(raw).hexdigest(),
        "presenter_methods": methods,
        "reshade_commit": UPSTREAM_COMMIT,
        "reshade_source_sha256": PINNED,
        "generated_sha256": {n: hashlib.sha256(t.encode()).hexdigest() for n, t in outputs.items()},
        "extraction": ["complete Presenter routing/capture/applyMode and recursive helpers",
                       "complete ReShade handle_windows_hook and key-state/next_frame methods",
                       "unchanged ReShade legacy key switch arms and draw_gui keyboard decision block"],
        "adapters": ["Win32 focus/foreground and native/post queue delivery model",
                     "ReShade registration lookup map and accepted addon callback",
                     "Steam-style queue modifier observation (not Steam implementation)"],
        "excluded": ["actual Windows hooks/input injection/UI", "real Steam", "ReShade rendering", "game", "FPS"],
    }
    (args.output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
