"""Pin the complete Starfelll distribution and explicit user preset, without private material."""
import configparser
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
import l4n_payload as l4n


def active_vdf(data):
    """Read active KeyValues while ignoring examples and comments in the preset."""
    tokens = []
    pattern = r'(?P<comment>//[^\r\n]*)|(?P<quoted>"(?:[^"\\]|\\.)*")|(?P<brace>[{}])|(?P<word>[^\s{}"]+)'
    for match in re.finditer(pattern, data.decode("utf-8")):
        kind = match.lastgroup
        value = match.group()
        if kind != "comment":
            tokens.append((kind, value[1:-1] if kind == "quoted" else value))
    position = 0

    def block(nested=False):
        nonlocal position
        result = {}
        while position < len(tokens):
            kind, key = tokens[position]
            position += 1
            if kind == "brace":
                if key == "}" and nested:
                    return result
                raise ValueError("Unexpected VDF brace")
            if key in result or position >= len(tokens):
                raise ValueError("Duplicate VDF key or missing value")
            kind, value = tokens[position]
            position += 1
            if kind == "brace":
                if value != "{":
                    raise ValueError("Missing VDF value")
                value = block(True)
            result[key] = value
        if nested:
            raise ValueError("Unclosed VDF block")
        return result

    return block()


class L4NPayload(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.files = l4n.source_l4n_files()

    def test_exact_distribution_and_original_author_readme(self):
        metadata = l4n.manifest_metadata(self.files[l4n.MANIFEST_NAME])
        self.assertEqual(metadata["author"], "Starfelll")
        self.assertEqual(metadata["original_author_signature"], "@Starfelll")
        self.assertEqual(metadata["version"], "2.55.0")
        self.assertEqual(len(metadata["files"]), 96)
        self.assertEqual(sum(len(self.files[name]) for name in metadata["files"]), 34152646)
        self.assertEqual(hashlib.sha256(self.files["readme_l4n.txt"]).hexdigest(),
                         metadata["source_archive_files"]["readme_l4n.txt"]["sha256"])
        self.assertIn("bin/neko/other_tools.7z", self.files)
        self.assertIn("bin/neko/读我_readme.txt", self.files)
        self.assertIn("bin/left4neko_server.dll", self.files)
        self.assertIn("bin/vphysics_jolt.dll", self.files)
        self.assertIn("crash_dumps/crashpad_handler.exe", self.files)
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
        self.assertEqual(len(originals), 79)
        self.assertEqual(set(originals) - set(metadata["files"]), {template})
        extension = metadata["necola_integration"]["files"]
        self.assertEqual(set(metadata["files"]) - set(originals), {
            "dxvk.conf", config, "left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs"}
            | set(extension))
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

    def test_pinned_combined_archive_has_explicit_original_file_scope(self):
        metadata = l4n.manifest_metadata(self.files[l4n.MANIFEST_NAME])
        self.assertEqual(metadata["source_archive_sha256"],
                         "041143bdd3df275af81ac71249fa01d41c73a3d7e6ae3e22e90879a4198c6a79")
        self.assertEqual(metadata["source_archive_bytes"], 17662400)
        origin = metadata["source_repository"]
        self.assertEqual(origin["url"], "https://github.com/NPCodex/l4n-necola")
        self.assertEqual(origin["commit"], "ba7104eecc4073f08ab609f87ce05ff50852bae2")
        self.assertEqual(origin["archive_path"], "release-assets/L4N-Necola.zip")
        self.assertEqual(origin["archive_git_blob"], "c1a11b7b6b97709310b9e63633ec4112b43608fd")
        self.assertIn("not a separately acquired L4N 7z", metadata["source_archive_files_scope"])
        self.assertEqual(metadata["original_file_count"], len(metadata["source_archive_files"]))
        extension = metadata["necola_integration"]
        self.assertEqual(extension["source_commit"], origin["commit"])
        self.assertEqual(extension["source_archive_sha256"], metadata["source_archive_sha256"])

    def test_necola_runtime_only_provenance_and_coexisting_plugin_paths(self):
        metadata = l4n.manifest_metadata(self.files[l4n.MANIFEST_NAME])
        extension = metadata["necola_integration"]
        self.assertEqual(extension["name"], "L4N-Necola")
        self.assertEqual(extension["necola_version"], "1.4.1")
        self.assertEqual(extension["source_archive_sha256"],
                         "041143bdd3df275af81ac71249fa01d41c73a3d7e6ae3e22e90879a4198c6a79")
        self.assertEqual(extension["source_archive_bytes"], 17662400)
        expected = {
            "bin/neko/plugins/l4n_necola_bridge.dll", "bin/neko/necola/necola.dll",
            "kpatch.ini", "necola/FeatureConfig.json", "necola/weaponParticles.json",
            "left4dead2/addons/l4n_necola_audio_base.vpk",
        } | {f"left4dead2/addons/l4n_necola_audio_ncl{index}0.vpk" for index in range(1, 10)}
        self.assertEqual(set(extension["files"]), expected)
        self.assertEqual(len(expected), 15)
        for name, provenance in extension["files"].items():
            self.assertEqual(metadata["files"][name], provenance, name)
        # The existing Bridge settings plugin is compiled separately, and must
        # not be replaced or shadowed by this x86 L4N runtime extension.
        self.assertNotIn("bin/neko/plugins/L4D2BridgePlugin.dll", metadata["files"])
        for name in extension["excluded_package_paths"]:
            self.assertNotIn(name, metadata["files"])
        for name in ("README_merge.md", "package-manifest.json", "left4dead2_necola.exe",
                     "start.bat", "scripts/install.ps1", "scripts/restore.ps1"):
            self.assertNotIn(name, metadata["files"])
        self.assertFalse(any(name.startswith(("docs/", "tools/", "src/", "tests/"))
                             for name in metadata["files"]))

    def test_necola_configuration_keeps_l4n_feature_ownership(self):
        settings = configparser.ConfigParser(interpolation=None, strict=True)
        settings.read_string(self.files["kpatch.ini"].decode("utf-8-sig"))
        for section, key in (("Necola", "openInspect"), ("Necola", "customParticleManifest"),
                             ("Necola", "enableHeapSizeHook"), ("AdsSupport", "enableAdsSupport")):
            self.assertIs(settings.getboolean(section, key), False, f"{section}.{key}")
        # Missing serverbrowser.dll must leave a bounded initialization wait.
        self.assertEqual(settings.getint("Necola", "injectWaitTime"), 5)
        features = json.loads(self.files["necola/FeatureConfig.json"])
        self.assertIs(features["InspectInitiative"]["EnableInspect"], False)
        self.assertIs(features["AdsSupport"]["EnableAdsSupport"], False)
        self.assertEqual(json.loads(self.files["necola/weaponParticles.json"]), [])

    def test_requested_jolt_is_the_only_active_preset_change(self):
        settings = active_vdf(self.files["left4dead2/neko/config.vdf"])
        launch = settings["config"]["launch_options"]
        self.assertEqual(launch, {"-novid": "", "-l4n_use_neko_engine_post": "", "-l4n_vphysics_jolt": ""})
        del launch["-l4n_vphysics_jolt"]
        canonical = json.dumps(settings, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()
        # Independently recorded semantic digest of the accepted 2.53 preset.
        # This covers all active values, including user commands and paths.
        self.assertEqual(hashlib.sha256(canonical).hexdigest(),
                         "de115b63c84ff4b5fc4afc4d5c3c498da7e8ca413ff388a765f36843eebeaf01")
        self.assertNotIn("l4n_loading_boost", self.files["left4dead2/neko/config.vdf"].decode())
        for name, expected in {
            "dxvk.conf": "f2c98df4461ab3108db6be00302249f75712b77aec0f0adc18fbc3199e52027f",
            "left4dead2/shaders/fxc/neko_floattoscreen_ps20b.vcs": "7a345f0042f889ea808ffe6fb93a80a80fe018b625b40f31c9f20be7ad1f98ec",
        }.items():
            self.assertEqual(hashlib.sha256(self.files[name]).hexdigest(), expected, name)

    def test_preset_parser_distinguishes_comments_and_rejects_duplicate_options(self):
        data = b'"config" { "launch_options" { // "-l4n_vphysics_jolt" ""\n "-novid" "" } }'
        self.assertEqual(active_vdf(data), {"config": {"launch_options": {"-novid": ""}}})
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            active_vdf(b'"config" { "launch_options" { "-l4n_vphysics_jolt" "" "-l4n_vphysics_jolt" "" } }')

    def test_binary_config_and_manifest_tampering_rejected(self):
        for name in ("left4dead2.exe", "bin/left4neko.dll", "bin/left4neko_server.dll",
                     "bin/vphysics_jolt.dll", "bin/neko/读我_readme.txt", "crash_dumps/crashpad_handler.exe",
                     "dxvk.conf", "readme_l4n.txt",
                     "left4dead2/neko/config.vdf", "bin/neko/plugins/l4n_necola_bridge.dll",
                     "bin/neko/necola/necola.dll", "kpatch.ini", "necola/FeatureConfig.json",
                     "necola/weaponParticles.json", "left4dead2/addons/l4n_necola_audio_base.vpk"):
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
