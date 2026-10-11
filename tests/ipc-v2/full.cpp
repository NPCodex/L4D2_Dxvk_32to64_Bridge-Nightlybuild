// SPDX-License-Identifier: MIT
#include <atomic>
#include <cstdio>
#include <thread>
#include <stdexcept>
#define LDB_IPC_V2_TEST
namespace bridge_util {enum class Result {Success,Timeout,Failure};}
#include "ipc_full/message_queue.h"
std::atomic<bool> gbBridgeRunning{true};
using bridge_util::Result;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::string name(){static unsigned i=0;return "full-"+std::to_string(ldb_ipc_v2::processId())+"-"+std::to_string(++i);}
void send(ldb_full::MessageQueue& w,unsigned i){check(w.begin({Commands::IDirect3DDevice9Ex_SetRenderState,0,0,i}),"begin");check(w.scalar(i),"scalar");check(w.finish(false),"finish");}
void read(ldb_full::MessageQueue& r,unsigned i){Result result;auto h=r.pull(result,1000);check(result==Result::Success && h.pHandle==i,"header");check(r.readWord()==i,"body");r.finishRead();}
ldb_ipc_v2::Config fullConfig(){ldb_ipc_v2::Config c;c.blockCount=64;c.blockBytes=4096;return c;}
void pressureCases(){
  // All 64 message credits occupied: the next record must wait, never overwrite.
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);
   for(unsigned i=0;i<64;++i)send(w,i);
   std::atomic<bool> started{false},done{false};
   std::thread producer([&]{started.store(true);send(w,64);done.store(true);});
   while(!started.load())std::this_thread::yield();std::this_thread::sleep_for(std::chrono::milliseconds(20));
   check(!done.load(),"full message queue did not backpressure");
   for(unsigned i=0;i<65;++i)read(r,i);producer.join();}
  // Pool capacity completely pinned: release wakes the sleeping allocator.
  {auto n=name();constexpr unsigned Budget=ldb_full::PoolMetadata+16*4096;
   ldb_full::MessageQueue w(n,Budget,true),r(n,Budget,false);Result result;
   check(w.begin({Commands::IDirect3DVertexBuffer9_Lock,0,0,1}),"full pool begin");
   auto* p=w.beginBlob(16*4096-4,true);check(p!=nullptr,"full pool reserve");std::memset(p,0xa6,16*4096-4);
   check(w.endBlob()&&w.finish(true),"full pool publish");r.pull(result,1000);void* retained=nullptr;r.readBlob(&retained);r.pin(1);r.finishRead();
   std::atomic<bool> started{false},done{false};
   std::thread producer([&]{started.store(true);check(w.begin({Commands::Bridge_Response,0,0,2}),"blocked pool begin");
     auto* q=w.beginBlob(512,true);check(q!=nullptr,"released pool reuse");std::memset(q,0x5b,512);check(w.endBlob()&&w.finish(true),"blocked pool publish");done.store(true);});
   while(!started.load())std::this_thread::yield();std::this_thread::sleep_for(std::chrono::milliseconds(20));
   check(!done.load()&&static_cast<uint8_t*>(retained)[16*4096-5]==0xa6,"pinned full pool overwritten");r.unpin(1);
   r.pull(result,1000);void* q=nullptr;check(result==Result::Success&&r.readBlob(&q)==512&&static_cast<uint8_t*>(q)[511]==0x5b,"pool wake and bytes");r.finishRead();producer.join();}
  // Fragmentation is a real limit: four free pages cannot satisfy two contiguous pages.
  {auto n=name();auto c=fullConfig();constexpr unsigned Budget=ldb_full::PoolMetadata+8*4096;
   ldb_full::UploadPool w(n,Budget,true,c),r(n,Budget,false,c);std::array<ldb_full::Descriptor,8> d{};
   for(auto& lease:d){check(w.allocate(4096,lease),"fragment allocate");lease.bytes=4096;w.publish(lease);r.acquire(lease);}
   for(unsigned i=0;i<8;i+=2)check(r.release(d[i]),"fragment release");
   ldb_full::Descriptor next{};check(!w.allocate(8192,next),"fragmentation incorrectly accepted");
   check(r.release(d[1]),"contiguous release");check(w.allocate(8192,next),"contiguous reclaim failed");w.abort(next);
   for(unsigned i=3;i<8;i+=2)r.release(d[i]);}
}
void corruptionCases(){
  // The generation protects a reused physical offset, not just the slot's state.
  {auto n=name();auto c=fullConfig();ldb_full::UploadPool w(n,2*1024*1024,true,c),r(n,2*1024*1024,false,c);
   ldb_full::Descriptor old{},current{};check(w.allocate(4,old),"old generation allocate");old.bytes=4;w.publish(old);r.acquire(old);r.release(old);
   for(unsigned i=0;i<ldb_full::Slots;++i){check(w.allocate(4,current),"generation cycle");current.bytes=4;w.publish(current);r.acquire(current);r.release(current);}
   check(w.allocate(4,current),"next generation allocate");current.bytes=4;w.publish(current);
   auto forged=current;--forged.generation;bool rejected=false;try{r.acquire(forged);}catch(...){rejected=true;}check(rejected,"old generation accepted");r.acquire(current);r.release(current);}
  // Malformed bodies and upload descriptors cannot return a message credit.
  for(unsigned variant=0;variant<3;++variant){auto n=name();ldb_ipc_v2::Channel raw(n+"-full",fullConfig(),true);
   ldb_full::MessageQueue r(n,2*1024*1024,false);auto& block=raw.block(0);
   block.bytes=variant==2?40:28;block.records=1;block.sequence=0;
   ldb_ipc_v2::Record bad{block.bytes,0,static_cast<uint16_t>(variant==2?ldb_full::External:ldb_full::Inline),0,0,0,block.bytes-24};
   if(variant==0)bad.bodyBytes=UINT32_MAX;if(variant==1)bad.flags=ldb_full::Inline|ldb_full::External;
   std::memcpy(raw.data(block),&bad,sizeof(bad));if(variant==2){const ldb_full::Descriptor invalid{ldb_full::Slots,1,0,4};std::memcpy(raw.data(block)+24,&invalid,16);}
   raw.shared().producer.published.store(1);Result result;r.peek(result,100);
   check(result==Result::Failure&&raw.shared().fault.load()&&raw.shared().consumer.completed.load()==0,"malformed message consumed");}
  // Sequence arithmetic must survive the 32-bit counter wrap on both sides.
  {auto n=name();ldb_ipc_v2::Channel raw(n+"-full",fullConfig(),true);
   raw.shared().producer.published.store(UINT32_MAX-2);raw.shared().consumer.completed.store(UINT32_MAX-2);
   ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);
   for(unsigned i=0;i<128;++i){send(w,i);read(r,i);}check(raw.shared().consumer.completed.load()==125,"sequence wrap");}
  // An OS wait failure faults the stream and all four channels in this session.
  {std::array<std::unique_ptr<ldb_full::MessageQueue>,4> q;
   for(auto& p:q)p=std::make_unique<ldb_full::MessageQueue>(name(),2*1024*1024,false);
   ldb_ipc_v2::Event::failNextWait=true;Result result;q[0]->peek(result,100);
   check(result==Result::Failure,"OS wait failure hidden");for(const auto& p:q)check(!p->healthy(),"session fault not propagated");}
}
int main(){try{
  pressureCases();corruptionCases();
  for(unsigned batch:{1u,2u,4u,8u,16u,32u}){
    auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);w.configureBatch(batch);check(w.batchSize()==batch,"gear normalization");
    for(unsigned cycle=0;cycle<3;++cycle){
      for(unsigned i=0;i+1<batch;++i)send(w,cycle*batch+i);
      if(batch>1){Result result;r.peek(result,1);check(result==Result::Timeout&&w.pending(),"partial gear visible");
        bool rejected=false;try{w.configureBatch(1);}catch(...){rejected=true;}check(rejected,"pending gear reconfiguration");}
      send(w,cycle*batch+batch-1);check(!w.pending(),"full gear not published");for(unsigned i=0;i<batch;++i)read(r,cycle*batch+i);
    }
    for(auto command:{Commands::Bridge_Terminate,Commands::IDirect3DDevice9Ex_Present,Commands::IDirect3DDevice9Ex_Reset,Commands::IDirect3DTexture9_Destroy,Commands::IDirect3DQuery9_GetData}){
      if(batch>1)send(w,200);
      check(w.begin({command,0,0,201})&&w.scalar(201)&&w.finish(true),"semantic boundary flush");
      check(!w.pending(),"semantic tail retained");if(batch>1)read(r,200);read(r,201);
    }
    send(w,300);if(batch>1){check(w.pending()&&w.due(UINT64_MAX)&&w.flush(),"lazy deadline flush");}read(r,300);
    for(unsigned invalid:{0u,3u,5u,15u,31u,33u,64u,UINT32_MAX}){w.configureBatch(invalid);check(w.batchSize()==1,"invalid gear fallback");}
  }
  {auto n=name();ldb_full::MessageQueue r(n,2*1024*1024,false),w(n,2*1024*1024,true);w.configureBatch(32);
   for(unsigned i=0;i<31;++i)send(w,i);check(w.pending(),"batch pending");Result result;r.peek(result,1);check(result==Result::Timeout,"partial visible");
   send(w,31);for(unsigned i=0;i<32;++i)read(r,i);check(w.begin({Commands::Bridge_Response,0,0,42}),"reply begin");
   const uint8_t data[]={1,2,3};check(w.blob(3,data),"inline blob");check(w.finish(true),"reply finish");
   r.pull(result,1000);void* p=nullptr;check(r.readBlob(&p)==3 && std::memcmp(p,data,3)==0,"inline blob body");r.finishRead();}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);
   std::thread host([&]{for(unsigned i=0;i<100000;++i)read(r,i);});
   for(unsigned i=0;i<100000;++i)send(w,i);host.join();}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);Result result;
   check(w.begin({Commands::IDirect3DVertexBuffer9_Lock,0,0,7}),"lock begin");auto* p=w.beginBlob(80000,true);check(p!=nullptr,"reserved blob");std::memset(p,0xa5,80000);check(w.endBlob(),"end blob");check(w.scalar(123),"borrowed stable append");check(w.finish(true),"lock finish");
   r.pull(result,1000);void* retained=nullptr;check(r.readBlob(&retained)==80000,"lease length");r.pin(7);check(r.readWord()==123,"footer");r.finishRead();
   for(unsigned i=0;i<5000;++i){send(w,i);read(r,i);check(static_cast<uint8_t*>(retained)[79999]==0xa5,"pin overwrite");}
   r.unpin(7);for(unsigned i=0;i<300;++i){check(w.begin({Commands::Bridge_Response,0,0,i}),"pool reuse begin");p=w.beginBlob(80000,true);check(p!=nullptr,"pool reuse allocation");std::memset(p,i,80000);check(w.endBlob() && w.finish(true),"pool reuse publish");r.pull(result,1000);void* q=nullptr;check(r.readBlob(&q)==80000 && static_cast<uint8_t*>(q)[79999]==static_cast<uint8_t>(i),"pool reuse bytes");r.finishRead();}}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);Result result;
   check(w.begin({Commands::IDirect3DVertexBuffer9_Lock,0,0,7}),"mutable Lock begin");check(w.scalar(1),"Lock UID");auto* p=w.beginBlob(1024,true);check(p!=nullptr,"Lock lease");check(w.endBlob() && w.finish(true),"Lock publication");
   // Application writes after Lock publication, before the later Unlock record.
   std::memset(p,0x5a,1024);const auto reference=static_cast<uint32_t>(reinterpret_cast<uint32_t*>(p)-w.dataBase());
   r.pull(result,1000);check(r.readWord()==1,"Lock UID read");void* q=nullptr;check(r.readBlob(&q)==1024,"Lock body");r.pin(7);r.finishRead();
   check(w.begin({Commands::IDirect3DVertexBuffer9_Unlock,Commands::DataIsReserved,0,7}),"Unlock begin");check(w.scalar(reference) && w.finish(true),"Unlock publication");
   r.pull(result,1000);check(r.readWord()==reference,"Unlock reference");q=r.reservedData(7,reference,1024);check(std::memcmp(p,q,1024)==0,"mutable reserved bytes");
   bool rejected=false;try{r.reservedData(7,reference+256,1);}catch(...){rejected=true;}check(rejected,"reserved range overrun");r.unpin(7);r.finishRead();}
  {auto n=name();ldb_ipc_v2::Config c;ldb_full::UploadPool w(n,2*1024*1024,true,c),r(n,2*1024*1024,false,c);ldb_full::Descriptor d{};check(w.allocate(80000,d),"pool allocate");d.bytes=80000;w.publish(d);r.acquire(d);r.retain(d);check(!r.release(d),"early release");check(r.release(d),"last release");bool rejected=false;try{r.acquire(d);}catch(...){rejected=true;}check(rejected,"stale descriptor accepted");}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);check(w.begin({Commands::Bridge_Response,0,0,0}),"abort begin");check(w.scalar(1),"abort scalar");w.abort();check(w.flush(),"abort flush");Result result;r.peek(result,1);check(result==Result::Timeout,"aborted visible");}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);check(w.begin({Commands::Bridge_Response,0,0,0}),"underfill begin");check(w.declare(8),"declare");check(w.scalar(1),"underfill scalar");check(!w.finish(true),"underfill accepted");w.abort();}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);std::atomic<bool> early{true};Result result;r.peek(result,1,&early);check(result==Result::Timeout,"cancel");w.fault();check(!w.begin({Commands::Bridge_Response,0,0,0}),"post fault begin");check(!w.flush(),"post fault flush");}
  std::puts("IPC_FULL_PASS inline/blob/batch/wrap/long-Lock/lease-reuse/stale/abort/underfill/cancel/fault/full-queue/full-pool/fragmentation/generation/malformed/counter-wrap/OS-wait/session-fault");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"IPC_FULL_FAIL %s\n",e.what());return 1;}}
