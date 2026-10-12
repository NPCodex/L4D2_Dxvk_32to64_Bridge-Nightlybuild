"""Reuse frozen production fixture adapters, compile the complete full transport."""
from pathlib import Path
import argparse
import hashlib,json,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
pr6=ROOT/'.deps/ipc-v2-pr6';sys.path.insert(0,str(pr6/'scripts'))
from generate_ipc_transport_test import generate
parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,default=ROOT/'.deps/dxvk-remix')
parser.add_argument('--output',type=Path,default=ROOT/'work/ipc-full-native')
args=parser.parse_args()
source=args.source;out=args.output
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
shutil.copy2(ROOT/'tests/ipc_reply_budget_cases.h',out/'ipc_reply_budget_cases.h')
shutil.copy2(pr6/'tests/ipc_wait_injection.h',out/'ipc_wait_injection.h')
manifest={p.relative_to(util).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [util/'util_bridgecommand.h',util/'util_bridgecommand.cpp',util/'util_ipcchannel.h',*sorted((util/'ipc_full').glob('*.h')),*sorted((util/'ipc_v2').glob('*.h'))]}
(out/'full-source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

# Add actual mapped LockableBuffer to the existing cross-process harness.
client=source/'bridge/src/client'
if (client/'buffer_mapped_shadow.h').exists():
    (out/'client').mkdir(exist_ok=True)
    for name in ('buffer_shadow.h','buffer_shadow_arena.h','buffer_mapped_shadow.h','memory_diagnostics.h'):
        text=(client/name).read_text().replace('../util/queue_wait_diagnostics.h','../queue_wait_diagnostics.h').replace('../util/runtime_observation.h','../runtime_observation.h')
        (out/'client'/name).write_text(text)
    options=out/'config/global_options.h'
    options.write_text(options.read_text().replace('struct GlobalOptions {','struct GlobalOptions {\n static bool getUseSharedHeap(){return false;}\n static bool getUseSharedHeapForDynamicBuffers(){return false;}\n static bool getUseSharedHeapForStaticBuffers(){return false;}\n static bool getAlwaysCopyEntireStaticBuffer(){return false;}'))
    support='#include <d3d9.h>\n#include <queue>\n#include "client/buffer_shadow.h"\nstruct BaseDirect3DDevice9Ex_LSS {};\ntemplate<class T> struct Direct3DResource9_LSS {\n  Direct3DResource9_LSS(T*, BaseDirect3DDevice9Ex_LSS*) {}\n  uint32_t getId() const { return 77; }\n};\nnamespace SharedHeap { using AllocId=uint32_t; constexpr AllocId kInvalidId=UINT32_MAX;\ninline AllocId allocate(size_t){return kInvalidId;} inline void deallocate(AllocId){}\ninline uint8_t* getBuf(AllocId){return nullptr;} }\nstruct ClientOptions {\nstatic bool getOptimizedDynamicLock(){return false;}\nstatic bool getMappedBufferShadow(){return true;}\nstatic size_t getMappedBufferMinimum(){return 262144;}\nstatic size_t getMappedBufferCacheBudget(){return 0;}\n};\nusing DeviceBridge=Bridge<BridgeId::Device>; using ClientMessage=DeviceBridge::Command;\n'
    text=(client/'lockable_buffer.h').read_text();marker='template <typename T>'
    support+=marker+text.split(marker,1)[1]
    support+='\ntemplate<class T> class MappedTransportBuffer : public LockableBuffer<T> {\nusing Desc=std::conditional_t<std::is_same_v<T,IDirect3DVertexBuffer9>,D3DVERTEXBUFFER_DESC,D3DINDEXBUFFER_DESC>;\npublic: explicit MappedTransportBuffer(const Desc& d):LockableBuffer<T>(nullptr,nullptr,d){}\nusing LockableBuffer<T>::lock;using LockableBuffer<T>::unlock;};\n'
    (out/'mapped_buffer_template.h').write_text(support)
    shutil.copy2(ROOT/'tests/vbib_mapped_transport.h',out/'vbib_mapped_transport.h')
    (out/'full_bridge_cases.cpp').write_text((out/'full_bridge_cases.cpp').read_text().replace('int wmain(', '#ifdef LDB_MAPPED_BUFFER_TRANSPORT\n#include "vbib_mapped_transport.h"\n#endif\nint wmain(',1).replace('    else {transfer<Device>();transfer<Module>();}', '    #ifdef LDB_MAPPED_BUFFER_TRANSPORT\n    else if(std::wstring(argv[2])==L"vbib-mapped"){mappedBufferTransfer();}\n    #endif\n    else {transfer<Device>();transfer<Module>();}',1))
