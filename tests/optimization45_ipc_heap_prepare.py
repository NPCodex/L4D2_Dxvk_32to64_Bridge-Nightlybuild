"""Compile the unchanged production SharedHeap with only logger/options/command adapters."""
import hashlib
import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--logger', required=True, type=Path)
args = parser.parse_args()
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_buffer_contract_test import method
SOURCE = args.source / 'bridge/src/util'
OUT = args.output
OUT.mkdir(parents=True, exist_ok=True)
names = ('util_sharedheap.h', 'util_sharedheap.cpp', 'util_common.h', 'util_sharedmemory.h',
         'util_sharedmemory.cpp', 'util_guid.h', 'util_bytes.h', 'util_commands.h')
for name in names:
    shutil.copy2(SOURCE / name, OUT / name)
(OUT / 'log').mkdir(exist_ok=True)
shutil.copy2(args.logger, OUT / 'log/log.h')
(OUT / 'config').mkdir(exist_ok=True)
(OUT / 'config/global_options.h').write_text('''#pragma once
#include <cstdint>
struct GlobalOptions {
static uint32_t getSharedHeapChunkSize(){return 4096;}
static uint32_t getSharedHeapDefaultSegmentSize(){return 262144;}
static uint32_t getSharedHeapFreeChunkWaitTimeout(){return 0;}
static bool getUseSharedHeap(){return true;}
};
''')
(OUT / 'api_wait_diagnostics.h').write_text('''#pragma once
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <vector>
namespace l4d2_api_wait {
enum class Metric {SharedHeapRetry}; enum class Api {Other};
struct Span {Span(Metric,Api,bool){} void start(){}};
}
''')
(OUT / 'util_devicecommand.h').write_text('''#pragma once
#include <cstdint>
#include <vector>
#include "util_commands.h"
struct HeapMessage {Commands::D3D9Command command; uint32_t handle; std::vector<uint32_t> fields;};
inline std::vector<HeapMessage> heapMessages;
struct ClientMessage {
HeapMessage message;
ClientMessage(Commands::D3D9Command command,uint32_t handle):message{command,handle,{}}{}
void send_data(uint32_t value){message.fields.push_back(value);}
~ClientMessage(){heapMessages.push_back(message);}
};
''')
(OUT / 'source-manifest.json').write_text(json.dumps({
    'files': {name: hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() for name in names},
    'adapters': ['logger', 'options', 'ClientMessage captures exact command fields', 'disabled timing spans'],
    'memory': 'production SharedMemory uses real Windows mappings',
}, indent=2) + '\n')

# Only OS heap allocation is fault-injected; the complete VB/IB template and
# the exact Surface shared-heap branch/pointer helper come from this build.
support = (ROOT / 'tests/buffer_contract_support.h').read_text()
for getter in ('getUseSharedHeap', 'getUseSharedHeapForDynamicBuffers', 'getUseSharedHeapForStaticBuffers'):
    support = support.replace(f'{getter}() {{ return false; }}', f'{getter}() {{ return true; }}')
begin = support.index('namespace SharedHeap {')
end = support.index('\n}\n', begin) + 3
support = support[:begin] + '''#include <map>
namespace SharedHeap {
using AllocId = uint32_t; constexpr AllocId kInvalidId = UINT32_MAX;
inline bool failAllocate=false, failPointer=false;
inline AllocId next=1;
inline std::map<AllocId,std::vector<uint8_t>> buffers;
inline AllocId allocate(size_t size) {if(failAllocate)return kInvalidId;auto id=next++;buffers[id].resize(size);return id;}
inline void deallocate(AllocId id) {buffers.erase(id);}
inline uint8_t* getBuf(AllocId id) {auto it=buffers.find(id);return failPointer || it==buffers.end()?nullptr:it->second.data();}
}
''' + support[end:]
client = args.source / 'bridge/src/client'
template = (client / 'lockable_buffer.h').read_text()
template = template[template.index('template <typename T>'):]
surface = (client / 'd3d9_surface.cpp').read_text()
start, end = method(surface, 'bool Direct3DSurface9_LSS::lock(')
lock = surface[start:end]
start, end = method(lock, '  if (m_bUseSharedHeap) {')
shared_branch = lock[start:end]
start, end = method(surface, 'void* Direct3DSurface9_LSS::getBufPtr(')
pointer = surface[start:end].replace('Direct3DSurface9_LSS::getBufPtr', 'HeapSurface::getBufPtr')
wrapper = '''
namespace bridge_util { inline size_t calcImageByteOffset(int pitch,const RECT& rect,D3DFORMAT) {return size_t(rect.top)*pitch+rect.left*4;} }
struct HeapSurface {
D3DSURFACE_DESC m_desc{}; bool m_bUseSharedHeap=true; SharedHeap::AllocId m_bufferId=SharedHeap::kInvalidId;
struct Info {D3DLOCKED_RECT locked;RECT rect;DWORD flags;SharedHeap::AllocId id,discard;}; std::queue<Info> m_lockInfoQueue;
void* getBufPtr(int,const RECT&);
bool lock(D3DLOCKED_RECT& lockedRect,const RECT& rect,DWORD flags) {lockedRect.Pitch=16;uint32_t surfaceSize=64;
''' + shared_branch + '\nreturn true;}\n};\n' + pointer
(OUT / 'clientheap-production.h').write_text(support + '\n' + template + '\n' + wrapper)
manifest = json.loads((OUT / 'source-manifest.json').read_text())
manifest['client_files'] = {name: hashlib.sha256((client / name).read_bytes()).hexdigest()
                            for name in ('lockable_buffer.h', 'd3d9_surface.cpp')}
manifest['client_adapters'] = ['SharedHeap allocation/view failure injection', 'disabled transport/observers',
                              'full LockableBuffer template', 'Surface exact shared-heap branch and pointer helper']
(OUT / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
