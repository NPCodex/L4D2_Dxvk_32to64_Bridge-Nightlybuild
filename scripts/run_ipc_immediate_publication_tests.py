# SPDX-License-Identifier: MIT
"""Run bounded native IPC invariants and verify the intended negative control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

from generate_ipc_immediate_publication_test import source_manifest

ROOT = Path(__file__).resolve().parents[1]


def run(output):
    receipt = json.loads((output / "source-receipt.json").read_text(encoding="utf-8"))
    results = {"source": receipt["source"], "cases": [], "passed": False}

    def save():
        (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    try:
        for arch in ("x86", "x64"):
            for variant in ("production", "negative-no-fallback"):
                exe = output / variant / arch / "immediate-publication.exe"
                negative = variant == "negative-no-fallback"
                args = [str(exe)] + (["--negative"] if negative else [])
                start = time.monotonic()
                p = subprocess.run(args, capture_output=True, text=True, timeout=90)
                text = p.stdout + p.stderr
                if negative:
                    passed = (p.returncode == 2 and
                              "IMMEDIATE_PUBLICATION_FAIL failed immediate publication lost pending marker" in text)
                else:
                    bits = 32 if arch == "x86" else 64
                    passed = (p.returncode == 0 and
                              f"IMMEDIATE_PUBLICATION_PASS cases=26 bits={bits}" in text and
                              text.count("RAW_FAILURE_PASS ") == 18 and
                              text.count("WRAPPER_FAILURE_PASS ") == 6 and
                              text.count("CLOSE_TIMELINE_PASS ") == 2)
                item = {"variant": variant, "architecture": arch, "args": args,
                        "returncode": p.returncode, "expected_negative_control": negative,
                        "passed": passed, "seconds": time.monotonic() - start,
                        "stdout": p.stdout, "stderr": p.stderr,
                        "binary_sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
                results["cases"].append(item)
                save()
                (output / f"{variant}-{arch}.txt").write_text(text, encoding="utf-8")
                if not passed:
                    raise RuntimeError(f"Unexpected {variant}/{arch} outcome:\n{text}")
                print(f"IMMEDIATE_PUBLICATION_EXPECTED_PASS variant={variant} architecture={arch}", flush=True)
        exe = output / "production/x86/immediate-publication.exe"
        start = time.monotonic()
        p = subprocess.run([str(exe), "--concurrency"], capture_output=True, text=True, timeout=90)
        text = p.stdout + p.stderr
        passed = p.returncode == 0 and "PRODUCTION_CONCURRENCY_PASS writers=4 commands=2048 " in text
        results["cases"].append({"variant": "client-concurrency", "architecture": "x86",
                                 "returncode": p.returncode, "passed": passed,
                                 "seconds": time.monotonic() - start,
                                 "stdout": p.stdout, "stderr": p.stderr,
                                 "binary_sha256": hashlib.sha256(exe.read_bytes()).hexdigest()})
        (output / "client-concurrency-x86.txt").write_text(text, encoding="utf-8")
        if not passed:
            raise RuntimeError(f"Client concurrency failed:\n{text}")
        current = source_manifest(Path(receipt["source"]))
        results["source_unchanged"] = current == receipt["source_sha256"]
        if not results["source_unchanged"]:
            raise RuntimeError("Production source changed during native tests")
        results["passed"] = True
        print("IMMEDIATE_PUBLICATION_ALL_PASS positive_cases=52 negative_controls=2 client_commands=2048", flush=True)
    except (Exception, KeyboardInterrupt) as e:
        results["error"] = str(e)
        raise
    finally:
        save()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "work/ipc-full-native/immediate-publication")
    run(parser.parse_args().output.resolve())
