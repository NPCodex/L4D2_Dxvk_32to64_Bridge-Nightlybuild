"""Extract unchanged production device lifetime code for an isolated native test."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from native_source import function


def macro(source, name):
    marker = "#define " + name + "("
    if source.count(marker) != 1:
        raise ValueError("Expected one macro: " + name)
    begin = source.index(marker)
    end = begin
    while True:
        end = source.index("\n", end) + 1
        if not source[begin:end].rstrip("\n").endswith("\\"):
            return source[begin:end]


def dispatch(source, command, wrapper):
    marker = "      case " + command + ":"
    if source.count(marker) != 1:
        raise ValueError("Expected one dispatch: " + command)
    begin = source.index(marker)
    end = source.index("      case ", begin + len(marker))
    return "static void " + wrapper + "() { switch (rpcHeader.command) {\n" + source[begin:end] + "} }\n"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--control", choices=("none", "strict-destroy"), default="none")
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    names = ["bridge/src/server/main.cpp", "bridge/src/server/host_upload_contract.h"]
    files = {}
    for name in names:
        files[name] = (a.source / name).read_bytes()
    source = files[names[0]].decode("utf-8").replace("\r\n", "\n")
    helpers = files[names[1]].decode("utf-8").replace("\r\n", "\n")
    destroy = dispatch(source, "IDirect3DDevice9Ex_Destroy", "dispatchDestroy")
    if a.control == "strict-destroy":
        # Restore only the former strict cleanup guard; all live-command guards
        # and the actual release/lookup implementations stay unchanged.
        fixed = ("        // Cleanup is idempotent, including a wrapper whose backend creation failed.\n"
                 "        GET_HND(pD3DDeviceHandle);\n"
                 "        const auto pD3DDevice = lookupHostHandle(gpD3DDevices, pD3DDeviceHandle);")
        assert destroy.count(fixed) == 1, "Expected exactly one idempotent Device Destroy block"
        assert "GET_RES(pD3DDevice, gpD3DDevices)" not in destroy, "Destroy is already strict"
        destroy = destroy.replace(fixed, "        GET_RES(pD3DDevice, gpD3DDevices);")
    parts = [
        "namespace l4d2_host_upload {\ntemplate<typename Map>\n" +
        function(helpers, "typename Map::mapped_type find(") + "}\n",
        "template<typename Map>\n" + function(source, "static typename Map::mapped_type lookupHostHandle("),
        macro(source, "GET_HND"), macro(source, "GET_RES"),
        function(source, "static inline void safeDestroy("),
        destroy,
        dispatch(source, "IDirect3DDevice9Ex_TestCooperativeLevel", "dispatchLiveCall"),
    ]
    extracted = "\n".join(parts)
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / "production_device_destroy.h").write_text(extracted, encoding="utf-8")
    (a.output / "source-manifest.json").write_text(json.dumps({
        "source": str(a.source),
        "control": a.control,
        "source_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
        "extracted_sha256": hashlib.sha256(extracted.encode()).hexdigest(),
        "extracted": ["actual find", "actual lookupHostHandle", "actual GET_HND and GET_RES",
                      "actual safeDestroy", "whole Destroy dispatch", "whole TestCooperativeLevel dispatch"],
        "adapters": ["IUnknown backend records Release and cooperative calls",
                     "DeviceBridge records faults", "ServerMessage records response", "map and command header"],
        "excluded": ["real D3D backend", "transport", "game", "FPS", "debug-only verbose release path"],
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
