// SPDX-License-Identifier: MIT
template<class B> void optimization45Replies(bool abandoned) {
  GlobalOptions::retries = 2;
#ifdef REMIX_BRIDGE_CLIENT
  std::atomic<bool> sent{false};
  std::thread first([&] {
    UID uid;
    {typename B::Command c(Commands::IDirect3DQuery9_GetData, 111);
     uid=static_cast<UID>(c.get_uid());c.send_data(111);
     require(c.finish()==Result::Success,"first request");}
    sent.store(true);
    if (abandoned) return;
    Sleep(120);
    require(B::waitForCommand(Commands::Bridge_Response,1,nullptr,true,uid,1000)==Result::Success,"delayed owner wait");
    require(B::get_data()==111,"delayed owner data");B::pop_front();
  });
  while (!sent.load()) std::this_thread::yield();
  UID uid;
  {typename B::Command c(Commands::IDirect3DQuery9_GetData, 222);
   uid=static_cast<UID>(c.get_uid());c.send_data(222);
   require(c.finish()==Result::Success,"second request");}
  const auto before=GetTickCount64();
  const auto result=B::waitForCommand(Commands::Bridge_Response,1,nullptr,true,uid,abandoned?180:1000);
  const auto elapsed=GetTickCount64()-before;
  first.join();
  if (abandoned) {
    require(result==Result::Failure && !gbBridgeRunning.load() && elapsed>=150 && elapsed<800,"abandoned owner hard deadline");
  } else {
    require(result==Result::Success && elapsed>=90 && elapsed<800,"default retries spent on other UID");
    require(B::get_data()==222,"second owner data");B::pop_front();
  }
#else
  for(unsigned n=0;n<2;++n) {
    require(B::waitForCommand(Commands::Bridge_Any,2000)==Result::Success,"reply regression request");
    const auto h=B::pop_front();const auto uid=B::get_data();const auto value=B::get_data();B::end_read_data();
    require(h.pHandle==value,"reply regression identity");
    typename B::Command c(Commands::Bridge_Response,uid);c.send_data(value);
    require(c.finish()==Result::Success,"reply regression finish");
  }
  // Keep the producer alive while the caller deliberately holds the queue head.
  Sleep(abandoned?400:250);
#endif
  std::printf("OPT45_REPLY_PASS mode=%s default_retries=2\n",abandoned?"abandoned":"delayed");
}
