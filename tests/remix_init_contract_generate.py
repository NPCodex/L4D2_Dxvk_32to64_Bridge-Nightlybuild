# SPDX-License-Identifier: MIT
"""Extract production Remix initialization and status dispatch for native tests."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from native_source import function


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("Expected one production control site: " + old)
    return text.replace(old, new)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--control", choices=("none", "before-start", "wrong-uid", "discard-status"), default="none")
    args = parser.parse_args()
    names = ["bridge/src/client/remix_api.cpp", "bridge/src/server/main.cpp",
             "bridge/src/server/remix_api.cpp", "public/include/remixapi/bridge_remix_api.h",
             "public/include/remix/remix_c.h"]
    files = {name: (args.source / name).read_bytes() for name in names}
    client, server, server_api = [files[name].decode("utf-8").replace("\r\n", "\n") for name in names[:3]]
    client_init = function(client, "DLLEXPORT remixapi_ErrorCode __stdcall remixapi_InitializeLibrary(")
    host_init = function(server, "bool InitializeD3D()")
    dispatch = function(server, "      case RemixApi_GetInitializationStatus:")
    state = "  remixapi_ErrorCode g_initStatus = REMIXAPI_ERROR_CODE_NOT_INITIALIZED;"
    if server_api.count(state) != 1:
        raise ValueError("Production initial Remix status changed")
    if "remixapi::g_initStatus = status;" not in host_init:
        raise ValueError("Host does not publish its actual Remix initialization result")
    if args.control == "before-start":
        client_init = replace_once(client_init,
            "if (BridgeState::getServerState() != BridgeState::ProcessState::Running)", "if (false)")
    elif args.control == "wrong-uid":
        dispatch = replace_once(dispatch, "ServerMessage c(Commands::Bridge_Response, currentUID);",
                                "ServerMessage c(Commands::Bridge_Response, currentUID + 1);")
    elif args.control == "discard-status":
        host_init = replace_once(host_init, "remixapi::g_initStatus = status;", "/* mutation: discard status */")
    extracted = "\n".join([
        "namespace remixapi {\n" + state + "\n" +
        function(client, "bool isRemixApiVersionCompatible(") +
        function(client, "constexpr bool strings_equal(") + client_init + "}\n",
        "static void dispatchStatus() { using namespace Commands; switch (Commands::RemixApi_GetInitializationStatus) {\n" + dispatch + "} }\n",
        host_init,
    ])
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "production_remix_init.h").write_text(extracted, encoding="utf-8")
    (args.output / "source-manifest.json").write_text(json.dumps({
        "source": str(args.source), "control": args.control,
        "source_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
        "extracted_sha256": hashlib.sha256(extracted.encode()).hexdigest(),
        "extracted": ["complete client remixapi_InitializeLibrary", "actual version compatibility and strings_equal",
                      "complete Host InitializeD3D", "whole initialization-status dispatch", "actual initial status"],
        "adapters": ["recording IPC boundaries with UID matching", "Windows loader and D3D/Remix backend entry points",
                     "configuration and process state", "interface entry-point sentinels, never called"],
        "excluded": ["real DLL/backend loading", "cross-process transport", "runtime filesystem fallback",
                     "game", "first-launch/cold-start reproduction", "FPS"],
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
