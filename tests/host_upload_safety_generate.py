# SPDX-License-Identifier: MIT
# Copyright (c) 2026 yeyunyyds.
"""Extract entire production upload dispatch cases; adapt backend types only."""
import argparse
from pathlib import Path
import re
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_buffer_contract_test import method

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
src = a.source / 'bridge/src'
main = (src / 'server/main.cpp').read_text(encoding='utf-8')
cases = []
for name in ['IDirect3DVertexBuffer9_Unlock', 'IDirect3DIndexBuffer9_Unlock',
             'IDirect3DSurface9_UnlockRect', 'IDirect3DVolume9_UnlockBox',
             'IDirect3DVolumeTexture9_UnlockBox']:
    marker = '      case ' + name + ':'
    start = main.index(marker)
    end = main.index('      case ', start + len(marker))
    text = main[start:end]
    for old, new in [('IDirect3DVertexBuffer9', 'TestBuffer'), ('IDirect3DIndexBuffer9', 'TestBuffer'),
                     ('IDirect3DSurface9', 'TestSurface'), ('IDirect3DVolumeTexture9', 'TestVolumeTexture')]:
        text = text.replace('(' + old + '*)', '(' + new + '*)')
    cases.append(text)
a.output.mkdir(parents=True, exist_ok=True)
(a.output / 'actual_upload_dispatch.h').write_text(
    'static void dispatchUpload() { switch (rpcHeader.command) {\n' + ''.join(cases) + '} }\n', encoding='utf-8')
formats = (src / 'util/util_texture_and_volume.h').read_text(encoding='utf-8')
out = '\n'.join(re.findall(r'^#define D3DFMT_.*$', formats, re.M)) + '\n'
out += 'namespace bridge_util { namespace caps { constexpr uint32_t MinSurfacePitch = sizeof(size_t); }\n'
for marker in ['static uint32_t getBlockSize(', 'static uint32_t getBytesFromFormat(',
               'static inline uint32_t calcStride(', 'static inline uint32_t calcRowSize(',
               'static inline size_t calcImageByteOffset(']:
    start, end = method(formats, marker)
    out += formats[start:end] + '\n'
out += '}\n' + formats[formats.index('#define FOR_EACH_RECT_ROW'):]
(a.output / 'actual_upload_formats.h').write_text(out, encoding='utf-8')
print('Extracted all five production upload handlers')
