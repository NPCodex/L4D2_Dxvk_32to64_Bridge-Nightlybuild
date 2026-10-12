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
parser.add_argument("--control", choices=["none", "pitch", "lock", "query", "row", "row-allocation"], default="none")
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
