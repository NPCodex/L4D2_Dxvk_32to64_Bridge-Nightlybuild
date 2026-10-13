# SPDX-License-Identifier: MIT
"""Verify and reproduce the pinned GPLALL shader-only backend modification."""
import argparse
import copy
import difflib
import hashlib
import json
from pathlib import Path
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCH_PATHS = {"x86": "x32/d3d9.dll", "x64": "x64/d3d9.dll"}
SOURCE_MARKER = "  [[unroll]]\n  for (uint s = 0u; s < c_src_samples; s++)"
MODIFIED_MARKER = "  [[dont_unroll]]\n  for (uint s = 0u; s < c_src_samples; s++)"
MODIFIED_NOTICE = "// Modified by NPCodex on 2026-10-13: only the linear/nearest sample loop\n// changes from unroll to dont_unroll. Original GPLALL/DXVK attribution and license apply.\n"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked_asset(root, entry):
    path = (root / entry["path"]).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError("Backend recipe asset escapes repository")
    raw = path.read_bytes()
    if sha(raw) != entry["sha256"]:
        raise ValueError(f"Backend recipe asset checksum mismatch: {entry['path']}")
    return raw


def validated_shaders(root, metadata):
    mod = metadata["modification"]
    if mod["kind"] != "spirv-loop-control" or mod["id"] != "msaa-linear-dont-unroll-20261013":
        raise ValueError("Unknown backend modification recipe")
    original_source = checked_asset(root, mod["original_source"])
    modified_source = checked_asset(root, mod["modified_source"])
    text = original_source.decode("utf-8")
    if text.count(SOURCE_MARKER) != 1 or modified_source.decode("utf-8") != MODIFIED_NOTICE + text.replace(SOURCE_MARKER, MODIFIED_MARKER):
        raise ValueError("Backend source changes exceed the reviewed loop hint")
    source_patch = checked_asset(root, mod["source_patch"])
    expected_patch = "".join(difflib.unified_diff(
        text.splitlines(keepends=True), modified_source.decode("utf-8").splitlines(keepends=True),
        fromfile="a/" + mod["upstream_path"], tofile="b/" + mod["upstream_path"])).encode("utf-8")
    if source_patch != expected_patch:
        raise ValueError("Backend source patch does not reproduce the marked modification")
    checked_asset(root, mod["license"])
    original = checked_asset(root, mod["original_spirv"])
    modified = checked_asset(root, mod["modified_spirv"])
    if len(original) != len(modified) or len(original) % 4:
        raise ValueError("Backend shader size mismatch")
    words = list(struct.unpack(f"<{len(original) // 4}I", original))
    if words[:1] != [0x07230203]:
        raise ValueError("Backend shader is not SPIR-V")
    offset, loops = 5, []
    while offset < len(words):
        count, opcode = words[offset] >> 16, words[offset] & 65535
        if not count or offset + count > len(words):
            raise ValueError("Invalid backend SPIR-V instruction")
        if opcode == 246:  # OpLoopMerge
            if count != 4:
                raise ValueError("Unexpected backend loop-control operands")
            loops.append(offset + 3)
        offset += count
    if len(loops) != 2 or [words[i] for i in loops] != [1, 1] or loops[1] != mod["control_word"]:
        raise ValueError("Backend loop-control boundary changed")
    words[loops[1]] = 2  # DontUnroll; first average-resolve loop is untouched.
    if struct.pack(f"<{len(words)}I", *words) != modified:
        raise ValueError("Backend shader changes exceed one loop-control word")
    return original, modified


def patch_backend(data, architecture, metadata, root=ROOT):
    relative = ARCH_PATHS[architecture]
    if sha(data) != metadata["files"][relative]:
        raise ValueError("Original GPLALL backend checksum mismatch")
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("Original GPLALL backend is not PE")
    pe = struct.unpack_from("<I", data, 60)[0]
    expected_machine = 0x14c if architecture == "x86" else 0x8664
    if pe > len(data) - 24 or data[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", data, pe + 4)[0] != expected_machine:
        raise ValueError("Original GPLALL backend architecture mismatch")
    original, modified = validated_shaders(root, metadata)
    mod = metadata["modification"]
    offset = data.find(original)
    if data.count(original) != 1 or offset != mod["embedded_offsets"][relative]:
        raise ValueError("Expected one original shader at the pinned backend offset")
    result = data[:offset] + modified + data[offset + len(original):]
    if sha(result) != mod["files"][relative]:
        raise ValueError("Modified GPLALL backend checksum mismatch")
    return result


def package_backend_info(root, metadata, backends):
    """Validate modified production inputs before package output is created."""
    archive = root / ".deps/gplall/backend.zip"
    hashes = {arch: sha(data) for arch, data in backends.items()}
    info = {"files": hashes, "default_architecture": "x86_64", "source": "custom -DxvkDll / -Dxvk32Dll"}
    archive_matches = archive.is_file() and sha(archive.read_bytes()) == metadata["sha256"]
    if "modification" in metadata:
        if not archive_matches:
            raise ValueError("Modified backend requires the pinned original GPLALL archive")
        with zipfile.ZipFile(archive) as source:
            for arch, relative in ARCH_PATHS.items():
                if source.namelist().count(relative) != 1:
                    raise ValueError("Original GPLALL archive member missing or duplicated")
                expected = patch_backend(source.read(relative), arch, metadata, root)
                if backends[arch] != expected:
                    raise ValueError(f"Packaged {arch} backend does not match the reviewed modification")
        info.update(copy.deepcopy(metadata))
        info["source"] = metadata["release"] + " (modified: " + metadata["modification"]["id"] + ")"
        info["original_files"] = info["files"]
    elif archive_matches and hashes == {arch: metadata["files"][relative] for arch, relative in ARCH_PATHS.items()}:
        info.update(copy.deepcopy(metadata))
        info["source"] = metadata["release"]
    else:
        return info
    info["archive_sha256"] = info.pop("sha256")
    info["files"] = hashes
    return info


def prepare(source, output, architecture, root=ROOT):
    metadata = json.loads((root / "config/backend.json").read_text(encoding="utf-8"))
    if source.resolve() == output.resolve():
        raise ValueError("Do not overwrite the pinned original backend")
    data = source.read_bytes()
    result = patch_backend(data, architecture, metadata, root)
    if output.exists() and output.read_bytes() != result:
        raise ValueError("Existing modified backend differs; preserve or move it before retrying")
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists():
        output.write_bytes(result)
    receipt = {"kind": "verified-shader-only-modification", "architecture": architecture,
               "source": str(source.resolve()), "output": str(output.resolve()),
               "source_sha256": sha(data), "output_sha256": sha(result),
               "original_shader_sha256": metadata["modification"]["original_spirv"]["sha256"],
               "modified_shader_sha256": metadata["modification"]["modified_spirv"]["sha256"],
               "modification": metadata["modification"]["id"], "entire_backend_rebuilt": False}
    output.with_suffix(".recipe.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--architecture", choices=ARCH_PATHS, required=True)
    args = parser.parse_args()
    print(prepare(args.input, args.output, args.architecture))
