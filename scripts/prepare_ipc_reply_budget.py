# SPDX-License-Identifier: MIT
"""Extract production IPC and one selectively reverted reply-budget control."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from generate_ipc_full_test import generate
from native_source import _block_end

ROOT = Path(__file__).resolve().parents[1]


def prepare(source, output):
    candidate = output / "candidate"
    negative = output / "without-uid-budget-preservation"
    generate(source, candidate)
    shutil.copy2(ROOT / "tests/ipc_reply_budget_cases.h", candidate / "ipc_reply_budget_cases.h")
    dispatch = candidate / "full_bridge_cases.cpp"
    text = dispatch.read_text()
    needle = "    else {transfer<Device>();transfer<Module>();}"
    if text.count(needle) != 1 or text.count("int wmain(") != 1:
        raise RuntimeError("Production transport fixture dispatch changed")
    text = text.replace("int wmain(", '#include "ipc_reply_budget_cases.h"\nint wmain(', 1)
    text = text.replace(needle, '    else if(std::wstring(argv[2]).find(L"reply-")==0) {replyBudgetCase<Device>(argv[2]);}\n' + needle, 1)
    dispatch.write_text(text)
    shutil.copytree(candidate, negative, dirs_exist_ok=True)
    transport = negative / "ipc_transport.cpp"
    text = transport.read_text()
    needle = "if (verifyUID && hardTimeoutMS && header.command == command && !uidVerified) {"
    if text.count(needle) != 1:
        raise RuntimeError("Expected one existing UID retry-budget preservation branch")
    start = text.index(needle)
    end = _block_end(text, text.index("{", start))
    removed = text[start:end]
    if "Sleep(1)" not in removed or "--attemptNum" not in removed:
        raise RuntimeError("Reply-budget control does not target the reviewed branch")
    transport.write_text(text[:start] + text[end:])
    receipt = {
        "source": str(source),
        "source_manifest": "candidate/full-source-manifest.json",
        "negative_control": "Only the other-UID retry-budget preservation branch is removed from the extracted production function; not a frozen whole-version comparison",
        "removed_branch_sha256": hashlib.sha256(removed.encode()).hexdigest(),
        "candidate_transport_sha256": hashlib.sha256((candidate / "ipc_transport.cpp").read_bytes()).hexdigest(),
        "negative_transport_sha256": hashlib.sha256(transport.read_bytes()).hexdigest(),
    }
    (output / "preparation.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source.resolve(), args.output.resolve())
