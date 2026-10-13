// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
// Adapted from upstream PR #11 ipc_reply_budget_cases.h and Nightly's
// optimization45_ipc_reply_cases.h. Both retain their original MIT attribution.
template<class B> void replyBudgetCase(const std::wstring& mode) {
  const bool abandoned = mode == L"reply-abandoned", cancelled = mode == L"reply-cancel";
  const bool defaultBudget = mode == L"reply-default-budget" || mode == L"reply-default-budget-long";
  // Windows Sleep(10) can round to about 15.6ms. Keep 3300ms as an observation
  // and use 6500ms to distinguish the old budget within the unchanged 10s cap.
  const uint32_t pauseMS = mode == L"reply-default-budget-long" ? 6500 : (defaultBudget ? 3300 : 120);
  GlobalOptions::retries = defaultBudget ? 300 : 2;
#ifdef REMIX_BRIDGE_CLIENT
  std::atomic<bool> firstSent{false}, early{cancelled};
  bool ownerSucceeded = false;
  std::thread first([&] {
    UID uid;
    { typename B::Command c(Commands::IDirect3DQuery9_GetData, 111);
      uid = static_cast<UID>(c.get_uid()); c.send_data(111);
      require(c.finish() == Result::Success, "owner request finish"); }
    firstSent.store(true);
    if (abandoned || cancelled) { return; }
    Sleep(pauseMS);
    const auto result = B::waitForCommand(Commands::Bridge_Response, 10, nullptr, true, uid, 10000);
    if (result == Result::Success) {
      ownerSucceeded = B::get_data() == 111 && B::pop_front().pHandle == uid;
    }
  });
  while (!firstSent.load()) { std::this_thread::yield(); }
  UID uid;
  { typename B::Command c(Commands::IDirect3DQuery9_GetData, 222);
    uid = static_cast<UID>(c.get_uid()); c.send_data(222);
    require(c.finish() == Result::Success, "second request finish"); }
  const auto before = GetTickCount64();
  const auto result = B::waitForCommand(Commands::Bridge_Response, defaultBudget ? 10 : 1,
                                       cancelled ? &early : nullptr, true, uid, abandoned ? 180 : 10000);
  const auto elapsed = GetTickCount64() - before;
  first.join();
  std::printf("IPC_REPLY_BUDGET_TIMING retries=%u owner_pause_ms=%u elapsed_ms=%llu result=%u\n",
              GlobalOptions::retries, pauseMS, static_cast<unsigned long long>(elapsed), static_cast<unsigned>(result));
  std::fflush(stdout);
  if (abandoned || cancelled) {
    require(result == Result::Failure && !gbBridgeRunning.load(), "missing/cancelled owner did not fail closed");
    require(cancelled ? elapsed < 1000 : elapsed >= 150 && elapsed < 1000, "reply hard deadline/cancellation");
  } else {
    require(result == Result::Success && gbBridgeRunning.load(), "another UID consumed our retry budget");
    require(ownerSucceeded, "delayed owner reply identity");
    require(B::get_data() == 222 && B::pop_front().pHandle == uid, "reply UID/payload ownership");
    require(elapsed < (defaultBudget ? 9500u : 2000u), "reply completion deadline");
  }
#else
  for (unsigned i = 0; i < 2; ++i) {
    const auto request = B::waitForCommand(Commands::Bridge_Any, 2000);
    if (cancelled && request != Result::Success && !B::healthy()) {
      std::puts("IPC_REPLY_CANCEL_PEER_PASS"); return;
    }
    require(request == Result::Success, "reply case request wait");
    const auto h = B::pop_front(); const auto uid = B::get_data(); const auto value = B::get_data(); B::end_read_data();
    require(h.pHandle == value, "request owner");
    typename B::Command c(Commands::Bridge_Response, uid); c.send_data(value);
    const auto sent = c.finish();
    if (cancelled && sent != Result::Success && !B::healthy()) {
      std::puts("IPC_REPLY_CANCEL_PEER_PASS"); return;
    }
    require(sent == Result::Success, "reply publication");
  }
  Sleep(defaultBudget ? pauseMS + 700 : 400);
#endif
  std::puts("IPC_REPLY_BUDGET_PASS"); GlobalOptions::retries = 2;
}
