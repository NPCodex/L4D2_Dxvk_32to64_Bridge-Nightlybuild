"""Extract production startup/IPC admission and test same-version pin separation.

This uses native harnesses, not the game or deployed Host. Legacy represents the
previous patch/version-only identity algorithm with otherwise identical inputs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from build_identity import protocol_digest, runtime_build_id, upstream_commit
from generate_buffer_contract_test import method

PREVIOUS_UPSTREAM = "029602f83a2fdec8f4d4419e1967bf231fb8f0b5"


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8", newline="\n")


def generate(source, output):
    output.mkdir(parents=True, exist_ok=True)
    commit = upstream_commit(subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip())
    version = (ROOT / "VERSION").read_text().strip()
    patch = (ROOT / "patches/l4d2-bridge.patch").read_bytes()
    normalized = patch.replace(b"\r\n", b"\n")
    legacy = hashlib.sha256(b"l4d2-nightly-ipc-v2\0" + version.encode() + b"\0"
                            + b"patches/l4d2-bridge.patch\0" + normalized).hexdigest()
    identities = [
        {"name": "selected", "upstream": commit, "protocol": protocol_digest(patch, version, commit),
         "startup": runtime_build_id(patch, version, commit)},
        {"name": "legacy", "upstream": PREVIOUS_UPSTREAM, "protocol": legacy,
         "startup": f"l4d2-{version}+{hashlib.sha256(patch).hexdigest()[:16]}"},
        {"name": "previous-upstream-bound", "upstream": PREVIOUS_UPSTREAM,
         "protocol": protocol_digest(patch, version, PREVIOUS_UPSTREAM),
         "startup": runtime_build_id(patch, version, PREVIOUS_UPSTREAM)},
    ]
    util = source / "bridge/src/util/ipc_v2"
    paths = [util / name for name in ("transport.h", "platform.h", "ipc_v2_build.h")]
    for path in paths:
        shutil.copy2(path, output / path.name)
    stamp = (util / "ipc_v2_build.h").read_text()
    for i in range(4):
        if f"#define LDB_IPC_V2_BUILD{i} 0x{identities[0]['protocol'][i*8:(i+1)*8]}u" not in stamp:
            raise ValueError("Prepared identity does not match actual selected upstream")
    main_path = source / "bridge/src/server/main.cpp"
    main = main_path.read_text(encoding="utf-8")
    start, end = method(main, "if (wcscmp(argList[1], BRIDGE_VERSION_W) != 0)")
    if (main.count("  initModuleBridge();") != 1 or main.count("  initDeviceBridge();") != 1
            or not end < main.index("  initModuleBridge();") < main.index("  initDeviceBridge();")):
        raise ValueError("Startup guard must precede both channel initializations")
    if "  return 1;" not in main[start:end]:
        raise ValueError("Startup mismatch no longer exits before initialization")
    (output / "startup_guard.h").write_text(
        "int startupGuard(const wchar_t* own, const wchar_t* peer) {\n"
        "  const wchar_t* argList[] = { L\"fixture-guid\", peer };\n"
        "#define BRIDGE_VERSION_W own\n#define BRIDGE_VERSION \"fixture\"\n"
        + main[start:end] + "\n#undef BRIDGE_VERSION_W\n#undef BRIDGE_VERSION\nreturn 0;\n}\n",
        encoding="utf-8", newline="\n")
    rows = []
    for item in identities:
        words = ", ".join("0x" + item["protocol"][i * 8:(i + 1) * 8] + "u" for i in range(4))
        rows.append('  {L"' + item["startup"] + '", {' + words + "}}")
    (output / "identities.h").write_text(
        "struct Identity { const wchar_t* startup; uint32_t words[4]; };\n"
        "constexpr Identity kIdentities[] {\n" + ",\n".join(rows) + "\n};\n"
        "constexpr unsigned kIdentityCount = sizeof(kIdentities) / sizeof(kIdentities[0]);\n",
        encoding="utf-8", newline="\n")
    write_json(output / "source-manifest.json", {
        "version": version, "selected_upstream": commit, "identities": identities,
        "source_sha256": {p.relative_to(source).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [*paths, main_path]},
        "harness_sha256": {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                           for p in [Path(__file__), ROOT / "tests/build_identity_contract.cpp",
                                     ROOT / "scripts/test_build_identity.ps1", ROOT / "scripts/build_identity.py"]},
        "adapters": ["startup argument array/logger/free callbacks", "synthetic queued payload sentinel",
                     "explicit old/new identity Config values; production Channel unchanged"],
        "scope": "native admission before payload dispatch; no game or deployed Client/Host execution",
    })


def run(output):
    manifest = json.loads((output / "source-manifest.json").read_text())
    identities = manifest["identities"]
    results = []

    def record(row):
        results.append(row)
        write_json(output / "results.json", results)
        if not row["passed"]:
            raise RuntimeError("Native identity regression failed: " + repr(row))

    for arch in ("x86", "x64"):
        exe = output / arch / "identity.exe"
        for self_id in range(3):
            for peer_id in range(3):
                expected = identities[self_id]["startup"] == identities[peer_id]["startup"]
                result = subprocess.run([exe, "startup", str(self_id), str(peer_id), str(int(expected))],
                                        capture_output=True, text=True, timeout=20)
                record({"test": "startup", "architecture": arch, "self": self_id, "peer": peer_id,
                        "expected_accept": expected, "exit_code": result.returncode,
                        "output": result.stdout + result.stderr,
                        "passed": result.returncode == 0 and "IDENTITY_STARTUP_PASS" in result.stdout})
    pairs = [(0, 0), (1, 1), (2, 2), (0, 1), (1, 0), (0, 2), (2, 0)]
    for owner_arch, peer_arch in (("x86", "x86"), ("x86", "x64"), ("x64", "x86")):
        for kind in (1, 2):
            for direction in (0, 1):
                for own, peer in pairs:
                    expected = identities[own]["protocol"][:32] == identities[peer]["protocol"][:32]
                    name = "identity-" + str(uuid.uuid4())
                    owner = subprocess.Popen([output / owner_arch / "identity.exe", "owner", name, str(own),
                                              str(kind), str(direction), "1"], stdin=subprocess.PIPE,
                                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                    try:
                        ready = owner.stdout.readline().strip()
                        if ready != "IDENTITY_OWNER_READY":
                            raise RuntimeError("Owner failed before peer: " + ready + owner.stderr.read())
                        result = subprocess.run([output / peer_arch / "identity.exe", "peer", name, str(peer),
                                                 str(kind), str(direction), str(int(expected))],
                                                capture_output=True, text=True, timeout=20)
                        owner_output, owner_error = owner.communicate("\n", timeout=20)
                        record({"test": "channel", "owner_arch": owner_arch, "peer_arch": peer_arch,
                                "owner": own, "peer": peer, "kind": kind, "direction": direction,
                                "expected_accept": expected, "exit_code": result.returncode,
                                "owner_exit_code": owner.returncode, "output": result.stdout + result.stderr,
                                "owner_output": owner_output + owner_error,
                                "passed": result.returncode == owner.returncode == 0
                                and "IDENTITY_CHANNEL_PASS" in result.stdout})
                    finally:
                        if owner.poll() is None:
                            owner.kill()
                            owner.communicate(timeout=5)
    print(f"BUILD_IDENTITY_NATIVE_PASS cases={len(results)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / ".deps/dxvk-remix")
    parser.add_argument("--output", type=Path, default=ROOT / ".deps/build-identity-contract")
    parser.add_argument("--run", action="store_true")
    args = parser.parse_args()
    if args.run:
        run(args.output.resolve())
    else:
        generate(args.source.resolve(), args.output.resolve())
