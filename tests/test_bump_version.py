"""Explicit semver minor releases must not bypass the commit version gate."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
from bump_version import version_for_commit


class VersionGate(unittest.TestCase):
    def test_minor_short_and_full_forms_are_preserved(self):
        for staged in ("1.3", "1.3.0"):
            self.assertEqual(version_for_commit("1.2.3", staged), staged)

    def test_patch_bump_and_explicit_patch_remain_unchanged(self):
        self.assertEqual(version_for_commit("1.2.3", "1.2.3"), "1.2.4")
        self.assertEqual(version_for_commit("1.2.3", "1.2.4"), "1.2.4")
        self.assertEqual(version_for_commit("1.3.0", "1.3.0"), "1.3.1")

    def test_other_version_jumps_are_rejected(self):
        for staged in ("1.3.1", "1.4.0", "2.0.0", "1.2.2", "v1.3.0", "1.3.00"):
            with self.subTest(staged=staged), self.assertRaises(ValueError):
                version_for_commit("1.2.3", staged)
