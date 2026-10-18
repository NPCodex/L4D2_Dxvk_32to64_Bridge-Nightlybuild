"""Pin the complete Starfelll distribution and explicit user preset, without private material."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
import l4n_payload as l4n


class L4NPayload(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = l4n.source_l4n_files()

    def test_exact_distribution_and_original_author_readme(self):
        metadata = l4n.manifest_metadata(self.files[l4n.MANIFEST_NAME])
        self.assertEqual(metadata["author"], "Starfelll")
        self.assertEqual(metadata["original_author_signature"], "@Starfelll")
        self.assertEqual(metadata["version"], "2.53.0")
        self.assertEqual(len(metadata["files"]), 79)
        self.assertEqual(sum(len(self.files[name]) for name in metadata["files"]), 23748123)
        self.assertEqual(hashlib.sha256(self.files["readme_l4n.txt"]).hexdigest(),
                         metadata["source_archive_files"]["readme_l4n.txt"]["sha256"])
        self.assertIn("bin/neko/other_tools.7z", self.files)
        self.assertIn("bin/left4neko_server.dll", self.files)
        self.assertNotIn("left4dead2/neko/config_template.vdf", self.files)
        self.assertIn("left4dead2/neko/config.vdf", self.files)
        self.assertNotIn("logs.7z", self.files)
        self.assertNotIn("L4N_v2.51.0.7z", self.files)
        self.assertNotIn("bin/d3d9.dll", self.files)
        self.assertNotIn("bin/.l4d2bridge/bridge.conf", self.files)

    def test_complete_original_distribution_with_only_template_merge_exception(self):
        metadata = l4n.manifest_metadata(self.files[l4n.MANIFEST_NAME])
        originals = metadata["source_archive_files"]
        template = "left4dead2/neko/config_template.vdf"
        config = "left4dead2/neko/config.vdf"
        self.assertEqual(len(originals), 77)
        self.assertEqual(set(originals) - set(metadata["files"]), {template})
        self.assertEqual(set(metadata["files"]) - set(originals), {
            "dxvk.conf", config, "left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs"})
        for name, expected in originals.items():
            if name != template:
                self.assertEqual(metadata["files"][name], expected, name)
        merge = metadata["config_merge"]
        self.assertEqual(merge["new_template"], originals[template])
        self.assertEqual(merge["merged_config"], metadata["files"][config])
        text = self.files[config].decode("utf-8")
        self.assertIn("!force/", text)
        self.assertIn("VPK Overlay(VPK挂载层)", text)
        self.assertRegex(text, r'(?m)^[ \t]*// "-nekook" ".*"\r?$')
        self.assertNotIn("generate_addoninfo", text)
        self.assertNotIn("l4n_bonesetup_patch", text)
        self.assertIn('"index_buffer_size" "131072"', text)
        self.assertIn('\t"-novid"\t""', text)
        self.assertIn('\t-l4n_use_neko_engine_post ""', text)

    def test_binary_config_and_manifest_tampering_rejected(self):
        for name in ("left4dead2.exe", "bin/left4neko.dll", "bin/left4neko_server.dll",
                     "dxvk.conf", "readme_l4n.txt",
                     "left4dead2/neko/config.vdf"):
            with self.subTest(name=name):
                files = dict(self.files)
                files[name] += b"tampered"
                with self.assertRaisesRegex(ValueError, "differs"):
                    l4n.validate_l4n_files(files)
        files = dict(self.files)
        metadata = json.loads(files[l4n.MANIFEST_NAME])
        metadata["files"]["logs.7z"] = {"size": 1, "sha256": "a" * 64}
        files[l4n.MANIFEST_NAME] = json.dumps(metadata).encode()
        with self.assertRaisesRegex(ValueError, "manifest differs"):
            l4n.validate_l4n_files(files)

    def test_manifest_formatting_does_not_change_identity_but_extra_files_fail(self):
        files = dict(self.files)
        files[l4n.MANIFEST_NAME] = json.dumps(json.loads(files[l4n.MANIFEST_NAME]), indent=1).replace("\n", "\r\n").encode()
        l4n.validate_l4n_files(files)
        files["private.log"] = b"private"
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            l4n.validate_l4n_files(files)
        files = dict(self.files)
        del files["left4dead2.exe"]
        with self.assertRaisesRegex(ValueError, "Unexpected or missing"):
            l4n.validate_l4n_files(files)

    def test_source_directory_must_not_contain_unrecorded_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = root / "runtime/l4n/payload"
            payload.mkdir(parents=True)
            (root / "runtime/l4n/manifest.json").write_bytes(self.files[l4n.MANIFEST_NAME])
            (payload / "logs.7z").write_bytes(b"not distributable")
            with self.assertRaisesRegex(ValueError, "Unexpected or missing source"):
                l4n.source_l4n_files(root)

    def test_reparse_point_inputs_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "linked"
            path.write_bytes(b"fixture")
            attributes = mock.Mock(st_file_attributes=0x400, st_mode=0x8000)
            with mock.patch.object(Path, "lstat", return_value=attributes):
                with self.assertRaisesRegex(ValueError, "Linked L4N input"):
                    l4n._read_regular(path, Path(directory))


if __name__ == "__main__":
    unittest.main()
