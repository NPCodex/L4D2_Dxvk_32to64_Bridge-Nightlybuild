# SPDX-License-Identifier: MIT
# Copyright (c) 2026 yeyunyyds.
"""Frozen production negative controls; only scheduling hooks modify test copies."""
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
frozen=ROOT/'.deps/v130-frozen'
source=ROOT/'.deps/ipc-safety-baseline'
subprocess.run(['python',str(frozen/'scripts/prepare_bridge.py'),'--source',str(source)],check=True)
out=ROOT/'work/ipc-safety-baseline'
subprocess.run(['python',str(ROOT/'scripts/generate_ipc_full_test.py'),'--source',str(source),'--output',str(out)],check=True)
# Add observation points, not fixes, to the extracted original queue bodies.
p=out/'ipc_full/message_queue.h';t=p.read_text()
needle='public:\n  static size_t getExtraMemoryRequirements()'
assert t.count(needle)==1
t=t.replace(needle,'public:\n#ifdef LDB_IPC_V2_TEST\n  inline static void (*afterPublishedSnapshot)() = nullptr;\n#endif\n  static size_t getExtraMemoryRequirements()',1)
needle='published = shared.producer.published.load(std::memory_order_seq_cst);'
assert t.count(needle)==1
t=t.replace(needle,needle+'\n#ifdef LDB_IPC_V2_TEST\n                if (afterPublishedSnapshot) { afterPublishedSnapshot(); }\n#endif',1)
p.write_text(t)
p=out/'ipc_v2/transport.h';t=p.read_text()
needle='  Reader(Channel& channel, uint32_t timeoutMs = 1000, uint32_t spins = 0)'
assert t.count(needle)==1
t=t.replace(needle,'#ifdef LDB_IPC_V2_TEST\n  inline static void (*afterPublishedSnapshot)() = nullptr;\n#endif\n'+needle,1)
needle='published = shared.producer.published.load(std::memory_order_seq_cst);'
start=t.index('class Reader {');end=t.index(needle,start)+len(needle)
t=t[:end]+'\n#ifdef LDB_IPC_V2_TEST\n        if (afterPublishedSnapshot) { afterPublishedSnapshot(); }\n#endif'+t[end:]
p.write_text(t)
shutil.copy2(ROOT/'tests/ipc-v2/full.cpp',out/'full-test.cpp')
t=(ROOT/'tests/ipc-v2/native.cpp').read_text().replace('../../experiments/ipc-v2/transport.h','ipc_v2/transport.h')
(out/'core-test.cpp').write_text(t)
print('Frozen v1.3.0 negative controls prepared')
