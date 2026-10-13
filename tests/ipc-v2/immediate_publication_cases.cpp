// SPDX-License-Identifier: MIT
// Extracted production MessageQueue, transport and Command failure invariants.
using Device = Bridge<BridgeId::Device>;
using Module = Bridge<BridgeId::Module>;
void require(bool b, const char* what) { if(!b) throw std::runtime_error(what); }
ldb_ipc_v2::Channel* target = nullptr;
unsigned hookCalls = 0;
unsigned serial = 0;
std::string testName(const char* base) {
  return std::string(base)+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++serial);
}
ldb_ipc_v2::Config config(){ldb_ipc_v2::Config c;c.blockCount=64;c.blockBytes=4096;return c;}
void closeConsumer() { ++hookCalls; target->shared().cold.consumerClosed.store(1, std::memory_order_seq_cst); }
void poisonChannel() { ++hookCalls; target->fault(ldb_ipc_v2::Fault::Protocol); }
void armNotificationFailure() {
  ++hookCalls; target->shared().messages.armed.store(1, std::memory_order_seq_cst);
  immediate_publication_test::failNextNotify.store(true);
}
void rawCase(unsigned batch, unsigned tail, bool external, const std::string& mode) {
  gbBridgeRunning.store(true); hookCalls=0; immediate_publication_test::failedNotifications.store(0);
  auto n=testName("deadline-raw");
  ldb_full::MessageQueue w(n,2*1024*1024,true), r(n,2*1024*1024,false);
  ldb_ipc_v2::Channel raw(n+"-full",config(),false);target=&raw;w.configureBatch(batch);
  for(unsigned i=0;i<tail;++i){
    require(w.begin({Commands::IDirect3DDevice9Ex_SetRenderState,0,0,i})&&w.scalar(i)&&w.finish(false),"tail setup");
  }
  require(w.pending()==bool(tail),"tail pending before fault");
  require(w.begin({Commands::IDirect3DQuery9_GetData,0,0,42}),"begin before fault");
  std::array<uint8_t,512> payload{};payload.fill(0xa6);
  require(external?w.blob(payload.size(),payload.data()):w.scalar(42),"write before fault");
  if(mode=="late-close")immediate_publication_test::afterEncoded=closeConsumer;
  else if(mode=="late-poison")immediate_publication_test::afterEncoded=poisonChannel;
  else if(mode=="notify-failure")immediate_publication_test::afterEncoded=armNotificationFailure;
  else throw std::runtime_error("unknown raw mode");
  // C1 force=false also reaches the optimized immediate-publication path.
  require(!w.finish(batch!=1||external),"faulted finish succeeded");
  require(hookCalls==1,"fault hook not reached after encoding");
  require(w.pending()&&w.due(UINT64_MAX),"failed immediate publication lost pending marker");
  require(!w.healthy(),"failed queue healthy");
  auto published=raw.shared().producer.published.load();
  require(published==(mode=="notify-failure"?1u:0u),"wrong publication timeline");
  auto expected=mode=="late-close"?ldb_ipc_v2::Fault::PeerExited:
      mode=="late-poison"?ldb_ipc_v2::Fault::Protocol:ldb_ipc_v2::Fault::SystemWait;
  require(raw.shared().fault.load()==unsigned(expected),"fault reason not preserved");
  if(mode=="notify-failure") {
    require(immediate_publication_test::failedNotifications.load()==1,"production notify call-site injection missed");
    auto& b=raw.block(0);require(b.records==tail+1,"encoded tail lost or duplicated");
  }
  require(!w.flush()&&!w.begin({Commands::Bridge_Response,0,0,9}),"poisoned queue retried");
  require(raw.shared().producer.published.load()==published,"retry duplicated publication");
  bridge_util::Result status;r.peek(status,1);
  require(status==Result::Failure&&raw.shared().consumer.completed.load()==0,"poisoned stream consumed credit");
  std::printf("RAW_FAILURE_PASS batch=%u tail=%u external=%u mode=%s published=%u pending=1\n",batch,tail,external?1:0,mode.c_str(),published);
}
void wrapperCase(bool external,const std::string& mode){
  gbBridgeRunning.store(true);hookCalls=0;immediate_publication_test::failedNotifications.store(0);
  WriterChannel dw(testName("DeviceForward"),2*1024*1024,256,32768);
  ReaderChannel dr(testName("DeviceReply"),2*1024*1024,256,32768);
  WriterChannel mw(testName("ModuleForward"),2*1024*1024,256,32768);
  ReaderChannel mr(testName("ModuleReply"),2*1024*1024,256,32768);
  Device::install(&dw,&dr);Module::install(&mw,&mr);dw.commands->configureBatch(8);
  ldb_ipc_v2::Channel raw(dw.commands->name()+"-full",config(),false);target=&raw;
  {
    Device::Command c(Commands::IDirect3DQuery9_GetData,42);
    if(external){std::array<uint8_t,512> b{};b.fill(0x7c);c.send_data(b.size(),b.data());}
    else c.send_data(42);
    if(mode=="late-close")immediate_publication_test::afterEncoded=closeConsumer;
    else if(mode=="late-poison")immediate_publication_test::afterEncoded=poisonChannel;
    else immediate_publication_test::afterEncoded=armNotificationFailure;
    require(c.finish()!=Result::Success,"production Command hid failed publication");
    require(hookCalls==1&&dw.commands->pending(),"production missing failure marker/hook");
    require(!gbBridgeRunning.load()&&!dw.commands->healthy()&&!dr.commands->healthy()&&
        !mw.commands->healthy()&&!mr.commands->healthy(),"production did not poison all four channels");
    auto before=raw.shared().producer.published.load();require(c.finish()!=Result::Success,"idempotent finish lost failure");
    require(before==raw.shared().producer.published.load(),"idempotent finish republished");
  }
  auto published=raw.shared().producer.published.load();
  require(published==(mode=="notify-failure"?1u:0u),"wrapper timeline differs");
  {Device::Command c(Commands::IDirect3DDevice9Ex_SetRenderState,88);c.send_data(88);require(c.finish()!=Result::Success,"post-poison command accepted");}
  Device::apiEntry();require(!Device::flushCommands(),"explicit flush succeeded after poison");
  require(raw.shared().producer.published.load()==published,"wrapper retry duplicated publication");
  std::printf("WRAPPER_FAILURE_PASS external=%u mode=%s published=%u all_four_poisoned=1\n",external?1:0,mode.c_str(),published);
}
void concurrentCloseCase(bool beforeCheck) {
  gbBridgeRunning.store(true);hookCalls=0;
  auto n=testName("deadline-race");ldb_full::MessageQueue w(n,2*1024*1024,true),r(n,2*1024*1024,false);
  ldb_ipc_v2::Channel raw(n+"-full",config(),false);target=&raw;w.configureBatch(8);
  require(w.begin({Commands::IDirect3DQuery9_GetData,0,0,99})&&w.scalar(99),"race command setup");
  // Hook joins a distinct peer thread to force the exact close/publication order.
  auto closeOnPeer=[](){++hookCalls;std::thread t([](){target->shared().cold.consumerClosed.store(1,std::memory_order_seq_cst);});t.join();};
  if(beforeCheck)immediate_publication_test::afterEncoded=closeOnPeer;
  else immediate_publication_test::beforePublish=closeOnPeer;
  bool success=w.finish(true);require(hookCalls==1,"race schedule hook");
  require(success==!beforeCheck,"existing close linearization changed");
  require(raw.shared().producer.published.load()==(beforeCheck?0u:1u),"close race publication frontier");
  if(beforeCheck)require(w.pending()&&!w.healthy(),"precheck close not terminal");
  else {
    require(!w.pending(),"successful publish not cleared");
    require(!w.begin({Commands::Bridge_Response,0,0,5})&&!w.healthy(),"next record ignored closed peer");
    require(raw.shared().producer.published.load()==1,"postcheck-close retry duplicated");
  }
  std::printf("CLOSE_TIMELINE_PASS close_before_check=%u published=%u\n",beforeCheck?1:0,raw.shared().producer.published.load());
}
void clientConcurrencyCase() {
#ifdef REMIX_BRIDGE_CLIENT
  gbBridgeRunning.store(true);immediate_publication_test::afterEncoded=nullptr;immediate_publication_test::beforePublish=nullptr;
  constexpr unsigned Threads=4,PerThread=512,Total=Threads*PerThread;
  auto n=testName("ConcurrentDevice");
  WriterChannel dw(n,2*1024*1024,256,32768);ReaderChannel dr(testName("ConcurrentReply"),2*1024*1024,256,32768);
  Device::install(&dw,&dr);dw.commands->configureBatch(8);
  ldb_full::MessageQueue consumer(dw.commands->name(),2*1024*1024,false);
  std::atomic<bool> stop{false};std::atomic<unsigned> entries{0};
  std::thread api([&]{while(!stop.load()){Device::apiEntry();++entries;std::this_thread::yield();}});
  std::thread reader([&]{
    std::vector<bool> seen(Total,false);
    for(unsigned expectedUid=0;expectedUid<Total;++expectedUid){
      Result status;auto h=consumer.pull(status,2000);
      require(status==Result::Success&&h.pHandle<Total&&!seen[h.pHandle],"concurrent output duplicate/missing");
      seen[h.pHandle]=true;require(consumer.readWord()==expectedUid,"serialized UID order changed");
      if(h.pHandle%4==0){void* p=nullptr;require(consumer.readBlob(&p)==512,"concurrent external size");
        for(unsigned j=0;j<512;++j)require(static_cast<uint8_t*>(p)[j]==uint8_t(h.pHandle),"concurrent external byte");}
      else require(consumer.readWord()==h.pHandle,"concurrent scalar corruption");
      consumer.finishRead();
    }
  });
  std::vector<std::thread> writers;
  for(unsigned t=0;t<Threads;++t)writers.emplace_back([&,t]{for(unsigned j=0;j<PerThread;++j){
    const unsigned id=t*PerThread+j;
    Device::apiEntry();
    Device::Command c(id%7==0?Commands::IDirect3DQuery9_GetData:Commands::IDirect3DDevice9Ex_SetRenderState,id);
    if(id%4==0){std::array<uint8_t,512>b{};b.fill(uint8_t(id));c.send_data(512,b.data());}
    else c.send_data(id);
    require(c.finish()==Result::Success,"concurrent production finish failed");
  }});
  for(auto& t:writers)t.join();require(Device::flushCommands(),"concurrent final tail flush");
  reader.join();stop.store(true);api.join();
  require(entries.load()>0&&!dw.commands->pending()&&dw.commands->healthy()&&consumer.healthy(),"concurrent completion state");
  std::printf("PRODUCTION_CONCURRENCY_PASS writers=%u commands=%u api_entries=%u batch=8 scalar/external/forced/UID-order\n",Threads,Total,entries.load());
#else
  std::puts("PRODUCTION_CONCURRENCY_NOT_APPLICABLE host has no Client multiwriter serialization");
#endif
}
int wmain(int argc,wchar_t** argv){try{
  // Negative control is expected to fail at the pending-marker invariant.
  if(argc==2&&std::wstring(argv[1])==L"--negative") {rawCase(8,0,false,"late-close");return 0;}
  if(argc==2&&std::wstring(argv[1])==L"--concurrency") {clientConcurrencyCase();return 0;}
  unsigned count=0;
  for(auto mode:{"late-close","late-poison","notify-failure"}){
    for(auto external:{false,true}){
      rawCase(1,0,external,mode);++count;
      rawCase(8,0,external,mode);++count;
      rawCase(8,3,external,mode);++count;
      wrapperCase(external,mode);++count;
    }
  }
  concurrentCloseCase(true);concurrentCloseCase(false);count+=2;
  std::printf("IMMEDIATE_PUBLICATION_PASS cases=%u bits=%u\n",count,unsigned(sizeof(void*)*8));
  return 0;
}catch(const std::exception& e){std::fprintf(stderr,"IMMEDIATE_PUBLICATION_FAIL %s\n",e.what());return 2;}}
