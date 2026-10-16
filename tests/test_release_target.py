"""Replacement builds preserve an existing identity without granting publication."""
import contextlib
import copy
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import detect_release
import publish_release
from release_target import read_release_target, resolve_release_target

VERSION = "1.5.17"
TAG = "v1.5.17-nightly-20261015-029602f8-r8b082b344703-b38856675685.1"
TARGET = {"schema": 1, "mode": "replace-existing", "repository": "owner/repo",
          "version": VERSION, "release_id": 42, "tag": TAG,
          "archive": "l4d2-bridge-" + TAG + ".zip"}
SHA = "a" * 40
RECIPE = "b" * 64


def release_objects():
    return {
        "repos/owner/repo/releases/42": {
            "id": 42, "tag_name": TAG, "draft": False, "prerelease": False,
            "assets": [{"name": TARGET["archive"] + suffix, "state": "uploaded", "size": 123}
                       for suffix in ("", ".sha256")]},
        "repos/owner/repo/releases/latest": {"id": 42},
        "repos/owner/repo/git/ref/tags/" + TAG: {
            "ref": "refs/tags/" + TAG, "object": {"type": "commit", "sha": "c" * 40}},
    }


class ReleaseTarget(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        (self.root / "config").mkdir()
        self.path = self.root / "config/release-target.json"

    def write(self, target=TARGET):
        self.path.write_text(json.dumps(target), encoding="utf-8")

    def test_missing_target_retains_normal_mode(self):
        self.assertIsNone(read_release_target(self.root, "owner/repo", VERSION))

    def test_valid_target_retains_exact_identity(self):
        self.write()
        self.assertEqual(read_release_target(self.root, "owner/repo", VERSION), TARGET)

    def test_invalid_policy_is_rejected(self):
        cases = [None, [], {}, *({**TARGET, key: value} for key, value in (
            ("schema", True), ("schema", 2), ("mode", "new-release"),
            ("repository", "wrong/repo"), ("version", "1.5.18"),
            ("release_id", True), ("release_id", "42"), ("release_id", 0),
            ("tag", "v1.5.18" + TAG[len("v1.5.17"):]), ("tag", "../" + TAG),
            ("tag", TAG + "\n"), ("tag", None),
            ("archive", "../" + TARGET["archive"]), ("archive", "new.zip")))]
        for target in cases:
            with self.subTest(target=target):
                self.write(target)
                with self.assertRaises(ValueError):
                    read_release_target(self.root, "owner/repo", VERSION)

    def test_malformed_json_is_rejected(self):
        self.path.write_text("{", encoding="utf-8")
        with self.assertRaises(ValueError):
            read_release_target(self.root, "owner/repo", VERSION)

    def test_valid_remote_target_disables_publication(self):
        objects = release_objects()
        api = Mock(side_effect=objects.__getitem__)
        result = resolve_release_target(TARGET, api)
        self.assertEqual(result, {"release_id": 42, "release_tag": TAG,
                                 "archive": TARGET["archive"],
                                 "delivery_mode": "replace-existing", "publish_allowed": False})
        self.assertEqual([call.args[0] for call in api.call_args_list], list(objects))

    def test_wrong_release_identity_or_status_is_rejected(self):
        for change in ({"id": 43}, {"tag_name": "other"}, {"draft": True},
                       {"prerelease": True}, {"draft": None}, {"prerelease": None}):
            with self.subTest(change=change):
                objects = release_objects()
                objects["repos/owner/repo/releases/42"].update(change)
                with self.assertRaisesRegex(ValueError, "identity"):
                    resolve_release_target(TARGET, objects.__getitem__)

    def test_asset_drift_is_rejected(self):
        good = release_objects()["repos/owner/repo/releases/42"]["assets"]
        cases = [[], good[:1], good + [good[0]], [good[0], good[0]], None, [None, None]]
        for change in ({"name": "other.zip"}, {"state": "starter"}, {"size": 0},
                       {"size": True}, {"size": "123"}):
            assets = copy.deepcopy(good)
            assets[0].update(change)
            cases.append(assets)
        for assets in cases:
            with self.subTest(assets=assets):
                objects = release_objects()
                objects["repos/owner/repo/releases/42"]["assets"] = assets
                with self.assertRaisesRegex(ValueError, "assets"):
                    resolve_release_target(TARGET, objects.__getitem__)

    def test_nonlatest_release_is_rejected(self):
        objects = release_objects()
        objects["repos/owner/repo/releases/latest"] = {"id": 43}
        with self.assertRaisesRegex(ValueError, "Latest"):
            resolve_release_target(TARGET, objects.__getitem__)

    def test_missing_or_malformed_tag_is_rejected(self):
        for reference in ({}, {"ref": "other", "object": {"type": "commit", "sha": SHA}},
                          {"ref": "refs/tags/" + TAG, "object": {"type": "tree", "sha": SHA}},
                          {"ref": "refs/tags/" + TAG, "object": {"type": "commit", "sha": "main"}},
                          {"ref": "refs/tags/" + TAG, "object": {"type": "commit", "sha": None}}):
            with self.subTest(reference=reference):
                objects = release_objects()
                objects["repos/owner/repo/git/ref/tags/" + TAG] = reference
                with self.assertRaisesRegex(ValueError, "tag"):
                    resolve_release_target(TARGET, objects.__getitem__)

    def test_remote_errors_never_become_a_new_release(self):
        for failed_path in release_objects():
            for code in (404, 403, 503):
                with self.subTest(path=failed_path, code=code):
                    objects = release_objects()
                    def api(path):
                        if path == failed_path:
                            raise HTTPError(path, code, "unavailable", {}, None)
                        return objects[path]
                    with self.assertRaises(HTTPError) as failure:
                        resolve_release_target(TARGET, api)
                    failure.exception.close()

    def run_detection(self, event="push", force=False, published=False, thinflex=False):
        self.write()
        (self.root / "VERSION").write_text(VERSION, encoding="utf-8")
        (self.root / "config/original-project.json").write_text(json.dumps({
            "repository": "upstream/project", "version": "1.3.0", "integration": "full",
            "commit": "c" * 40, "main_baseline": "c" * 40}), encoding="utf-8")
        objects = release_objects()
        objects.update({detect_release.UPSTREAM: {"default_branch": "main"},
                        detect_release.UPSTREAM + "/commits/main": {"sha": SHA},
                        detect_release.UPSTREAM + "/commits/" + SHA: {
                            "sha": SHA, "commit": {"committer": {"date": "2026-10-16T00:00:00Z"}}},
                        "repos/owner/repo/releases?per_page=100&page=1": [{
                            "tag_name": TAG, "draft": False,
                            "body": f"Upstream commit: {SHA}\nRecipe digest: {RECIPE if published else 'd' * 64}"}]})
        with patch.dict(os.environ, {
            "GITHUB_REPOSITORY": "owner/repo", "GITHUB_EVENT_NAME": event,
            "GITHUB_RUN_ID": "999", "GITHUB_RUN_ATTEMPT": "2", "UPSTREAM_COMMIT": "",
            "FORCE_REBUILD": str(force).lower(), "THINFLEX_TEST": str(thinflex).lower()}), \
                patch.object(detect_release, "ROOT", self.root), \
                patch.object(detect_release, "read_approval", return_value=("main", SHA)), \
                patch.object(detect_release, "recipe_digest", return_value=RECIPE), \
                patch.object(detect_release, "api", side_effect=objects.__getitem__) as api, \
                contextlib.redirect_stdout(io.StringIO()):
            return detect_release.pending(), api

    def test_all_triggers_preserve_target_and_only_build(self):
        for event, force in (("push", False), ("schedule", False), ("workflow_dispatch", True)):
            with self.subTest(event=event, force=force):
                rows, api = self.run_detection(event, force)
                self.assertEqual(len(rows), 1)
                row = rows[0]
                self.assertEqual(row["release_tag"], TAG)
                self.assertEqual(row["archive"], TARGET["archive"])
                self.assertFalse(row["publish_allowed"])
                self.assertEqual(row["delivery_mode"], "replace-existing")
                self.assertEqual(row["recipe_digest"], RECIPE)
                self.assertEqual(row["build_instance"],
                                 "v1.5.17-nightly-20261016-aaaaaaaa-rbbbbbbbbbbbb-b999.2")
                api.assert_any_call("repos/owner/repo/releases/42")

    def test_completed_recipe_still_skips_rebuild(self):
        rows, api = self.run_detection(published=True)
        self.assertEqual(rows, [])
        self.assertNotIn("repos/owner/repo/releases/42", [call.args[0] for call in api.call_args_list])

    def test_experimental_channel_cannot_replace_stable_release(self):
        with self.assertRaisesRegex(ValueError, "experimental"):
            self.run_detection(thinflex=True)

    def test_publisher_refuses_before_all_network_or_subprocess_calls(self):
        self.write()
        (self.root / "VERSION").write_text(VERSION, encoding="utf-8")
        with patch.dict(os.environ, {"GITHUB_REPOSITORY": "owner/repo", "RELEASE_TAG": TAG}), \
                patch.object(publish_release, "ROOT", self.root), \
                patch.object(publish_release, "api") as api, \
                patch.object(publish_release.subprocess, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "build-only"):
                publish_release.publish()
            api.assert_not_called()
            run.assert_not_called()

    def test_workflow_keeps_replacement_gate_and_native_evidence(self):
        workflow = (ROOT / ".github/workflows/upstream-release.yml").read_text(encoding="utf-8")
        publish = workflow.split("\n  publish:\n", 1)[1]
        self.assertIn("needs.detect.outputs.publish_allowed == 'true'", publish)
        self.assertIn("publish_allowed: ${{ steps.detect.outputs.publish_allowed }}", workflow)
        fetch = workflow.index("python scripts/fetch_reshade_keyboard_source.py")
        native = workflow.index("./scripts/test_overlay_reshade_keyboard.ps1")
        receipt = workflow.index("python scripts/record_build_validation.py")
        self.assertLess(fetch, native)
        self.assertLess(native, receipt)
        self.assertIn("-VcVarsVer 14.29", workflow[native:receipt])
        self.assertIn(".deps/overlay-reshade-keyboard/**", workflow)
        self.assertIn(".deps/reshade-keyboard-source/**", workflow)
        self.assertIn("BUILD_INSTANCE: ${{ matrix.build_instance }}", workflow)
        provenance = workflow.index("name: provenance-${{ matrix.release_tag }}")
        self.assertGreater(provenance, workflow.index("Set-Content dist/l4d2-bridge/UPSTREAM.json"))
        self.assertIn("path: dist/l4d2-bridge/UPSTREAM.json", workflow[provenance:])


if __name__ == "__main__":
    unittest.main()
