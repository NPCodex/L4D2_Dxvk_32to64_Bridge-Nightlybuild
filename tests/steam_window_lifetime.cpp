// SPDX-License-Identifier: MIT
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void require(bool value, const char* message) {
  if (!value) { std::fprintf(stderr, "FAIL observer lifecycle: %s error=%lu\n", message, GetLastError()); std::exit(2); }
}
static void** findSlot(HMODULE module, const char* name) {
  const auto base = reinterpret_cast<BYTE*>(module);
  const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
  auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  for (; descriptor->Name; ++descriptor) {
    if (!descriptor->OriginalFirstThunk) continue;
    auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
    auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots) {
      if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)
        && std::strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, name) == 0)
        return reinterpret_cast<void**>(&slots->u1.Function);
    }
  }
  return nullptr;
}
int wmain(int argc, wchar_t** argv) {
  require(argc == 3, "renderer and observer paths required");
  const HMODULE renderer = LoadLibraryW(argv[1]); require(renderer != nullptr, "load renderer");
  const HMODULE observer = LoadLibraryW(argv[2]); require(observer != nullptr, "load observer");
  using Start = bool (*)(HWND, const wchar_t*);
  using Stop = void (*)();
  using Count = unsigned (*)();
  using Window = HWND (WINAPI*)();
  const auto start = reinterpret_cast<Start>(GetProcAddress(observer, "LifetimeStart"));
  const auto stop = reinterpret_cast<Stop>(GetProcAddress(observer, "LifetimeStop"));
  const auto count = reinterpret_cast<Count>(GetProcAddress(observer, "LifetimeLogCalls"));
  require(start && stop && count, "observer exports");
  const HWND game = CreateWindowExW(0, L"STATIC", L"observer lifetime", WS_OVERLAPPED, 0, 0, 20, 20,
    nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  require(game != nullptr, "create window");
  const char* names[] = {"GetActiveWindow", "GetForegroundWindow", "SetWindowsHookExW"};
  void** slots[3]{}; void* originals[3]{}; void* wrappers[3]{};
  for (unsigned i = 0; i < 3; ++i) {
    slots[i] = findSlot(renderer, names[i]); require(slots[i] != nullptr, "renderer named IAT"); originals[i] = *slots[i];
  }
  require(start(game, argv[1]), "actual observer attaches all three imports");
  for (unsigned i = 0; i < 3; ++i) {
    wrappers[i] = *slots[i]; require(wrappers[i] != originals[i], "renderer slot replaced");
    HMODULE owner = nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(wrappers[i]), &owner) && owner == observer, "wrapper belongs to separately loaded observer DLL");
  }
  const auto retained = reinterpret_cast<Window>(wrappers[0]);
  const auto probe = reinterpret_cast<Window>(GetProcAddress(renderer, "ProbeActive")); require(probe != nullptr, "renderer export");
  const unsigned beforeProbe = count(); probe(); require(count() > beforeProbe, "wrapper executes before stop");
  stop();
  for (unsigned i = 0; i < 3; ++i) require(*slots[i] == originals[i], "stop restores renderer imports");
  const unsigned stoppedLogs = count();
  require(FreeLibrary(observer) != FALSE, "release caller's observer reference");
  // Query by the existing module name without taking a reference. Never call a
  // saved pointer if the negative-control DLL has unloaded.
  const HMODULE remaining = GetModuleHandleW(argv[2]);
  if (!remaining) {
    require(DestroyWindow(game) != FALSE, "destroy negative-control window");
    require(FreeLibrary(renderer) != FALSE, "release renderer");
    std::fprintf(stderr, "FAIL observer lifecycle: observer unloaded after stop/FreeLibrary; retained callback not invoked\n");
    return 42;
  }
  require(remaining == observer, "same observer remains mapped");
  HMODULE owner = nullptr;
  require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    reinterpret_cast<LPCWSTR>(wrappers[0]), &owner) && owner == remaining, "saved callback remains mapped in pinned observer");
  SetLastError(0x5151); const HWND expected = GetActiveWindow(); const DWORD expectedError = GetLastError();
  SetLastError(0x5151); require(retained() == expected && GetLastError() == expectedError, "retained callback delegates after stop/unload request");
  require(count() == stoppedLogs, "retained callback emits no stopped diagnostics");
  require(DestroyWindow(game) != FALSE && FreeLibrary(renderer) != FALSE, "normal cleanup");
  std::printf("PASS observer lifecycle bits=%zu: separate DLL, attach, restore, FreeLibrary, pinned retained callback, no stopped logging\n", sizeof(void*) * 8);
  return 0;
}
