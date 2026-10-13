# SPDX-License-Identifier: MIT
"""Compile real Host helpers and the real Query GetData dispatch body."""
from pathlib import Path
import argparse
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from native_source import function

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--control", choices=["none", "pitch", "lock", "query", "row", "row-allocation", "empty-buffer"], default="none")
args = parser.parse_args()
server = args.source / "bridge/src/server"
args.output.mkdir(parents=True, exist_ok=True)
upload = (server / "host_upload_contract.h").read_text(encoding="utf-8")
main = (server / "main.cpp").read_text(encoding="utf-8")
start = main.index("      case IDirect3DQuery9_GetData:\n")
end = main.index("      /*\n       * Other commands", start)
query = main[start:end]
query = query.replace("case IDirect3DQuery9_GetData:", "case 1:", 1)
if args.control == "pitch":
    assert upload.count("pitch < rowBytes") == 1
    upload = upload.replace("pitch < rowBytes", "false")
elif args.control == "lock":
    assert upload.count("if (FAILED(result.lock)) { return result; }") == 3
    upload = upload.replace("if (FAILED(result.lock)) { return result; }", "/* negative control: ignore lock failure */")
elif args.control == "query":
    assert query.count("hresult == S_OK && dwSize > 0") == 1
    query = query.replace("hresult == S_OK && dwSize > 0", "SUCCEEDED(hresult) && dwSize > 0")
elif args.control == "empty-buffer":
    # Selectively restore the shipped pre-fix helper; all callers stay identical.
    guard = "if (bytes && (!source || reinterpret_cast<uintptr_t>(source) > UINTPTR_MAX - bytes))"
    empty = "  // Empty Client uploads still preserve backend Lock/Unlock without touching pointers.\n  if (!bytes) { result.copied = true; }\n  else if (destination"
    assert upload.count(guard) == 1 and upload.count(empty) == 1
    upload = upload.replace(guard, "if (!source || !bytes || reinterpret_cast<uintptr_t>(source) > UINTPTR_MAX - bytes)")
    upload = upload.replace(empty, "  if (destination")
(args.output / "host_upload_contract.h").write_text(upload, encoding="utf-8")
(args.output / "host_query_extracted.h").write_text(
    "static void extractedHostQuery() { switch (1) {\n" + query + "} }\n", encoding="utf-8")
volume = function(main, "static bool receiveVolumeUpload(")
if args.control == "row-allocation":
    assert volume.count("staging.resize(checked.bytes);") == 1
    volume = volume.replace("staging.resize(checked.bytes);", "throw std::bad_alloc{};")
(args.output / "host_volume_extracted.h").write_text(
    "template<typename Lock, typename Unlock>\n" + volume, encoding="utf-8")
surface_start = main.index("        // Consume the complete payload before a backend call can fail.")
surface_end = main.index("      case IDirect3DSurface9_GetDC:", surface_start)
surface = main[surface_start:surface_end]
(args.output / "host_surface_extracted.h").write_text(
    "static void extractedHostSurface(Surface* pSurface, RECT* pRect, DWORD dFormat, DWORD IncomingPitch) {\n"
    "const uint32_t pHandle = rpcHeader.pHandle; const DWORD Flags = 0; switch (1) { case 1: {\n"
    + surface + "} }\n", encoding="utf-8")
formats = (args.source / "bridge/src/util/util_texture_and_volume.h").read_text(encoding="utf-8")
defines = "\n".join(re.findall(r"^#define D3DFMT_.*$", formats, re.M))
format_source = defines + "\nnamespace bridge_util {\n" + function(formats, "static uint32_t getBlockSize(") + function(formats, "static uint32_t getBytesFromFormat(") + "}\n"
(args.output / "host_formats_extracted.h").write_text(format_source, encoding="utf-8")
buffer_start = main.index("        const bool reserved = Commands::IsDataReserved(rpcHeader.flags);", main.index("case IDirect3DVertexBuffer9_Unlock:"))
buffer_end = main.index("      case IDirect3DVertexBuffer9_GetDesc:", buffer_start)
(args.output / "host_buffer_extracted.h").write_text(
    "static void extractedHostBuffer(Buffer* buffer, uint32_t OffsetToLock, uint32_t SizeToLock, uint32_t reference, void* data) {\n"
    "const uint32_t pHandle = rpcHeader.pHandle; const DWORD Flags = 0; switch (1) { case 1: {\n"
    + main[buffer_start:buffer_end] + "} }\n", encoding="utf-8")
with (args.output / "host_buffer_extracted.h").open("a", encoding="utf-8") as output:
    # Complete production dispatch bodies, including field decoding and both routes.
    for kind in ("Vertex", "Index"):
        marker = f"      case IDirect3D{kind}Buffer9_Unlock:"
        start = main.index(marker)
        end = main.index(f"      case IDirect3D{kind}Buffer9_GetDesc:", start)
        dispatch = main[start:end].replace(marker, "      case 1:", 1)
        output.write(f"static void extractedHost{kind}Packet() {{ switch (1) {{\n" + dispatch + "} }\n")

# Exercise the entire unmodified Client template with the established COM/IPC adapters.
client = (args.source / "bridge/src/client/lockable_buffer.h").read_text(encoding="utf-8")
marker = "template <typename T>"
assert client.count(marker) == 1
(args.output / "host_empty_client.cpp").write_text(
    '#include "buffer_contract_support.h"\n' + client[client.index(marker):]
    + '\n#include "host_empty_client_cases.cpp"\n', encoding="utf-8")
