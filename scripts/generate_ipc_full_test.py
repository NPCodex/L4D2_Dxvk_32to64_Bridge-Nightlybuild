"""Reuse frozen production fixture adapters, compile the complete full transport."""
from pathlib import Path
import hashlib,json,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
pr6=ROOT/'.deps/ipc-v2-pr6';sys.path.insert(0,str(pr6/'scripts'))
from generate_ipc_transport_test import generate
source=ROOT/'.deps/dxvk-remix';out=ROOT/'work/ipc-full-native'
baseline=ROOT/'work/c32-source'
if not baseline.exists():baseline=ROOT/'.deps/c32-baseline'
generate(baseline,out)
util=source/'bridge/src/util'
for folder in ('ipc_full','ipc_v2'):shutil.copytree(util/folder,out/folder,dirs_exist_ok=True)
for name in ('util_ipcchannel.h','util_commands.h','ipc_c32_message_queue.h'):shutil.copy2(util/name,out/name)
options=out/'config/global_options.h';options.write_text(options.read_text().replace('struct GlobalOptions {','struct GlobalOptions {\n static uint32_t getIpcBatchSize(){return 1;}'))
header=(util/'util_bridgecommand.h').read_text();bridge=header[header.index('struct BridgeReadFailure'):]
bridge=bridge.replace('public:\n  static void init(','''public:
  static void install(WriterChannel* writer, ReaderChannel* reader) {
    s_pWriterChannel=writer; s_pReaderChannel=reader; s_cmdUID=0;s_readActive=false;
  }
  static void init(''',1)
support=(pr6/'tests/ipc_transport_support.h').read_text()
cpp=(util/'util_bridgecommand.cpp').read_text();cpp='\n'.join(line for line in cpp.splitlines() if not line.startswith('#include'))
(out/'ipc_transport.cpp').write_text(support+'\n'+bridge+'\n'+cpp+'\n#include "full_bridge_cases.cpp"\n')
shutil.copy2(ROOT/'tests/ipc-v2/full_bridge_cases.cpp',out/'full_bridge_cases.cpp')
shutil.copy2(pr6/'tests/ipc_wait_injection.h',out/'ipc_wait_injection.h')
manifest={p.relative_to(util).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [util/'util_bridgecommand.h',util/'util_bridgecommand.cpp',util/'util_ipcchannel.h',*sorted((util/'ipc_full').glob('*.h')),*sorted((util/'ipc_v2').glob('*.h'))]}
(out/'full-source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
