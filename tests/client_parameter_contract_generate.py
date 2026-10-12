"""Compile actual Client parameter/state methods with a transport adapter.

The negative controls restore only the original range/pending-result decisions;
the same boundary tests must reject each control in Release on x86 and x64.
"""
import argparse
import re
from pathlib import Path


def block(text, marker):
    start = text.index(marker)
    begin = text.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    if text[end:].startswith(" catch ("):
        end += len(block(text[end:], " catch ("))
    return text[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--control", choices=("none", "range", "query", "pixel-size"), default="none")
    args = parser.parse_args()
    source = args.source / "bridge/src"
    device = (source / "client/d3d9_device.cpp").read_text(encoding="utf-8")
    header = (source / "client/d3d9_device_base.h").read_text(encoding="utf-8")
    query = (source / "client/d3d9_query.cpp").read_text(encoding="utf-8")
    common = (source / "util/util_common.h").read_text(encoding="utf-8")
    args.output.mkdir(parents=True, exist_ok=True)
    caps = block(common, "namespace caps {")
    constants = block(header, "  struct ShaderConstants {") + ";\n"
    (args.output / "constant_layout.h").write_text(constants, encoding="utf-8")
    (args.output / "constant_caps.h").write_text(caps, encoding="utf-8")
    methods = []
    for operation in ("set", "get", "commonGetSet"):
        token = ("std::tuple<HRESULT, size_t>" if operation == "commonGetSet" else "HRESULT")
        method = block(device, f"{token} BaseDirect3DDevice9Ex_LSS::{operation}ShaderConstants(" if operation != "commonGetSet"
                       else f"{token} BaseDirect3DDevice9Ex_LSS::commonGetSetConstants(")
        methods.append("template<ShaderType ShaderT, ConstantType ConstantT, typename T>\n" + method)
    if args.control == "range":
        old = "if (startRegister > regCountSoftware || count > regCountSoftware - startRegister)"
        assert sum(x.count(old) for x in methods) == 1
        methods = [x.replace(old, "if (startRegister + count > regCountSoftware)") for x in methods]
    macro_begin = device.index("#define SetShaderConst(")
    macro_end = device.index("\n\n", macro_begin)
    methods.append(device[macro_begin:macro_end])
    declarations = []
    for shader in ("Vertex", "Pixel"):
        for kind in ("F", "I", "B"):
            for operation in ("Set", "Get"):
                name = f"{operation}{shader}ShaderConstant{kind}"
                method = block(device, f"HRESULT Direct3DDevice9Ex_LSS<EnableSync>::{name}(")
                signature = method[:method.index("{")].removesuffix("try ").strip()
                declarations.append(signature.replace("Direct3DDevice9Ex_LSS<EnableSync>::", "") + ";")
                methods.append("template<bool EnableSync>\n" + method)
    (args.output / "device_declarations.h").write_text("\n".join(declarations), encoding="utf-8")
    size_method = block(query, "DWORD Direct3DQuery9_LSS::GetDataSize(")
    if args.control == "pixel-size":
        old = "case D3DQUERYTYPE_PIXELTIMINGS:         return sizeof(D3DDEVINFO_D3D9STAGETIMINGS);"
        assert size_method.count(old) == 1
        size_method = size_method.replace(old, "case D3DQUERYTYPE_PIXELTIMINGS:         return sizeof(D3DDEVINFO_D3D9PIPELINETIMINGS);")
    methods.append(size_method)
    method = block(query, "HRESULT Direct3DQuery9_LSS::GetData(")
    if args.control == "query":
        assert method.count("if (hresult == S_OK && dwSize > 0)") == 1
        method = method.replace("if (hresult == S_OK && dwSize > 0)", "if (SUCCEEDED(hresult) && dwSize > 0)")
    methods.append(method)
    methods.append("template<bool EnableSync>\n" + block(device, "HRESULT Direct3DDevice9Ex_LSS<EnableSync>::UpdateTexture("))
    notices = device[:device.index('#include "pch.h"')]
    (args.output / "client_methods.h").write_text(notices + "\n\n".join(methods), encoding="utf-8")


if __name__ == "__main__":
    main()
