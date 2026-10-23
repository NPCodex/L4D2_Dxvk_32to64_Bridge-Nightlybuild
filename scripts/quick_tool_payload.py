# SPDX-License-Identifier: MIT
"""Verify and select exactly the built quick tool and its two player entry files."""
import hashlib
import json
from pathlib import Path
import re
import struct

from engine_payload import read_regular

ROOT = Path(__file__).resolve().parents[1]
SOURCE_NAMES = ("Core.cs", "HoverHelp.cs", "LogMonitor.cs", "SteamSupport.cs",
                "Installation.cs", "StartupCheck.cs", "LaunchPane.cs",
                "MainForm.cs", "AssemblyInfo.cs")
BUILD_INPUTS = tuple("tools/quick-tool/" + name for name in SOURCE_NAMES) + (
    "scripts/build_quick_tool.ps1",)
EXE = "tools/BridgeQuickTool/BridgeQuickTool.exe"
GUIDE = "tools/BridgeQuickTool/README.txt"
LAUNCHER = "L4D2-Quick-Tool.cmd"
PLAYER_PATHS = (EXE, GUIDE, LAUNCHER)


def validate_executable(data):
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("Quick tool is not a PE executable")
    pe = struct.unpack_from("<I", data, 60)[0]
    if pe > len(data) - 24 or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid quick-tool PE header")
    optional = pe + 24
    size = struct.unpack_from("<H", data, pe + 20)[0]
    if (struct.unpack_from("<H", data, pe + 4)[0] != 0x14C or size < 224
            or optional + size > len(data)
            or struct.unpack_from("<H", data, optional)[0] != 0x10B
            or struct.unpack_from("<H", data, optional + 68)[0] != 2
            or struct.unpack_from("<I", data, optional + 92)[0] < 15):
        raise ValueError("Quick tool must be an AnyCPU .NET Windows GUI executable")
    cli_rva, cli_size = struct.unpack_from("<II", data, optional + 96 + 14 * 8)
    if not cli_rva or cli_size < 72:
        raise ValueError("Quick tool is missing its managed CLR directory")


def quick_tool_files(build, root=ROOT):
    if build is None:
        raise ValueError("A validated quick-tool build is required for a player ZIP")
    build, root = Path(build), Path(root)
    receipt_data = read_regular(build / "quick-tool-build.json")
    if len(receipt_data) > 64 * 1024:
        raise ValueError("Quick-tool build receipt exceeds limit")
    receipt = json.loads(receipt_data)
    if receipt.get("schema") != 1 or receipt.get("product") != "BridgeQuickTool":
        raise ValueError("Invalid quick-tool build receipt")
    expected = {name: hashlib.sha256(read_regular(root / name)).hexdigest()
                for name in BUILD_INPUTS}
    if receipt.get("sources") != expected:
        raise ValueError("Quick-tool build is stale or has different source inputs")
    assembly = read_regular(root / "tools/quick-tool/AssemblyInfo.cs").decode("utf-8-sig")
    version = re.search(r'AssemblyFileVersion\("(\d+\.\d+\.\d+)\.\d+"\)', assembly)
    if version is None or receipt.get("version") != version[1]:
        raise ValueError("Quick-tool version differs from source")
    executable = read_regular(build / "BridgeQuickTool.exe")
    if hashlib.sha256(executable).hexdigest() != receipt.get("exe_sha256"):
        raise ValueError("Quick-tool executable differs from its build receipt")
    validate_executable(executable)
    # Read each approved input exactly once. Never include build/test outputs,
    # state, reports, credentials or arbitrary staging files.
    guide = read_regular(root / "tools/quick-tool/USER-README.txt")
    launcher = read_regular(root / "tools/quick-tool/Launch.cmd")
    if not guide.strip() or not launcher.strip():
        raise ValueError("Quick-tool player instructions/launcher are missing")
    return {EXE: executable, GUIDE: guide, LAUNCHER: launcher}
