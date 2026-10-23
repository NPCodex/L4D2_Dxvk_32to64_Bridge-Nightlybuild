"""The full ZIP carries the exact engine repair and complete approved L4N payload."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
import archive_runtime as runtime
import engine_payload as engine
import l4n_payload as l4n
import quick_tool_payload as quick
import package_quick_tool as tool_update


class Packaging(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        for name in runtime.REQUIRED + runtime.NOTICE_INPUTS + ("UPSTREAM.json",):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture")
        self.engine_files = engine.source_engine_files()
        self.l4n_files = l4n.source_l4n_files()
        for relative, data in {**self.engine_files, **self.l4n_files}.items():
            destination = self.source / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
        (self.source / "VERSION").write_bytes(b"1.1.4\n")
        manifest = {name: hashlib.sha256((self.source / name).read_bytes()).hexdigest()
                    for name in runtime.REQUIRED if name.endswith((".dll", ".exe"))}
        (self.source / "SHA256.json").write_text(json.dumps(manifest), encoding="utf-8")
        self.output = self.root / "full.zip"
        self.quick_tool = self.root / "quick-tool-build"
        self.quick_tool.mkdir()
        executable = bytearray(1024)
        executable[:2] = b"MZ"
        struct.pack_into("<I", executable, 60, 128)
        executable[128:132] = b"PE\0\0"
        struct.pack_into("<H", executable, 132, 0x14C)
        struct.pack_into("<H", executable, 148, 224)
        struct.pack_into("<H", executable, 152, 0x10B)
        struct.pack_into("<H", executable, 152 + 68, 2)
        struct.pack_into("<I", executable, 152 + 92, 16)
        struct.pack_into("<II", executable, 152 + 96 + 14 * 8, 512, 72)
        (self.quick_tool / "BridgeQuickTool.exe").write_bytes(executable)
        receipt = {"schema": 1, "product": "BridgeQuickTool", "version": "1.1.0",
                   "exe_sha256": hashlib.sha256(executable).hexdigest(),
                   "sources": {name: hashlib.sha256((quick.ROOT / name).read_bytes()).hexdigest()
                               for name in quick.BUILD_INPUTS}}
        (self.quick_tool / "quick-tool-build.json").write_text(json.dumps(receipt), encoding="utf-8")

    def test_full_package_contains_complete_l4n_and_only_approved_integration_files(self):
        for name in ("bin/.l4d2bridge/ReShade.dll", "bin/.l4d2bridge/resource-retention.db",
                     "bin/other-engine.dll", "player.dmp", "private.log", "docs/guide.md",
                     "config/example.conf", "scripts/tool.py", "config.json", "licenses/private.txt",
                     "README_merge.md", "package-manifest.json", ".build-owned.json",
                     "scripts/install.ps1", "scripts/restore.ps1", "docs/validation.md",
                     "necola/l4n_bridge.log", "left4dead2_necola.exe", "start.bat"):
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"must not ship")
        runtime.runtime_archive(self.source, self.output, self.quick_tool)
        with zipfile.ZipFile(self.output) as archive:
            names = set(archive.namelist())
            # Derive coverage independently from the pinned source manifest,
            # not from the packager's selector: omitted tools must fail here.
            l4n_paths = set(l4n.manifest_metadata(self.l4n_files[l4n.MANIFEST_NAME])["files"])
            expected = set(runtime.REQUIRED) | l4n_paths | {
                "bin/studiorender.dll", "README.txt", "THIRD-PARTY-NOTICES.txt",
                "L4D2-Quick-Tool.cmd", "tools/BridgeQuickTool/BridgeQuickTool.exe",
                "tools/BridgeQuickTool/README.txt"}
            expected.remove("optional/L4N/L4D2BridgePlugin.dll")
            expected.add("bin/neko/plugins/L4D2BridgePlugin.dll")
            self.assertEqual(names, expected)
            self.assertEqual(len(names), 107)
            self.assertEqual(archive.read(quick.EXE), (self.quick_tool / "BridgeQuickTool.exe").read_bytes())
            self.assertEqual(archive.read(quick.LAUNCHER), (quick.ROOT / "tools/quick-tool/Launch.cmd").read_bytes())
            self.assertIn(b"No compilation required", archive.read(quick.GUIDE))
            self.assertIn(b"--game-root", archive.read(quick.LAUNCHER))
            self.assertIn("L4D2-Quick-Tool.cmd", archive.read("README.txt").decode("utf-8"))
            for name in runtime.REQUIRED:
                destination = ("bin/neko/plugins/L4D2BridgePlugin.dll"
                               if name == "optional/L4N/L4D2BridgePlugin.dll" else name)
                self.assertEqual(archive.read(destination), (self.source / name).read_bytes())
            self.assertFalse(any(name.startswith("optional/") for name in names))
            self.assertEqual(archive.read("bin/studiorender.dll"), self.engine_files["bin/studiorender.dll"])
            for name in l4n_paths:
                self.assertEqual(archive.read(name), self.l4n_files[name])
            self.assertIn("left4dead2/neko/server_name_filter_template.txt", names)
            self.assertIn("left4dead2/bin/game_shader_generic_neko", names)
            # These original L4N companion files were lost during ZIP slimming.
            # Check delivery independently of the packager's runtime selection.
            for basename in ("localize_overrides_template.vdf", "mdl_extension.qc",
                             "neko_proxy.vmt", "scheme_overrides_template.vdf",
                             "sequence_event_template.vdf"):
                name = "left4dead2/neko/" + basename
                self.assertIn(name, names)
                self.assertEqual(archive.read(name), self.l4n_files[name])
            self.assertNotIn("left4dead2/neko/config_template.vdf", names)
            self.assertEqual({name for name in names if name.endswith(".json")},
                             {"necola/FeatureConfig.json", "necola/weaponParticles.json"})
            self.assertFalse(any(name.endswith((".md", ".py", ".ps1")) for name in names))
            self.assertFalse(any(name.startswith(("scripts/", "docs/", "config/", "licenses/")) for name in names))
            self.assertEqual({name for name in names if name.startswith("bin/neko/")},
                             {name for name in l4n_paths if name.startswith("bin/neko/")}
                             | {"bin/neko/plugins/L4D2BridgePlugin.dll"})
            for name in ("bin/left4neko_server.dll", "bin/neko/other_tools.7z", "bin/neko/plugins/l4n_plugin.h",
                         "bin/neko/to_l4n_survivor.bat", "to_l4n_survivor.exe",
                         "left4dead2 no-insecure.bat", "readme_l4n.txt"):
                self.assertIn(name, names)
                self.assertEqual(archive.read(name), self.l4n_files[name])
            self.assertIn("bin/neko/plugins/l4n_necola_bridge.dll", names)
            self.assertIn("bin/neko/necola/necola.dll", names)
            self.assertIn("bin/neko/plugins/L4D2BridgePlugin.dll", names)
            self.assertEqual(len([name for name in names
                                  if name.startswith("left4dead2/addons/l4n_necola_audio_")]), 10)
            for name in ("README_merge.md", "package-manifest.json", ".build-owned.json",
                         "necola/l4n_bridge.log", "left4dead2_necola.exe", "start.bat"):
                self.assertNotIn(name, names)
            notices = archive.read("THIRD-PARTY-NOTICES.txt")
            for name in runtime.NOTICE_INPUTS:
                self.assertIn((self.source / name).read_bytes(), notices)
            self.assertNotIn(b"must not ship", notices)
            guide = archive.read("README.txt").decode("utf-8")
            self.assertIn("v1.1.4", guide)
            self.assertIn("无需运行修复工具", guide)
            self.assertIn("原始备份", guide)
            self.assertIn("Starfelll", guide)
            self.assertIn(f"完整保留 L4N {l4n.VERSION}", guide)
            self.assertIn("模板不单独附带", guide)
            self.assertIn("bin/neko/plugins/L4D2BridgePlugin.dll", guide)
            self.assertNotIn("optional/", guide)
            self.assertIn(engine.ORIGINAL_SHA256, guide)
            self.assertIn(engine.PATCHED_SHA256, guide)
            self.assertNotIn("ENGINE-PATCH.json", guide)
            self.assertNotIn("L4N-PAYLOAD.json", guide)
        before = self.output.read_bytes()
        with self.assertRaises(FileExistsError):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertEqual(self.output.read_bytes(), before)

    def test_missing_engine_is_rejected(self):
        (self.source / "bin/studiorender.dll").unlink()
        with self.assertRaises(FileNotFoundError):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertFalse(self.output.exists())

    def test_legacy_tool_is_not_published(self):
        tool = self.source / "tools/thinflex/ThinFlexPatch.exe"
        tool.parent.mkdir(parents=True)
        tool.write_bytes(b"old bundled Python tool")
        runtime.runtime_archive(self.source, self.output, self.quick_tool)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual({name for name in archive.namelist() if name.startswith("tools/")},
                             {"tools/BridgeQuickTool/BridgeQuickTool.exe", "tools/BridgeQuickTool/README.txt"})

    def test_engine_and_record_tampering_are_rejected(self):
        for name in ("bin/studiorender.dll", "ENGINE-PATCH.json"):
            with self.subTest(name=name):
                path = self.source / name
                data = path.read_bytes()
                path.write_bytes(data + b"tampered")
                with self.assertRaisesRegex(ValueError, "differs"):
                    runtime.runtime_archive(self.source, self.output, self.quick_tool)
                self.assertFalse(self.output.exists())
                path.write_bytes(data)

    def test_archive_uses_verified_engine_snapshot(self):
        original_read = runtime.staged_engine_files
        original_bytes = self.engine_files["bin/studiorender.dll"]

        def change_after_read(source):
            files = original_read(source)
            (source / "bin/studiorender.dll").write_bytes(b"unverified later contents")
            return files

        with patch.object(runtime, "staged_engine_files", change_after_read):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read("bin/studiorender.dll"), original_bytes)

    def test_archive_uses_verified_l4n_snapshot_and_excludes_private_archives(self):
        original_read = runtime.staged_l4n_files
        expected = self.l4n_files["left4dead2.exe"]
        for name in ("logs.7z", "L4N_v2.51.0.7z", "crash_dumps/player.dmp"):
            (self.source / name).write_bytes(b"private or duplicate archive")

        def change_after_read(source):
            files = original_read(source)
            (source / "left4dead2.exe").write_bytes(b"unverified later contents")
            return files

        with patch.object(runtime, "staged_l4n_files", change_after_read):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read("left4dead2.exe"), expected)
            for name in ("logs.7z", "L4N_v2.51.0.7z", "crash_dumps/player.dmp"):
                self.assertNotIn(name, archive.namelist())
            self.assertEqual(archive.read("bin/neko/other_tools.7z"),
                             self.l4n_files["bin/neko/other_tools.7z"])

    def test_shipped_companion_tools_require_valid_provenance(self):
        path = self.source / "bin/neko/other_tools.7z"
        path.write_bytes(b"unverified source archive")
        with self.assertRaisesRegex(ValueError, "differs"):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertFalse(self.output.exists())

    def test_payload_collisions_are_rejected_before_output(self):
        for name in ("bin/studiorender.dll", "BIN/D3D9.DLL",
                     "bin/neko/plugins/L4D2BridgePlugin.dll", "README.txt",
                     "THIRD-PARTY-NOTICES.txt", "TOOLS/BridgeQuickTool/BridgeQuickTool.exe",
                     "L4D2-Quick-Tool.cmd"):
            with self.subTest(name=name):
                files = dict(self.l4n_files)
                files[name] = b"conflicting supplied file"
                with patch.object(runtime, "staged_l4n_files", return_value=files):
                    with self.assertRaisesRegex(ValueError, "Package file collision"):
                        runtime.runtime_archive(self.source, self.output, self.quick_tool)
                self.assertFalse(self.output.exists())

    def test_binary_receipt_or_required_license_failure_never_creates_output(self):
        binary = self.source / "bin/.l4d2bridge/L4D2Bridge64.exe"
        original = binary.read_bytes()
        binary.write_bytes(b"unverified replacement host")
        with self.assertRaisesRegex(ValueError, "source package receipt"):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertFalse(self.output.exists())
        binary.write_bytes(original)
        (self.source / "licenses/DXVK-GPLALL-LICENSE.txt").unlink()
        with self.assertRaises(FileNotFoundError):
            runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertFalse(self.output.exists())

    def test_competing_output_is_preserved(self):
        original_archive = runtime.archive

        def race(source, output):
            original_archive(source, output)
            self.output.write_bytes(b"another build")

        with patch.object(runtime, "archive", race):
            with self.assertRaises(FileExistsError):
                runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertEqual(self.output.read_bytes(), b"another build")

    def test_partial_final_archive_is_removed(self):
        original_copy = shutil.copyfileobj

        def fail(source, target, *args, **kwargs):
            if getattr(target, "name", None) == str(self.output):
                target.write(b"partial archive")
                raise OSError("simulated disk full")
            return original_copy(source, target, *args, **kwargs)

        with patch.object(shutil, "copyfileobj", fail):
            with self.assertRaisesRegex(OSError, "disk full"):
                runtime.runtime_archive(self.source, self.output, self.quick_tool)
        self.assertFalse(self.output.exists())

    def test_missing_stale_tampered_or_native_quick_tool_never_publishes(self):
        receipt_path = self.quick_tool / "quick-tool-build.json"
        original = receipt_path.read_bytes()
        executable = self.quick_tool / "BridgeQuickTool.exe"
        original_exe = executable.read_bytes()
        cases = ("missing", "stale", "tampered", "native")
        for case in cases:
            with self.subTest(case=case):
                receipt_path.write_bytes(original)
                executable.write_bytes(original_exe)
                receipt = json.loads(original)
                if case == "missing":
                    executable.unlink()
                elif case == "stale":
                    receipt["sources"]["tools/quick-tool/Core.cs"] = "0" * 64
                    receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
                elif case == "tampered":
                    executable.write_bytes(original_exe + b"tampered")
                else:
                    data = bytearray(original_exe)
                    struct.pack_into("<II", data, 152 + 96 + 14 * 8, 0, 0)
                    executable.write_bytes(data)
                    receipt["exe_sha256"] = hashlib.sha256(data).hexdigest()
                    receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
                with self.assertRaises((ValueError, FileNotFoundError)):
                    runtime.runtime_archive(self.source, self.output, self.quick_tool)
                self.assertFalse(self.output.exists())

    def test_only_approved_tool_files_ship_and_build_receipt_stays_internal(self):
        for relative in ("BridgeQuickTool.Tests.exe", "state/steam-launch-backups/private.vdf",
                         "reports/private.zip", "untrusted.exe", "debug.log"):
            path = self.quick_tool / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"private build state")
        runtime.runtime_archive(self.source, self.output, self.quick_tool)
        with zipfile.ZipFile(self.output) as archive:
            self.assertNotIn("quick-tool-build.json", archive.namelist())
            self.assertFalse(any("private" in name or "Tests.exe" in name for name in archive.namelist()))
            self.assertEqual(archive.read(quick.EXE), (self.quick_tool / "BridgeQuickTool.exe").read_bytes())

    def test_missing_quick_tool_argument_cannot_silently_build_an_incomplete_zip(self):
        with self.assertRaisesRegex(ValueError, "required"):
            runtime.runtime_archive(self.source, self.output, None)
        self.assertFalse(self.output.exists())

    def test_ready_to_extract_tool_update_contains_only_gui_launcher_and_guide(self):
        update = self.root / "L4D2-Quick-Tool.zip"
        tool_update.package(self.quick_tool, update)
        with zipfile.ZipFile(update) as archive:
            self.assertEqual(set(archive.namelist()), set(quick.PLAYER_PATHS))
            self.assertEqual(archive.read(quick.EXE), (self.quick_tool / "BridgeQuickTool.exe").read_bytes())
        self.assertEqual(Path(str(update) + ".sha256").read_text(encoding="ascii").strip(),
                         hashlib.sha256(update.read_bytes()).hexdigest() + "  " + update.name)
        before = update.read_bytes()
        with self.assertRaises(FileExistsError):
            tool_update.package(self.quick_tool, update)
        self.assertEqual(update.read_bytes(), before)

    def test_tool_update_never_packages_a_tampered_executable(self):
        update = self.root / "update.zip"
        (self.quick_tool / "BridgeQuickTool.exe").write_bytes(b"untrusted")
        with self.assertRaisesRegex(ValueError, "receipt"):
            tool_update.package(self.quick_tool, update)
        self.assertFalse(update.exists())
        self.assertFalse(Path(str(update) + ".sha256").exists())


if __name__ == "__main__":
    unittest.main()
