# SPDX-License-Identifier: MIT
# Copyright (c) 2026 yeyunyyds.
"""Cross-process reply-budget coverage adapted from upstream PR #11."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import uuid


def validate_pair(row):
    """Accept only the intended early retry exhaustion as a negative result."""
    output = row["client"]
    timings = re.findall(
        r"^IPC_REPLY_BUDGET_TIMING retries=(\d+) owner_pause_ms=(\d+) elapsed_ms=(\d+) result=(\d+)$",
        output, re.MULTILINE)
    if len(timings) != 1:
        raise RuntimeError("Missing or ambiguous reply timing receipt: " + str(row))
    retries, pause, elapsed, result = map(int, timings[0])
    default = row["mode"] in ("reply-default-budget", "reply-default-budget-long")
    expected_pause = 6500 if row["mode"] == "reply-default-budget-long" else (3300 if default else 120)
    if retries != (300 if default else 2) or pause != expected_pause:
        raise RuntimeError("Wrong reply test parameters: " + str(row))
    if row["host_rc"] or "IPC_FULL_BRIDGE_PASS" not in row["server"]:
        raise RuntimeError("Host failed: " + str(row))
    passed = row["client_rc"] == 0 and "IPC_REPLY_BUDGET_PASS" in output and "IPC_FULL_BRIDGE_PASS" in output
    # A crash, generic fixture exception, peer failure or the 10s hard deadline
    # must never satisfy the selectively reverted retry-budget control.
    known_failure = (
        row["client_rc"] == 2
        and output.count("C32_BRIDGE_FAIL another UID consumed our retry budget") == 1
        and "IPC_FULL_BRIDGE_EXCEPTION" not in output
        and result == 2
        and elapsed < pause
    )
    if row["expected"] == "pass":
        if not passed:
            raise RuntimeError("Reply regression failed: " + str(row))
    elif row["expected"] == "fail":
        if not known_failure:
            raise RuntimeError("Reverted reply defect was not detected: " + str(row))
    elif row["expected"] == "observation":
        if not passed and not known_failure:
            raise RuntimeError("3300ms observation failed for an unrelated reason: " + str(row))
    else:
        raise RuntimeError("Unknown reply expectation: " + str(row))


def run(output):
    rows = []
    # A failed rerun cannot leave an earlier successful summary as its result.
    (output / "summary.json").unlink(missing_ok=True)
    (output / "results.json").write_text("[]\n")

    def pair(variant, host_arch, mode, expected):
        folder = output / variant
        identity = str(uuid.uuid4())
        env = dict(os.environ, LDB_TEST_IPC_BATCH="8")
        # The negative control changes only the Client waiter. Reuse the exact
        # candidate Host, keeping publication, protocol and ownership identical.
        host = subprocess.Popen([str(output / "candidate" / f"transport-host{host_arch}.exe"), identity, mode],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
        client = None
        try:
            client = subprocess.Popen([str(folder / "transport-client32.exe"), identity, mode],
                                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
            cout = client.communicate(timeout=20)[0]
            hout = host.communicate(timeout=20)[0]
            row = dict(variant=variant, host=host_arch, mode=mode, expected=expected,
                       client_rc=client.returncode, host_rc=host.returncode, client=cout, server=hout)
            rows.append(row)
            (output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            validate_pair(row)
            print("REPLY_BUDGET_PAIR_PASS", variant, host_arch, mode, expected, flush=True)
        finally:
            for child in (client, host):
                if child is not None and child.poll() is None:
                    child.kill()
                    child.wait()

    for arch in ("32", "64"):
        for mode in ("reply-short-budget", "reply-default-budget", "reply-default-budget-long", "reply-abandoned", "reply-cancel"):
            pair("candidate", arch, mode, "pass")
    # Preserve the original observation: 3300ms may fit the old Windows budget.
    pair("without-uid-budget-preservation", "64", "reply-default-budget", "observation")
    for mode in ("reply-default-budget-long", "reply-short-budget"):
        pair("without-uid-budget-preservation", "64", mode, "fail")
    summary = dict(passed=True, positive_pairs=10, expected_negative_pairs=2, observations=1,
                   source_manifest="candidate/full-source-manifest.json", game_performance_claim=False)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("IPC_REPLY_BUDGET_SUITE_PASS 10 positives, 2 targeted negatives, 1 timer observation", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output.resolve())
