"""Offline checks for reviewed source selection and safe retry/deduplication."""
import contextlib
import importlib.util
import io
import json
import os
import sys
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from urllib.error import HTTPError

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))

spec = importlib.util.spec_from_file_location("detect", Path(__file__).parents[1] / "scripts/detect_release.py")
detect = importlib.util.module_from_spec(spec)
spec.loader.exec_module(detect)
SHA = "a" * 40
RECIPE = "b" * 64
ORIGINAL = {"repository": "upstream/project", "commit": "c" * 40,
            "version": "1.3.0", "integration": "full", "main_baseline": "c" * 40}
PARTIAL = {**ORIGINAL, "integration": "partial", "partial_version": "1.3.1",
           "reviewed_commit": "d" * 40, "partial_commit": "d" * 40}


class HeadDetection(unittest.TestCase):
    def run_case(self, existing=None, failure=None, manual="", branch="main", force=False,
                 thinflex=False, head_sha=SHA, approved=SHA, approval=None, resolved_sha=None,
                 original=None):
        calls = []
        def api(path):
            calls.append(path)
            if path == detect.UPSTREAM:
                return {"default_branch": branch}
            if "/commits/" in path:
                ref = path.rsplit("/", 1)[1]
                selected = manual.lower() if manual else approved.lower()
                sha = (resolved_sha or selected) if ref == selected else head_sha
                return {"sha": sha, "commit": {"committer": {"date": "2026-10-05T00:00:00Z"}}}
            if failure:
                raise HTTPError(path, failure, "failure", {}, None)
            if "/releases?" in path:
                return [] if existing is None else [dict(existing, tag_name="previous", body=existing.get("body", f"Upstream commit: {SHA}\nRecipe digest: {RECIPE}"))]
            raise HTTPError(path, 404, "missing", {}, None)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "config").mkdir()
            (root / "VERSION").write_text("1.5.3\n", encoding="utf-8")
            (root / "config/original-project.json").write_text(
                json.dumps(ORIGINAL if original is None else original), encoding="utf-8")
            approval_path = Path(directory) / "nvidia-upstream.json"
            approval_path.write_text(json.dumps(approval if approval is not None else {
                "repository": "https://github.com/NVIDIAGameWorks/dxvk-remix",
                "branch": "main", "approved_commit": approved}), encoding="utf-8")
            captured = io.StringIO()
            with patch.dict(os.environ, {"GITHUB_REPOSITORY": "owner/repo", "UPSTREAM_COMMIT": manual, "FORCE_REBUILD": str(force).lower(), "THINFLEX_TEST": str(thinflex).lower(), "GITHUB_RUN_ID":"123", "GITHUB_RUN_ATTEMPT":"1"}), patch.object(detect, "api", api), patch.object(detect, "recipe_digest", return_value=RECIPE), patch.object(detect, "APPROVAL_PATH", approval_path), patch.object(detect, "ROOT", root), contextlib.redirect_stdout(captured):
                rows = detect.pending()
            self.last_output = captured.getvalue()
        return rows, calls

    def test_observes_head_and_builds_approved_commit(self):
        rows, calls = self.run_case()
        self.assertEqual(rows[0]["commit"], SHA)
        self.assertEqual(rows[0]["branch"], "main")
        self.assertIn(detect.UPSTREAM + "/commits/main", calls)
        self.assertIn(detect.UPSTREAM + "/commits/" + SHA, calls)
        self.assertFalse(any(p.startswith(detect.UPSTREAM + "/releases?") for p in calls))

    def test_full_integration_keeps_full_sync_title(self):
        rows, _ = self.run_case(original=ORIGINAL)
        self.assertEqual(rows[0]["title"], "v1.5.3 L4N 整合包")

    def test_partial_integration_identifies_selected_version_without_full_sync_claim(self):
        rows, _ = self.run_case(original=PARTIAL)
        self.assertEqual(rows[0]["title"], "v1.5.3 L4N 整合包")
        self.assertNotIn("完整同步", rows[0]["title"])
        self.assertEqual(rows[0]["commit"], SHA)

    def test_incomplete_partial_provenance_fails_closed(self):
        for field in ("partial_version", "partial_commit", "reviewed_commit", "main_baseline"):
            original = dict(PARTIAL)
            del original[field]
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "L4D2"):
                self.run_case(original=original)
        for field in ("partial_commit", "reviewed_commit", "main_baseline"):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, "full .* SHA"):
                self.run_case(original={**PARTIAL, field: "main"})

    def test_unknown_integration_does_not_default_to_full_sync(self):
        for integration in (None, "", "partial-review", "complete"):
            with self.subTest(integration=integration), self.assertRaisesRegex(ValueError, "explicitly"):
                self.run_case(original={**ORIGINAL, "integration": integration})

    def test_default_branch_changes(self):
        rows, calls = self.run_case(branch="dev/bridge")
        self.assertEqual(rows[0]["branch"], "main")
        self.assertIn(detect.UPSTREAM + "/commits/dev%2Fbridge", calls)

    def test_unapproved_head_does_not_replace_approved_build_source(self):
        rows, calls = self.run_case(head_sha="d" * 40)
        self.assertEqual(rows[0]["commit"], SHA)
        self.assertIn(detect.UPSTREAM + "/commits/main", calls)
        self.assertIn("HEAD awaits review", self.last_output)
        self.assertIn("d" * 40, self.last_output)

    def test_unapproved_head_does_not_republish_completed_approved_build(self):
        rows, _ = self.run_case(head_sha="d" * 40, existing={"draft": False})
        self.assertEqual(rows, [])

    def test_new_approved_commit_builds_despite_previous_release(self):
        rows, _ = self.run_case(approved="d" * 40, head_sha="d" * 40,
                                existing={"draft": False})
        self.assertEqual(rows[0]["commit"], "d" * 40)

    def test_existing_success_skipped(self):
        self.assertEqual(self.run_case(existing={"draft": False})[0], [])

    def test_force_rebuild_bypasses_all_published_release_checks(self):
        rows, calls = self.run_case(existing={"draft": False}, force=True)
        self.assertEqual(rows[0]["commit"], SHA)
        self.assertFalse(any("/releases" in path for path in calls))

    def test_force_manual_commit(self):
        rows, calls = self.run_case(manual=SHA, force=True)
        self.assertEqual(rows[0]["branch"], "manual")

    def test_thinflex_test_has_distinct_title_tag_and_one_full_archive(self):
        rows, _ = self.run_case(manual=SHA, force=True, thinflex=True)
        row = rows[0]
        self.assertIn("-thinflex-test-", row["release_tag"])
        self.assertIn("ThinFlex", row["title"])
        self.assertIn("测试", row["title"])
        self.assertIn(row["release_tag"], row["archive"])
        self.assertNotIn("update_archive", row)
        self.assertNotIn("thinflex_archive", row)

    def test_channels_do_not_suppress_each_other(self):
        normal = {"draft": False, "body": f"Upstream commit: {SHA}\nRecipe digest: {RECIPE}"}
        experiment = {"draft": False, "body": f"Release channel: thinflex-test\nUpstream commit: {SHA}\nExperimental recipe digest: {RECIPE}"}
        self.assertTrue(self.run_case(existing=normal, thinflex=True)[0])
        self.assertTrue(self.run_case(existing=experiment)[0])
        self.assertEqual(self.run_case(existing=experiment, thinflex=True)[0], [])

    def test_draft_retried(self):
        self.assertEqual(len(self.run_case(existing={"draft": True})[0]), 1)

    def test_api_failure_not_mistaken_for_new_commit(self):
        with self.assertRaises(HTTPError):
            self.run_case(failure=403)

    def test_manual_override(self):
        rows, calls = self.run_case(manual="d" * 40)
        self.assertEqual(rows[0]["commit"], "d" * 40)
        self.assertEqual(rows[0]["branch"], "manual")
        self.assertNotIn(detect.UPSTREAM, calls)

    def test_force_does_not_approve_unreviewed_head(self):
        rows, _ = self.run_case(head_sha="d" * 40, force=True)
        self.assertEqual(rows[0]["commit"], SHA)

    def test_bad_approved_pin_fails_closed(self):
        for approved in ("main", "a" * 39, "g" * 40, "a" * 40 + "\n", None):
            with self.subTest(approved=approved), self.assertRaises(ValueError):
                self.run_case(approved=approved)

    def test_wrong_or_missing_approval_identity_fails_closed(self):
        for approval in ({}, [], {"repository": "https://github.com/other/repo",
                                "branch": "main", "approved_commit": SHA}):
            with self.subTest(approval=approval), self.assertRaises(ValueError):
                self.run_case(approval=approval)

    def test_resolved_commit_must_match_selected_pin(self):
        for manual in ("", SHA):
            with self.subTest(manual=manual), self.assertRaisesRegex(ValueError, "differs"):
                self.run_case(manual=manual, resolved_sha="d" * 40)

    def test_invalid_manual_input(self):
        with self.assertRaises(ValueError):
            self.run_case(manual="main; echo unsafe")

    def test_same_upstream_changed_recipe_builds(self):
        rows, _ = self.run_case(existing={"draft":False, "body": f"Upstream commit: {SHA}\nRecipe digest: {'c'*64}"})
        self.assertEqual(rows[0]["recipe_digest"], RECIPE)

    def test_old_release_without_recipe_does_not_skip(self):
        self.assertTrue(self.run_case(existing={"draft":False,"body":f"Upstream commit: {SHA}"})[0])

    def test_other_upstream_does_not_skip(self):
        self.assertTrue(self.run_case(existing={"draft":False,"body":f"Upstream commit: {'d'*40}\nRecipe digest: {RECIPE}"})[0])

    def test_match_on_second_page_skips(self):
        release = {"draft":False,"tag_name":"match","body":f"Upstream commit: {SHA}\nRecipe digest: {RECIPE}"}
        def api(path):
            if path == detect.UPSTREAM: return {"default_branch":"main"}
            if "/commits/" in path: return {"sha":SHA,"commit":{"committer":{"date":"2026-10-05T00:00:00Z"}}}
            return [release] if path.endswith("page=2") else [{"draft":True,"body":""}]*100
        with patch.dict(os.environ,{"GITHUB_REPOSITORY":"owner/repo","FORCE_REBUILD":"false","UPSTREAM_COMMIT":"","THINFLEX_TEST":"false"}), patch.object(detect,"api",api), patch.object(detect,"recipe_digest",return_value=RECIPE), patch.object(detect,"read_approval",return_value=("main",SHA)):
            self.assertEqual(detect.pending(),[])


if __name__ == "__main__":
    unittest.main()

