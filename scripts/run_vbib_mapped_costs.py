"""Preserve alternating old/off/zero/cache native process samples and paired costs."""
from pathlib import Path
import hashlib
import json
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'work/vbib-mapped-native'
samples = []
variants = ['old', 'off', 'zero', 'cache']
for iteration in range(6):
    order = variants[iteration % 4:] + variants[:iteration % 4]
    if iteration % 2:
        order.reverse()
    for variant in order:
        exe = out / ('old-x86.exe' if variant == 'old' else 'new-x86.exe')
        run = subprocess.run([str(exe), variant], capture_output=True, text=True, timeout=180)
        (out / f'cost-{iteration}-{variant}.log').write_text(run.stdout + run.stderr)
        samples.append(dict(iteration=iteration, variant=variant, rc=run.returncode,
                            rows=[json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]))
        (out / 'cost-raw.json').write_text(json.dumps(samples, indent=2) + '\n')
        if run.returncode or len(samples[-1]['rows']) != 16:
            raise RuntimeError(f'Native cost process failed: {iteration}/{variant}')
summary = []
for size in [65536, 262144, 1048576, 8388608]:
    for mode in ['static-hot', 'dynamic-hot', 'create-upload-destroy', 'loaded-idle']:
        group = dict(bytes=size, mode=mode, variants={})
        for variant in variants:
            rows = [row for sample in samples if sample['iteration'] and sample['variant'] == variant
                    for row in sample['rows'] if row['bytes'] == size and row['mode'] == mode]
            values = dict(ns_per_operation=statistics.median(r['ns'] / r['count'] for r in rows),
                          cycles_per_operation=statistics.median(r['cycles'] / r['count'] for r in rows),
                          va_used=statistics.median(r['va_free_before'] - r['va_free_after'] for r in rows),
                          largest_after=statistics.median(r['largest_after'] for r in rows),
                          mapped=statistics.median(r['mapped'] for r in rows), backing=statistics.median(r['backing'] for r in rows))
            group['variants'][variant] = values
        summary.append(group)
receipt = dict(rounds=5, warmup_rounds=1, baseline_commit='759c8e8d2816ba21d298e469699d2ea869ccee9d',
    compiler='MSVC 14.29 /O2 /MT /DNDEBUG /LARGEADDRESSAWARE', timing='outer batch only',
    limitations=['Actual production LockableBuffer and storage, deterministic COM/IPC upload capture',
                 'No real IPC, Host, GPU or FPS measurement', 'Loaded-idle includes adapters and heap retention; not owner-exact VA'],
    binaries={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.exe')}, results=summary)
(out / 'cost-summary.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('VBIB_MAPPED_NATIVE_COSTS_PASS 24 processes; 1 warmup + 5 measured rounds')
