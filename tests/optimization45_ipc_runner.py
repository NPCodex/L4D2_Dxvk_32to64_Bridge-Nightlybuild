"""Record native correctness and same-toolchain timings; no game FPS inference."""
import argparse
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import uuid
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'results/optimization45/ipc'
parser = argparse.ArgumentParser()
parser.add_argument('--skip-transport', action='store_true')
parser.add_argument('--candidate-only', action='store_true')
parser.add_argument('--root', type=Path, default=OUT)
args = parser.parse_args()
OUT = args.root
rows = []


def record(row):
    rows.append(row)
    (OUT / 'receipts.json').write_text(json.dumps(rows, indent=2) + '\n')


def single(folder, exe, *arguments, timeout=120):
    result = subprocess.run([folder / exe, *arguments], capture_output=True, text=True, timeout=timeout)
    record(dict(variant=folder.name, executable=exe, arguments=list(map(str, arguments)), rc=result.returncode,
                stdout=result.stdout, stderr=result.stderr))
    result.check_returncode()
    print(result.stdout.strip(), flush=True)


def pair(variant, arch, batch, mode, iteration):
    folder = OUT / variant
    identity = str(uuid.uuid4())
    env = dict(os.environ, LDB_TEST_IPC_BATCH=str(batch))
    host = subprocess.Popen([folder / f'transport-host{arch}.exe', identity, mode], env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    client = None
    try:
        client = subprocess.Popen([folder / 'transport-client32.exe', identity, mode], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        ct = client.communicate(timeout=40)[0]
        ht = host.communicate(timeout=40)[0]
        row = dict(variant=variant, arch=arch, batch=batch, mode=mode, iteration=iteration,
                   client_rc=client.returncode, host_rc=host.returncode, client=ct, host=ht)
        record(row)
        if client.returncode or host.returncode:
            raise RuntimeError(f'Cross-process failure: {row}')
        return row
    finally:
        for child in (client, host):
            if child is not None and child.poll() is None:
                child.kill()
                child.wait()


if args.candidate_only:
    for arch in ('x86', 'x64'):
        single(OUT / 'candidate', f'pool-{arch}.exe')
        single(OUT / 'candidate', f'full-{arch}.exe')
    for batch in (1, 2, 4, 8, 16, 32):
        for arch in ('32', '64'):
            for mode in ('opt45-reply-delay', 'opt45-reply-abandon'):
                pair('candidate', arch, batch, mode, 0)
    print('OPTIMIZATION45_IPC_CONTRACT_PASS 2 pool models, 2 policies, 24 reply pairs', flush=True)
    sys.exit(0)

for variant in ('baseline', 'candidate'):
    folder = OUT / variant
    for arch in ('x86', 'x64'):
        single(folder, f'pool-{arch}.exe')
        single(folder, f'full-{arch}.exe')
        single(folder, f'core-{arch}.exe', '--host', folder / f'core-{"x64" if arch == "x86" else "x86"}.exe')

timings = []
pattern = re.compile(r'POOL_TIMING prefix_pages=(\d+) batch=(\d+) operations=(\d+) cycles=(\d+) wall_ns=(\d+)')
for iteration in range(7):
    for arch in ('x86', 'x64'):
        for variant in (('baseline', 'candidate') if iteration % 2 == 0 else ('candidate', 'baseline')):
            result = subprocess.run([OUT / variant / f'pool-{arch}.exe', '--benchmark'], capture_output=True,
                                    text=True, check=True, timeout=120)
            record(dict(variant=variant, arch=arch, iteration=iteration, benchmark=True, stdout=result.stdout))
            for prefix, batch, count, cycles, ns in pattern.findall(result.stdout):
                timings.append(dict(variant=variant, arch=arch, iteration=iteration, prefix_pages=int(prefix),
                                    batch=int(batch), cycles_per_operation=int(cycles) / int(count),
                                    ns_per_operation=int(ns) / int(count)))

summary = []
for arch in ('x86', 'x64'):
    for prefix in (0, 64, 16384):
        for batch in (1, 128):
            row = dict(arch=arch, prefix_pages=prefix, batch=batch)
            for metric in ('cycles_per_operation', 'ns_per_operation'):
                medians = {variant: statistics.median(t[metric] for t in timings if t['iteration'] > 0 and
                           t['arch'] == arch and t['prefix_pages'] == prefix and t['batch'] == batch and
                           t['variant'] == variant) for variant in ('baseline', 'candidate')}
                row[metric] = dict(**medians, change_percent=(medians['candidate'] / medians['baseline'] - 1) * 100)
            summary.append(row)
(OUT / 'pool-timings.json').write_text(json.dumps(dict(raw=timings, summary=summary), indent=2) + '\n')

if not args.skip_transport:
    modes = ('stream', 'mixed', 'threaded', 'upload-wrap', 'surface-boundaries', 'surface-small-generic',
             'surface-small-declared', 'concurrent-replies', 'handshake', 'vbib-mapped',
             'opt45-reply-delay', 'opt45-reply-abandon')
    for batch in (1, 2, 4, 8, 16, 32):
        for mode in ('abort', 'oversize', 'underfill', 'overrun'):
            result = subprocess.run([OUT / 'candidate/transport-client32.exe', mode],
                                    env=dict(os.environ, LDB_TEST_IPC_BATCH=str(batch)),
                                    capture_output=True, text=True, timeout=20)
            record(dict(variant='candidate', batch=batch, fault=mode, rc=result.returncode,
                        stdout=result.stdout, stderr=result.stderr))
            result.check_returncode()
        for arch in ('32', '64'):
            for mode in modes:
                for iteration in range(3):
                    pair('candidate', arch, batch, mode, iteration)
            print(f'IPC_CROSS_PROCESS_PASS batch={batch} host={arch} 30 pairs', flush=True)
    compare = []
    for iteration in range(7):
        for batch in (1, 8, 32):
            for arch in ('32', '64'):
                for mode in ('stream', 'mixed'):
                    for variant in (('baseline', 'candidate') if iteration % 2 == 0 else ('candidate', 'baseline')):
                        compare.append(pair(variant, arch, batch, mode, iteration))
    (OUT / 'transport-comparison.json').write_text(json.dumps(compare, indent=2) + '\n')
print('OPTIMIZATION45_IPC_PASS', flush=True)
