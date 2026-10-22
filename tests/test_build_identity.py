import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"scripts"))
from build_identity import recipe_digest, protocol_digest, runtime_build_id, upstream_commit, INPUT_FILES

class Identity(unittest.TestCase):
    def test_same_version_distinct_upstream_has_distinct_startup_and_channel_identity(self):
        patch = b"unchanged cumulative patch\n"
        old = "029602f83a2fdec8f4d4419e1967bf231fb8f0b5"
        new = "cf83c0514d5690a2e54369ba24a6d044b989e6c8"
        digests = [protocol_digest(patch, "1.5.17", pin) for pin in (old, new)]
        self.assertNotEqual(digests[0][:32], digests[1][:32])
        for pin, digest in zip((old, new), digests):
            self.assertEqual(runtime_build_id(patch, "1.5.17", pin), "l4d2-1.5.17+" + digest)
            self.assertEqual(digest, protocol_digest(patch.replace(b"\n", b"\r\n"), "1.5.17", pin.upper()))
        self.assertNotEqual(runtime_build_id(patch, "1.5.17", old), runtime_build_id(patch, "1.5.17", new))

    def test_full_sha_required_in_both_runtime_guards(self):
        for pin in (None, "", "main", "cf83c05", "g" * 40, "a" * 39, "a" * 41, "a" * 40 + "\n"):
            for function in (protocol_digest, runtime_build_id):
                with self.subTest(pin=pin, function=function.__name__), self.assertRaises(ValueError):
                    function(b"patch", "1.5.17", pin)
        self.assertEqual(upstream_commit("A" * 40), "a" * 40)

    def test_runtime_inputs_change_identity_but_docs_and_line_endings_do_not(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name in INPUT_FILES:
                (root/name).parent.mkdir(parents=True, exist_ok=True)
                (root/name).write_bytes(b"test\n")
            (root/"config").mkdir(); config=root/"config/backend.json"
            config.write_bytes(b"one\n"); before=recipe_digest(root)
            config.write_bytes(b"one\r\n"); self.assertEqual(before,recipe_digest(root))
            (root/"README.md").write_bytes(b"docs"); self.assertEqual(before,recipe_digest(root))
            config.write_bytes(b"two\n"); self.assertNotEqual(before,recipe_digest(root))
            before=recipe_digest(root); (root/"patches").mkdir()
            (root/"patches/new.patch").write_bytes(b"patch\n"); self.assertNotEqual(before,recipe_digest(root))
            before=recipe_digest(root)
            (root/"docs/THINFLEX-TEST-README.txt").write_bytes(b"installation correction\n")
            self.assertNotEqual(before,recipe_digest(root))
            before=recipe_digest(root); (root/"runtime").mkdir()
            binary=root/"runtime/studiorender.dll"
            binary.write_bytes(b"MZ\0\r\n"); self.assertNotEqual(before,recipe_digest(root))
            before=recipe_digest(root)
            binary.write_bytes(b"MZ\0\n"); self.assertNotEqual(before,recipe_digest(root))

    def test_plugin_and_experiment_sources_change_recipe(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in INPUT_FILES:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")
            before = recipe_digest(root)
            for relative in ("plugins/l4n/plugin.cpp", "experiments/dxvk-memory/prepare.py",
                             "patches/gplall-msaa-loop/modified.frag",
                             "patches/gplall-msaa-loop/modified.spv"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"new source")
                self.assertNotEqual(before, recipe_digest(root))
                before = recipe_digest(root)
