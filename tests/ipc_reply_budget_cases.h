// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
// Delayed-owner case informed by NPCodex optimization45_ipc_reply_cases.h (MIT).
template<class B> void replyBudgetCase(const std::wstring& mode) {
  const bool abandoned=mode==L"reply-abandoned", cancelled=mode==L"reply-cancel";
  const bool longPause=mode==L"reply-default-budget";
  GlobalOptions::retries=longPause?300:2;
#ifdef REMIX_BRIDGE_CLIENT
  std::atomic<bool> firstSent{false}, early{cancelled};
  std::thread first([&] {
    UID uid;
    {typename B::Command c(Commands::IDirect3DQuery9_GetData,111);uid=static_cast<UID>(c.get_uid());c.send_data(111);
      require(c.finish()==Result::Success,"owner request finish");}
    firstSent.store(true);
    if(abandoned||cancelled) {return;}
    Sleep(longPause?3300:120);
    require(B::waitForCommand(Commands::Bridge_Response,10,nullptr,true,uid,10000)==Result::Success,"delayed owner reply");
    require(B::get_data()==111,"delayed owner payload");B::pop_front();
  });
  while(!firstSent.load()) {std::this_thread::yield();}
  UID uid;
  {typename B::Command c(Commands::IDirect3DQuery9_GetData,222);uid=static_cast<UID>(c.get_uid());c.send_data(222);
    require(c.finish()==Result::Success,"second request finish");}
  const auto before=GetTickCount64();
  const auto result=B::waitForCommand(Commands::Bridge_Response,longPause?10:1,cancelled?&early:nullptr,true,uid,abandoned?180:10000);
  const auto elapsed=GetTickCount64()-before;first.join();
  if(abandoned||cancelled) {
    require(result==Result::Failure&&!gbBridgeRunning.load(),"missing/cancelled owner did not fail closed");
    require(cancelled?elapsed<1000:elapsed>=150&&elapsed<1000,"reply hard deadline/cancellation");
  } else {
    require(result==Result::Success&&gbBridgeRunning.load(),"another UID consumed our retry budget");
    require(B::get_data()==222&&B::pop_front().pHandle==uid,"reply UID/payload ownership");
    require(elapsed<(longPause?8000u:2000u),"reply completion deadline");
  }
#else
  for(unsigned i=0;i<2;++i) {
    const auto request=B::waitForCommand(Commands::Bridge_Any,2000);
    if(cancelled && request!=Result::Success && !B::healthy()) {std::puts("IPC_REPLY_CANCEL_PEER_PASS");return;}
    require(request==Result::Success,"reply case request wait");
    const auto h=B::pop_front();const auto uid=B::get_data();const auto value=B::get_data();B::end_read_data();
    require(h.pHandle==value,"request owner");typename B::Command c(Commands::Bridge_Response,uid);c.send_data(value);
    const auto sent=c.finish();
    if(cancelled && sent!=Result::Success && !B::healthy()) {std::puts("IPC_REPLY_CANCEL_PEER_PASS");return;}
    require(sent==Result::Success,"reply publication");
  }
  Sleep(longPause?4000:400);
#endif
  std::puts("IPC_REPLY_BUDGET_PASS");GlobalOptions::retries=2;
}
