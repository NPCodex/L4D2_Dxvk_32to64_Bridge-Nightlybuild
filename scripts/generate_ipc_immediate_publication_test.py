# SPDX-License-Identifier: MIT
"""Extract prepared production IPC and inject deterministic publication failures.

The input must already contain the immediate-publication deadline optimization.
Only the negative control removes its cold failure fallback. No production source
is patched by this generator, and all injection adapters remain in the fixture.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from generate_ipc_full_test import generate as generate_full
from generate_ipc_transport_test import FILES

ROOT = Path(__file__).resolve().parents[1]
FALLBACK = """      if (!flushed && firstInBatch) {
        m_deadline.store(ldb_ipc_v2::milliseconds() + 1, std::memory_order_release);
      }
"""


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(path, old, new):
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise ValueError(f"Expected one injection boundary in {path}, found {count}")
    path.write_text(text.replace(old, new), encoding="utf-8")


def source_manifest(source):
    util = source / "bridge/src/util"
    paths = {util / name for name in FILES}
    paths.update(util / name for name in (
        "util_bridgecommand.h", "util_bridgecommand.cpp", "ipc_c32_message_queue.h"))
    for directory in ("ipc_full", "ipc_v2"):
        paths.update((util / directory).rglob("*"))
    client = source / "bridge/src/client"
    if (client / "buffer_mapped_shadow.h").exists():
        paths.update(client / name for name in (
            "buffer_shadow.h", "buffer_shadow_arena.h", "buffer_mapped_shadow.h",
            "memory_diagnostics.h", "lockable_buffer.h"))
    return {p.relative_to(source).as_posix(): sha(p)
            for p in sorted(paths) if p.is_file()}


def generate(source, output):
    source, output = source.resolve(), output.resolve()
    if source == output or source in output.parents or output in source.parents:
        raise ValueError("Source and fixture output must be separate, non-nested trees")
    queue = source / "bridge/src/util/ipc_full/message_queue.h"
    text = queue.read_text(encoding="utf-8")
    if text.count(FALLBACK) != 1:
        raise ValueError("Prepared source must contain the reviewed cold failure fallback")
    manifest = source_manifest(source)
    production = output / "production"
    generate_full(source, production)
    replace_once(production / "ipc_full/message_queue.h", "    m_sentBytes += m_written;",
                 "    immediate_publication_test::invoke(immediate_publication_test::afterEncoded);\n"
                 "    m_sentBytes += m_written;")
    marker = "    // SC publication pairs with SC armed/recheck. One exchange per batch."
    replace_once(production / "ipc_v2/transport.h", marker,
                 "    immediate_publication_test::invoke(immediate_publication_test::beforePublish);\n" + marker)
    platform = production / "ipc_v2/platform.h"
    replace_once(platform, "return SetEvent(m_handle) != FALSE;",
                 "return immediate_publication_test::setEvent(m_handle) != FALSE;")
    platform.write_text('#include "immediate_publication_hooks.h"\n' +
                        platform.read_text(encoding="utf-8"), encoding="utf-8")
    replace_once(production / "ipc_transport.cpp", '#include "full_bridge_cases.cpp"',
                 '#include "immediate_publication_cases.cpp"')
    for name in ("immediate_publication_hooks.h", "immediate_publication_cases.cpp"):
        shutil.copy2(ROOT / "tests/ipc-v2" / name, production / name)
    negative = output / "negative-no-fallback"
    # Copy only the generated sources, never stale binaries from an earlier run.
    shutil.copytree(production, negative, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("x86", "x64"))
    replace_once(negative / "ipc_full/message_queue.h", FALLBACK, "")
    if source_manifest(source) != manifest:
        raise RuntimeError("Production source changed during extraction")
    receipt = {
        "source": str(source), "source_sha256": manifest,
        "source_mutated": False,
        "positive_cases_per_architecture": 26,
        "negative_control": "remove only immediate-publication cold failure deadline fallback",
        "injections": ["one-shot callback after successful command encoding",
                       "one-shot callback after consumer-close check, before publication",
                       "one-shot FALSE/ERROR_INVALID_HANDLE substitution at production SetEvent call site"],
        "limitations": ["Notification failure is deterministic call-site injection, not OS handle corruption",
                        "Extracted logger/options/peer callbacks and application dispatch use existing IPC adapters"],
        "fixture_sha256": {
            variant: {p.relative_to(output / variant).as_posix(): sha(p)
                      for p in sorted((output / variant).rglob("*"))
                      if p.is_file() and not {"x86", "x64"}.intersection(p.relative_to(output / variant).parts)}
            for variant in ("production", "negative-no-fallback")},
    }
    (output / "source-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"IMMEDIATE_PUBLICATION_EXTRACT_PASS source_files={len(manifest)} source={source}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".deps/dxvk-remix")
    parser.add_argument("--output", type=Path, default=ROOT / "work/ipc-full-native/immediate-publication")
    args = parser.parse_args()
    generate(args.source, args.output)
