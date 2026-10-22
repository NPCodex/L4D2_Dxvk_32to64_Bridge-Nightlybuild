# SPDX-License-Identifier: MIT
"""Extract actual material producer/consumer and serializers for native ABI tests."""
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
        raise ValueError("Expected one production site: " + old)
    return text.replace(old, new)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--role", choices=("client", "server"), required=True)
    parser.add_argument("--control", choices=("none", "prefix-overread", "transform-order"), default="none")
    args = parser.parse_args()
    names = ["bridge/src/client/remix_api.cpp", "bridge/src/server/main.cpp",
             "bridge/src/server/remix_api.h", "bridge/src/util/util_remixapi.cpp",
             "bridge/src/util/util_remixapi.h", "bridge/src/util/util_serializable.h",
             "public/include/remixapi/bridge_remix_api.h", "public/include/remix/remix_c.h"]
    raw = {name: (args.source / name).read_bytes() for name in names}
    src = {name: value.decode("utf-8").replace("\r\n", "\n") for name, value in raw.items()}
    client, server, server_h, serializer, header = [src[name] for name in names[:5]]
    # Select the actual role explicitly so both Windows data models exercise both
    # production paths; util_common normally selects Client/Host by architecture.
    header = replace_once(header, '#include "util_common.h"',
                          '#include <cstdint>\n#include <type_traits>\n#define REMIX_BRIDGE_' + args.role.upper())
    # Complete support routines and material implementations, no unrelated mesh/light code.
    marker = "//////////////\n// MeshInfo //"
    if serializer.count(marker) != 1:
        raise ValueError("Material serializer section boundary changed")
    serializer = serializer[:serializer.index(marker)] + "\n} // namespace bridge_util\n"
    if args.control == "transform-order":
        old = function(serializer, "void MaterialInfoOpaque::_serialize(void*& pSerialize) const")
        new = old.replace("fold_helper::serialize(pSerialize, MaterialInfoOpaqueVars);",
            "const float textureRotation = this->textureScrollRate;\n  fold_helper::serialize(pSerialize, MaterialInfoOpaqueVars);")
        serializer = replace_once(serializer, old, new)
    chunks = []
    if args.role == "client":
        start = client.index("template<typename T>\ninline void send(")
        stop = client.index("remixapi_ErrorCode REMIXAPI_CALL remixapi_CreateMaterial(")
        helpers = client[start:stop]
        # Only the temporary allocation is adapted: the actual size calculation,
        # serialize call and queue call execute unchanged against a protected end.
        helpers = replace_once(helpers, "auto pSlzd = new uint8_t[serializableSize];",
                               "GuardedEnd storage(serializableSize);\n  auto pSlzd = storage.data;")
        helpers = replace_once(helpers, "delete pSlzd;", "/* GuardedEnd owns the buffer. */")
        create = function(client, "remixapi_ErrorCode REMIXAPI_CALL remixapi_CreateMaterial(")
        if args.control == "prefix-overread":
            create = replace_once(create,
                "offsetof(remixapi_MaterialInfoOpaqueEXT, enableDlssControlMask)",
                "sizeof(remixapi_MaterialInfoOpaqueEXT)")
        chunks = ["namespace remixapi { uint64_t g_appRemixApiVersion = 0;\n", helpers, create, "}\n"]
    else:
        chunks = ["namespace remixapi {\n",
            function(server_h, "static inline remixapi_StructType pullSType()"),
            function(server_h, "static inline bool pullBool()"), "}\n",
            "template<typename SerializableT>\n" + function(server, "static void deserializeFromQueue(SerializableT& serializableT)"),
            "static void dispatchMaterial() { using namespace Commands; switch (RemixApi_CreateMaterial) {\n",
            function(server, "      case RemixApi_CreateMaterial:"), "} }\n"]
    generated = {"util_remixapi.h": header, "production_material_serializers.h": serializer,
                 "production_material_path.h": "".join(chunks)}
    args.output.mkdir(parents=True, exist_ok=True)
    for name, text in generated.items():
        (args.output / name).write_bytes(text.encode("utf-8"))
    manifest = {
        "schema": 1, "source": str(args.source), "role": args.role, "control": args.control,
        "source_sha256": {name: hashlib.sha256(value).hexdigest() for name, value in raw.items()},
        "generated_sha256": {name: hashlib.sha256(value.encode()).hexdigest() for name, value in generated.items()},
        "extracted": ["complete Client CreateMaterial and send/serializeAndSend helpers" if args.role == "client" else
                      "complete Host CreateMaterial dispatch, deserializeFromQueue, pullSType and pullBool",
                      "actual material serializer definitions and support routines", "actual public C ABI structures"],
        "adapters": ["explicit Client/Host role for both Windows architectures", "record/replay IPC boundary",
                     "PAGE_NOACCESS-ended temporary serialization allocation", "Host backend validation callback"],
        "excluded": ["real shared-memory IPC and process handshake", "DLL/backend loading", "renderer", "game", "performance"]}
    (args.output / "source-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
