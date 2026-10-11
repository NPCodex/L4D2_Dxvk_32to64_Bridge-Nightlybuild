// SPDX-License-Identifier: MIT
#include <atomic>
#include <cstdio>
#include <thread>
#include <stdexcept>
namespace bridge_util {enum class Result {Success,Timeout,Failure};}
#include "ipc_full/message_queue.h"
std::atomic<bool> gbBridgeRunning{true};
using bridge_util::Result;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
std::string name(){static unsigned i=0;return "full-"+std::to_string(ldb_ipc_v2::processId())+"-"+std::to_string(++i);}
void send(ldb_full::MessageQueue& w,unsigned i){check(w.begin({Commands::IDirect3DDevice9Ex_SetRenderState,0,0,i}),"begin");check(w.scalar(i),"scalar");check(w.finish(false),"finish");}
void read(ldb_full::MessageQueue& r,unsigned i){Result result;auto h=r.pull(result,1000);check(result==Result::Success && h.pHandle==i,"header");check(r.readWord()==i,"body");r.finishRead();}
int main(){try{
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
  {auto n=name();ldb_ipc_v2::Config c;ldb_full::UploadPool w(n,2*1024*1024,true,c),r(n,2*1024*1024,false,c);ldb_full::Descriptor d{};check(w.allocate(80000,d),"pool allocate");d.bytes=80000;w.publish(d);r.acquire(d);r.retain(d);check(!r.release(d),"early release");check(r.release(d),"last release");bool rejected=false;try{r.acquire(d);}catch(...){rejected=true;}check(rejected,"stale descriptor accepted");}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);check(w.begin({Commands::Bridge_Response,0,0,0}),"abort begin");check(w.scalar(1),"abort scalar");w.abort();check(w.flush(),"abort flush");Result result;r.peek(result,1);check(result==Result::Timeout,"aborted visible");}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);check(w.begin({Commands::Bridge_Response,0,0,0}),"underfill begin");check(w.declare(8),"declare");check(w.scalar(1),"underfill scalar");check(!w.finish(true),"underfill accepted");w.abort();}
  {auto n=name();ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);std::atomic<bool> early{true};Result result;r.peek(result,1,&early);check(result==Result::Timeout,"cancel");w.fault();check(!w.begin({Commands::Bridge_Response,0,0,0}),"post fault begin");check(!w.flush(),"post fault flush");}
  std::puts("IPC_FULL_PASS inline/blob/batch/wrap/long-Lock/lease-reuse/stale/abort/underfill/cancel/fault");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"IPC_FULL_FAIL %s\n",e.what());return 1;}}
