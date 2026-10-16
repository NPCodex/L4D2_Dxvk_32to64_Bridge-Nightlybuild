"""Offline integrity and failure-path checks for the fixed source fetcher."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/fetch_reshade_keyboard_source.py"
SPEC = importlib.util.spec_from_file_location("pinned_reshade_fetch", SCRIPT)
fetcher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fetcher)


class FixedReShadeSourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name)
        self.payload = {p: ("offline verified bytes " + p).encode() for p in fetcher.PINNED_FILES}
        pins = {p: hashlib.sha256(b).hexdigest() for p, b in self.payload.items()}
        self.patch_pins = patch.object(fetcher, "PINNED_FILES", pins)
        self.patch_pins.start()
        self.addCleanup(self.patch_pins.stop)

    def test_exact_official_commit_and_no_url_ref_cli_override(self):
        self.assertEqual(fetcher.UPSTREAM_COMMIT, "18deaa52de0c425a78b329e9cb3c497281cd00ec")
        self.assertEqual(fetcher.SOURCE_BASE, "https://raw.githubusercontent.com/crosire/reshade/18deaa52de0c425a78b329e9cb3c497281cd00ec/")
        for option in ("--url", "--version", "--commit", "--repository", "--sha256"):
            result = subprocess.run([sys.executable, str(SCRIPT), option, "unapproved"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, option)

    def test_verified_cache_never_contacts_network(self):
        for p, b in self.payload.items():
            target = self.output / p
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b)
        with patch.object(fetcher, "_download", side_effect=AssertionError("network must not be used")):
            result = fetcher.fetch(self.output)
        self.assertTrue(all(row["cache_hit"] for row in result["files"]))
        self.assertEqual(json.loads((self.output / "source-receipt.json").read_text())["commit"], fetcher.UPSTREAM_COMMIT)

    def test_downloads_only_fixed_paths_and_records_actual_bytes(self):
        with patch.object(fetcher, "_download", side_effect=self.payload.__getitem__) as download:
            result = fetcher.fetch(self.output)
        self.assertEqual([x.args[0] for x in download.call_args_list], list(self.payload))
        for row in result["files"]:
            self.assertEqual((self.output / row["path"]).read_bytes(), self.payload[row["path"]])
            self.assertEqual(row["bytes"], len(self.payload[row["path"]]))
            self.assertEqual(row["url"], fetcher.SOURCE_BASE + row["path"])

    def test_hash_failure_does_not_replace_any_existing_source_or_receipt(self):
        first, second, third = self.payload
        target = self.output / first
        target.parent.mkdir(parents=True)
        target.write_bytes(b"retain original failed cache")
        receipt = self.output / "source-receipt.json"
        receipt.write_bytes(b"retain prior receipt")
        responses = dict(self.payload)
        responses[third] = b"corrupt download"
        with patch.object(fetcher, "_download", side_effect=responses.__getitem__):
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                fetcher.fetch(self.output)
        self.assertEqual(target.read_bytes(), b"retain original failed cache")
        self.assertEqual(receipt.read_bytes(), b"retain prior receipt")
        self.assertFalse((self.output / second).exists())

    def test_http_failure_leaves_no_success_receipt(self):
        with patch.object(fetcher, "_download", side_effect=HTTPError("fixed", 503, "offline", {}, None)):
            with self.assertRaises(HTTPError):
                fetcher.fetch(self.output)
        self.assertFalse((self.output / "source-receipt.json").exists())

    def test_corrupt_cache_is_repaired_from_verified_bytes_only(self):
        for p, b in self.payload.items():
            target = self.output / p
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b)
        damaged = next(iter(self.payload))
        (self.output / damaged).write_bytes(b"damaged")
        with patch.object(fetcher, "_download", side_effect=self.payload.__getitem__) as download:
            result = fetcher.fetch(self.output)
        download.assert_called_once_with(damaged)
        self.assertEqual((self.output / damaged).read_bytes(), self.payload[damaged])
        self.assertEqual(sum(not row["cache_hit"] for row in result["files"]), 1)

    def test_redirect_and_unapproved_path_are_rejected(self):
        request = fetcher.Request(fetcher.SOURCE_BASE + "source/input.cpp")
        with self.assertRaisesRegex(HTTPError, "must not redirect") as caught:
            fetcher.NoRedirects().redirect_request(request, None, 302, "Found", {}, "https://example.com/replacement")
        caught.exception.close()
        with patch.object(fetcher, "build_opener", side_effect=AssertionError("must reject before network")):
            with self.assertRaisesRegex(ValueError, "Unapproved"):
                fetcher._download("../unapproved.cpp")

    def test_response_size_limit(self):
        class Opener:
            def open(self, request, timeout):
                return io.BytesIO(b"x" * (fetcher.MAX_SOURCE_BYTES + 1))
        with patch.object(fetcher, "build_opener", return_value=Opener()):
            with self.assertRaisesRegex(ValueError, "size bound"):
                fetcher._download(next(iter(self.payload)))


if __name__ == "__main__":
    unittest.main()
