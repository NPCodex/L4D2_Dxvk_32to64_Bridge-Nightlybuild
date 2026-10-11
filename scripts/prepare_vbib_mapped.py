"""Apply static VB/IB section views after the complete IPC v2 preparation."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def prepare(source):
    receipt_path = ROOT / 'work/ipc-full-build.json'
    receipt = json.loads(receipt_path.read_text())
    if receipt['build_id'] != 'ldb-ipc-full-dev+9f30cbf7ddabdb95':
        raise RuntimeError('Expected verified full IPC v2 baseline before mapped overlay')
    baseline = ROOT / 'work/vbib-mapped-baseline'
    baseline.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / 'bridge/src/client/lockable_buffer.h', baseline / 'lockable_buffer.h')
    patch = ROOT / 'patches/vbib-mapped.patch'
    subprocess.run(['git', 'apply', '--check', str(patch)], cwd=source, check=True)
    subprocess.run(['git', 'apply', str(patch)], cwd=source, check=True)
    helper = ROOT / 'experiments/vbib-mapped/buffer_mapped_shadow.h'
    shutil.copy2(helper, source / 'bridge/src/client/buffer_mapped_shadow.h')
    receipt['ipc_foundation_build'] = receipt['build_id']
    digest = hashlib.sha256(receipt['protocol_source_sha256'].encode())
    for path in (patch, helper):
        digest.update(path.relative_to(ROOT).as_posix().encode() + b'\0'
                      + path.read_bytes().replace(b'\r\n', b'\n') + b'\0')
    sha = digest.hexdigest()
    stamp = source / 'bridge/src/util/ipc_v2/ipc_v2_build.h'
    stamp.write_text(''.join(f'#define LDB_IPC_V2_BUILD{i} 0x{sha[i*8:i*8+8]}u\n' for i in range(4)))
    receipt.update(build_id='ldb-vbib-map-dev+' + sha[:16], protocol_source_sha256=sha,
                   variant='vbib-map', install_document='docs/VBIB-MAPPED-DEVELOPMENT.md',
                   mapped_static_vbib=True, mapped_minimum_bytes=262144,
                   mapped_idle_cache_bytes=33554432, mapped_default_enabled=True)
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    print(receipt['build_id'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / '.deps/dxvk-remix')
    prepare(parser.parse_args().source.resolve())
