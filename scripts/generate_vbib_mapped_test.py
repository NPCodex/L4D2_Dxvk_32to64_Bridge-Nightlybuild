"""Compile unchanged old/new production LockableBuffer with the same adapters."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / '.deps/dxvk-remix/bridge/src/client'
out = ROOT / 'work/vbib-mapped-native'
out.mkdir(parents=True, exist_ok=True)
# Preparation snapshots the verified full-v2 baseline before the overlay; do not reconstruct
# the old template by deleting new code or changing implementation checks.
old = ROOT / 'work/vbib-mapped-baseline/lockable_buffer.h'
for label, header in [('old', old), ('new', source / 'lockable_buffer.h')]:
    text = header.read_text()
    marker = 'template <typename T>'
    if text.count(marker) != 1:
        raise RuntimeError('Unexpected production template layout')
    template = marker + text.split(marker, 1)[1]
    (out / (label + '.cpp')).write_text('#include "vbib_mapped_support.h"\n' + template
                                      + '\n#include "vbib_mapped_cases.cpp"\n')
manifest = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in
            (source / 'lockable_buffer.h', source / 'buffer_shadow.h', source / 'buffer_mapped_shadow.h')}
manifest['old_template_sha256'] = hashlib.sha256(old.read_bytes()).hexdigest()
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
