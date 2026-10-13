"""Fail-closed shader-only backend recipes; fabricated PE files, no GPU calls."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
import warnings
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
import backend_recipe as recipe


class BackendRecipe(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        shutil.copytree(recipe.ROOT / "patches/gplall-msaa-loop", self.root / "patches/gplall-msaa-loop")
        self.metadata = json.loads((recipe.ROOT / "config/backend.json").read_text())
        self.original, self.modified = recipe.validated_shaders(self.root, self.metadata)
        self.backends = {}
        for arch, relative in recipe.ARCH_PATHS.items():
            data = bytearray(128)
            data[:2] = b"MZ"
            struct.pack_into("<I", data, 60, 64)
            data[64:68] = b"PE\0\0"
            struct.pack_into("<H", data, 68, 0x14c if arch == "x86" else 0x8664)
            data += self.original + b"unchanged suffix"
            data = bytes(data)
            self.backends[arch] = data
            self.metadata["files"][relative] = recipe.sha(data)
            self.metadata["modification"]["embedded_offsets"][relative] = 128
            self.metadata["modification"]["files"][relative] = recipe.sha(data.replace(self.original, self.modified))
        (self.root / "config").mkdir()
        self.save_metadata()

    def save_metadata(self):
        (self.root / "config/backend.json").write_text(json.dumps(self.metadata), encoding="utf-8")

    def archive(self, duplicate=False):
        path = self.root / ".deps/gplall/backend.zip"
        path.parent.mkdir(parents=True, exist_ok=True)
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(path, "w") as out:
                for arch, relative in recipe.ARCH_PATHS.items():
                    out.writestr(relative, self.backends[arch])
                if duplicate:
                    out.writestr("x32/d3d9.dll", self.backends["x86"])
        self.metadata["sha256"] = recipe.sha(path.read_bytes())
        return path

    def test_each_architecture_changes_only_one_expected_byte(self):
        for arch, data in self.backends.items():
            with self.subTest(arch=arch):
                output = recipe.patch_backend(data, arch, self.metadata, self.root)
                self.assertEqual([i for i, (a, b) in enumerate(zip(data, output)) if a != b], [128 + 4712])
                self.assertEqual(output[128 + 4712], 2)
                self.assertEqual(recipe.sha(output), self.metadata["modification"]["files"][recipe.ARCH_PATHS[arch]])

    def test_corrupt_original_is_rejected_before_output(self):
        source, output = self.root / "original.dll", self.root / "output/d3d9.dll"
        source.write_bytes(self.backends["x64"] + b"corruption")
        before = source.read_bytes()
        with self.assertRaisesRegex(ValueError, "Original GPLALL backend checksum"):
            recipe.prepare(source, output, "x64", self.root)
        self.assertEqual(source.read_bytes(), before)
        self.assertFalse(output.parent.exists())

    def test_absent_or_duplicate_shader_is_rejected_even_with_input_hash(self):
        for data in (self.backends["x64"].replace(self.original, b"\0" * len(self.original)), self.backends["x64"] + self.original):
            with self.subTest(length=len(data)):
                metadata = copy.deepcopy(self.metadata)
                metadata["files"]["x64/d3d9.dll"] = recipe.sha(data)
                with self.assertRaisesRegex(ValueError, "one original shader"):
                    recipe.patch_backend(data, "x64", metadata, self.root)

    def test_wrong_offset_output_hash_or_architecture_is_rejected(self):
        metadata = copy.deepcopy(self.metadata)
        metadata["modification"]["embedded_offsets"]["x64/d3d9.dll"] += 4
        with self.assertRaisesRegex(ValueError, "pinned backend offset"):
            recipe.patch_backend(self.backends["x64"], "x64", metadata, self.root)
        metadata = copy.deepcopy(self.metadata)
        metadata["modification"]["files"]["x64/d3d9.dll"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "Modified GPLALL backend checksum"):
            recipe.patch_backend(self.backends["x64"], "x64", metadata, self.root)
        bad = bytearray(self.backends["x64"])
        struct.pack_into("<H", bad, 68, 0x14c)
        metadata["files"]["x64/d3d9.dll"] = recipe.sha(bad)
        with self.assertRaisesRegex(ValueError, "architecture"):
            recipe.patch_backend(bytes(bad), "x64", metadata, self.root)

    def test_source_math_change_is_rejected_even_with_updated_asset_hash(self):
        entry = self.metadata["modification"]["modified_source"]
        path = self.root / entry["path"]
        changed = path.read_bytes().replace(b"0.0001f", b"0.01f")
        path.write_bytes(changed)
        entry["sha256"] = recipe.sha(changed)
        with self.assertRaisesRegex(ValueError, "source changes exceed"):
            recipe.validated_shaders(self.root, self.metadata)

    def test_additional_spirv_change_is_rejected_even_with_updated_asset_hash(self):
        entry = self.metadata["modification"]["modified_spirv"]
        path = self.root / entry["path"]
        changed = bytearray(path.read_bytes())
        changed[100] ^= 1
        path.write_bytes(changed)
        entry["sha256"] = recipe.sha(changed)
        with self.assertRaisesRegex(ValueError, "one loop-control word"):
            recipe.validated_shaders(self.root, self.metadata)

    def test_unverified_recipe_assets_fail_closed(self):
        for name in ("original_source", "modified_source", "source_patch", "original_spirv", "modified_spirv", "license"):
            with self.subTest(name=name):
                metadata = copy.deepcopy(self.metadata)
                metadata["modification"][name]["sha256"] = "0" * 64
                with self.assertRaisesRegex(ValueError, "asset checksum"):
                    recipe.validated_shaders(self.root, metadata)

    def test_prepare_keeps_original_and_refuses_conflicting_existing_output(self):
        source, output = self.root / "original.dll", self.root / "output/d3d9.dll"
        source.write_bytes(self.backends["x64"])
        recipe.prepare(source, output, "x64", self.root)
        expected = output.read_bytes()
        recipe.prepare(source, output, "x64", self.root)
        self.assertEqual(output.read_bytes(), expected)
        self.assertEqual(source.read_bytes(), self.backends["x64"])
        output.write_bytes(b"preserve me")
        with self.assertRaisesRegex(ValueError, "Existing modified backend differs"):
            recipe.prepare(source, output, "x64", self.root)
        self.assertEqual(output.read_bytes(), b"preserve me")
        with self.assertRaisesRegex(ValueError, "Do not overwrite"):
            recipe.prepare(source, source, "x64", self.root)

    def test_package_reconstructs_original_archive_and_records_modified_provenance(self):
        self.archive()
        backends = {arch: recipe.patch_backend(data, arch, self.metadata, self.root) for arch, data in self.backends.items()}
        info = recipe.package_backend_info(self.root, self.metadata, backends)
        self.assertEqual(info["original_files"], self.metadata["files"])
        self.assertEqual(info["files"], {arch: recipe.sha(data) for arch, data in backends.items()})
        self.assertEqual(info["modification"], self.metadata["modification"])
        self.assertIn("modified: msaa-linear-dont-unroll", info["source"])
        backends["x86"] = self.backends["x86"]
        with self.assertRaisesRegex(ValueError, "does not match"):
            recipe.package_backend_info(self.root, self.metadata, backends)

    def test_missing_bad_or_ambiguous_archive_is_not_silently_custom(self):
        with self.assertRaisesRegex(ValueError, "pinned original GPLALL archive"):
            recipe.package_backend_info(self.root, self.metadata, self.backends)
        path = self.archive()
        path.write_bytes(b"bad archive")
        with self.assertRaisesRegex(ValueError, "pinned original GPLALL archive"):
            recipe.package_backend_info(self.root, self.metadata, self.backends)
        self.archive(duplicate=True)
        with self.assertRaisesRegex(ValueError, "missing or duplicated"):
            recipe.package_backend_info(self.root, self.metadata, self.backends)


if __name__ == "__main__":
    unittest.main()
