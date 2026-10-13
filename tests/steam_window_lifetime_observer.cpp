// SPDX-License-Identifier: MIT
// Exercise the actual observer from an unloadable DLL, not the test EXE.
#include "steam_window_diagnostics.h"
#include <cstring>
static unsigned logCalls = 0;
static bool attached = false;
static void logLine(const char* line) {
  ++logCalls;
  if (std::strstr(line, "event=attached mask=7")) attached = true;
}
extern "C" __declspec(dllexport) bool LifetimeStart(HWND game, const wchar_t* renderer) {
  l4d2_steam_window::Diagnostics::poll(true, game, nullptr, logLine, renderer, false, "lifetime-fixture");
  return attached;
}
extern "C" __declspec(dllexport) void LifetimeStop() {
  l4d2_steam_window::Diagnostics::stop();
}
extern "C" __declspec(dllexport) unsigned LifetimeLogCalls() { return logCalls; }
