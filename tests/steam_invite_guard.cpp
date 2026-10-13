// SPDX-License-Identifier: MIT
// Targeted old-condition control for the production x86 GameUI caller guard.
#include "steam_web_diagnostics.h"
#include <cstdio>
#include <cstdlib>
#include <string>
namespace l4d2_web { struct NativeTest {
  static bool attach(void* utils, void* friends) { return Diagnostics::attach(utils, friends); }
  static void menu(void* base) { Diagnostics::menu = static_cast<HMODULE>(base); Diagnostics::validMenu = true; }
}; }
static void require(bool condition, const char* message) {
  if (!condition) { std::fprintf(stderr, "FAIL invite guard setup: %s\n", message); std::exit(2); }
}
static void* utilsTable[18]{}; static void* friendsTable[31]{};
struct Object { void** table; }; static Object utils{utilsTable}, friends{friendsTable};
static unsigned calls = 0;
static bool __fastcall availability(void* self, void*) { require(self == &utils, "this pointer"); ++calls; SetLastError(0x1234); return false; }
static void __fastcall web(void*, void*, const char*, int) {}
static void __fastcall overlay(void*, void*, const char*) {}
int main() {
  using D = l4d2_web::Diagnostics;
  utilsTable[17] = reinterpret_cast<void*>(&availability);
  friendsTable[30] = reinterpret_cast<void*>(&web); friendsTable[28] = reinterpret_cast<void*>(&overlay);
  require(l4d2_web::NativeTest::attach(&utils, &friends), "attach actual observer");
  const std::string name = "Local\\L4D2InviteGuard_" + std::to_string(GetCurrentProcessId());
  require(l4d2_workshop::publisher.publisher(name.c_str()), "publisher");
  require(l4d2_workshop::reader.reader(name.c_str(), GetCurrentProcess()), "reader");
  auto* fixture = static_cast<BYTE*>(VirtualAlloc(nullptr, 0x70000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
  require(fixture != nullptr, "allocate authored callers");
  const BYTE thunk[] = {0x8b,0x01,0x8b,0x40,0x44,0xff,0xd0,0xc3};
  const uintptr_t sites[] = {0x199ea,0x19aca,0x6ea9a,0x11a03};
  for (const auto site : sites) std::memcpy(fixture + site - 7, thunk, sizeof(thunk));
  require(FlushInstructionCache(GetCurrentProcess(), fixture, 0x70000) != FALSE, "flush instruction cache");
  l4d2_web::NativeTest::menu(fixture); D::setProxy(true);
  l4d2_workshop::publish(true, 1, GetTickCount64());
  using Guard = bool (__thiscall*)(void*);
  for (unsigned i = 0; i < 4; ++i) {
    const unsigned before = calls;
    const bool selected = reinterpret_cast<Guard>(fixture + sites[i] - 7)(&utils);
    require(calls == before + 1 && GetLastError() == 0x1234, "exact original call and error preservation");
    if (i == 3 && !selected) {
      D::stop(); l4d2_workshop::reader.close(); l4d2_workshop::publisher.close();
      require(VirtualFree(fixture, 0, MEM_RELEASE) != FALSE, "negative cleanup");
      std::fprintf(stderr, "FAIL invitation helper availability rejected at 0x11a03; three prior guard sites passed\n");
      return 43;
    }
    require(selected, "existing Workshop guard lost availability");
  }
  D::stop(); l4d2_workshop::reader.close(); l4d2_workshop::publisher.close();
  require(VirtualFree(fixture, 0, MEM_RELEASE) != FALSE, "cleanup");
  std::puts("PASS invitation helper: real x86 thiscall, four exact return sites, original once, preserved LastError");
  return 0;
}
