"""Frozen published C32 versus full: identical production fixture call stream."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import sys
ROOT=Path(__file__).resolve().parents[1]
pr6=ROOT/'.deps/ipc-v2-pr6'
sys.path.insert(0,str(pr6/'scripts'))
from generate_ipc_transport_test import generate
frozen=ROOT/'.deps/c32-frozen'
source=ROOT/'work/c32-source'
if not source.exists():source=ROOT/'.deps/c32-baseline'
out=ROOT/'work/ipc-full-comparison'
generate(source,out)
util=source/'bridge/src/util'
shutil.copytree(frozen/'experiments/ipc-v2',out/'ipc_v2',dirs_exist_ok=True)
shutil.copy2(frozen/'experiments/ipc-c32/message_queue.h',out/'ipc_c32_message_queue.h')
spec=importlib.util.spec_from_file_location('frozen_prepare',frozen/'scripts/prepare_ipc_c32.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
digest=module.identity()
(out/'ipc_v2/ipc_v2_build.h').write_text(''.join(f'#define LDB_IPC_V2_BUILD{i} 0x{digest[i*8:i*8+8]}u\n' for i in range(4)))
options=out/'config/global_options.h'
options.write_text(options.read_text().replace('struct GlobalOptions {','struct GlobalOptions {\n static uint32_t getIpcBatchSize(){return 1;}'))
code=(out/'ipc_transport.cpp').read_text().replace('#include "ipc_transport_cases.cpp"','#include "full_bridge_cases.cpp"')
(out/'ipc_transport.cpp').write_text(code)
shutil.copy2(ROOT/'tests/ipc-v2/full_bridge_cases.cpp',out/'full_bridge_cases.cpp')
shutil.copy2(pr6/'tests/ipc_wait_injection.h',out/'ipc_wait_injection.h')
manifest={p.relative_to(util).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in
          (util/'util_bridgecommand.h',util/'util_bridgecommand.cpp',util/'util_ipcchannel.h')}
manifest['frozen_build_id']='ldb-ipc-c32-dev+'+digest[:16]
manifest['frozen_commit']='2ce9f9b0e50ca9bc9af592478769fa8374596344'
manifest['caller_sha256']=hashlib.sha256((out/'full_bridge_cases.cpp').read_bytes()).hexdigest()
(out/'baseline-source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
