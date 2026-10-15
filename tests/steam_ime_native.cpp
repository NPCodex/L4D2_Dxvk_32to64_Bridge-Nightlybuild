// SPDX-License-Identifier: MIT
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <set>
#include <string>
#include <vector>

static unsigned assertions = 0;
static void require(bool ok, const char* reason) {
  ++assertions;
  if (!ok) {
    std::fprintf(stderr, "IME_NATIVE_FAIL: %s error=%lu\n", reason, GetLastError());
    std::exit(1);
  }
}

#ifdef IME_FAULT_INJECTION
// These adapters exercise failure cleanup in the unchanged production header.
namespace injected {
static std::string mode;
static unsigned initCalls = 0, uninitCalls = 0, getCalls = 0, createCalls = 0;
static unsigned activateCalls = 0, deactivateCalls = 0, releaseCalls = 0;
static std::vector<std::string> events;
static HMODULE ole() { return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(1)); }
static HMODULE ctf() { return reinterpret_cast<HMODULE>(static_cast<uintptr_t>(2)); }

class Manager final : public ITfThreadMgr {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** result) override { *result = nullptr; return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
  ULONG STDMETHODCALLTYPE Release() override { ++releaseCalls; events.emplace_back("release"); return 0; }
  HRESULT STDMETHODCALLTYPE Activate(TfClientId* client) override {
    ++activateCalls; events.emplace_back("activate");
    *client = TF_CLIENTID_NULL;
    if (mode == "activate-fail") { return E_FAIL; }
    *client = 17; return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Deactivate() override { ++deactivateCalls; events.emplace_back("deactivate"); return S_OK; }
  HRESULT STDMETHODCALLTYPE CreateDocumentMgr(ITfDocumentMgr**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE EnumDocumentMgrs(IEnumTfDocumentMgrs**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetFocus(ITfDocumentMgr**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetFocus(ITfDocumentMgr*) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE AssociateFocus(HWND, ITfDocumentMgr*, ITfDocumentMgr**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE IsThreadFocus(BOOL*) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetFunctionProvider(REFCLSID, ITfFunctionProvider**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE EnumFunctionProviders(IEnumTfFunctionProviders**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetGlobalCompartment(ITfCompartmentMgr**) override { return E_NOTIMPL; }
};
static Manager manager;
static HRESULT WINAPI initialize(LPVOID reserved, DWORD flags) {
  ++initCalls; events.emplace_back("initialize");
  require(!reserved && flags == COINIT_APARTMENTTHREADED, "STA initialization parameters");
  return mode == "init-fail" ? E_OUTOFMEMORY : mode == "existing-sta" ? S_FALSE : S_OK;
}
static void WINAPI uninitialize() { ++uninitCalls; events.emplace_back("uninitialize"); }
static HRESULT WINAPI getManager(ITfThreadMgr** result) {
  ++getCalls;
  *result = nullptr;
  if (mode == "get-fail") { return E_FAIL; }
  if (mode == "create-fail" || mode == "missing-create") { return S_FALSE; }
  *result = &manager; return S_OK;
}
static HRESULT WINAPI createManager(ITfThreadMgr** result) {
  ++createCalls;
  *result = nullptr;
  if (mode == "create-fail") { return E_OUTOFMEMORY; }
  *result = &manager; return S_OK;
}
template<class T> static FARPROC procedure(T pointer) {
  FARPROC result = nullptr;
  static_assert(sizeof(result) == sizeof(pointer), "procedure representation");
  std::memcpy(&result, &pointer, sizeof(result)); return result;
}
static HMODULE WINAPI load(LPCWSTR name, HANDLE file, DWORD flags) {
  require(!file && flags == LOAD_LIBRARY_SEARCH_SYSTEM32, "system DLL search is restricted");
  SetLastError(0x5678);
  if (std::wcscmp(name, L"ole32.dll") == 0) {
    events.emplace_back("load-ole"); return mode == "missing-ole" ? nullptr : ole();
  }
  require(std::wcscmp(name, L"msctf.dll") == 0, "known TSF DLL only");
  events.emplace_back("load-ctf"); return mode == "missing-ctf" ? nullptr : ctf();
}
static FARPROC WINAPI resolve(HMODULE module, LPCSTR name) {
  SetLastError(0x5678);
  if (module == ole()) {
    if (std::strcmp(name, "CoInitializeEx") == 0) { return mode == "missing-init" ? nullptr : procedure(&initialize); }
    require(std::strcmp(name, "CoUninitialize") == 0, "known COM export only");
    return mode == "missing-uninit" ? nullptr : procedure(&uninitialize);
  }
  require(module == ctf(), "known TSF module only");
  if (std::strcmp(name, "TF_GetThreadMgr") == 0) { return mode == "missing-get" ? nullptr : procedure(&getManager); }
  require(std::strcmp(name, "TF_CreateThreadMgr") == 0, "known TSF export only");
  return mode == "missing-create" ? nullptr : procedure(&createManager);
}
static BOOL WINAPI unload(HMODULE module) {
  require(module == ctf() || module == ole(), "release owned module only");
  events.emplace_back(module == ctf() ? "unload-ctf" : "unload-ole");
  SetLastError(0x5678); return TRUE;
}
}
#define LoadLibraryExW injected::load
#define GetProcAddress injected::resolve
#define FreeLibrary injected::unload
#endif
#include "steam_ime_native.h"
#ifdef IME_FAULT_INJECTION
#undef LoadLibraryExW
#undef GetProcAddress
#undef FreeLibrary
#endif

#ifndef IME_FAULT_INJECTION
static std::set<HMODULE> modules() {
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  require(snapshot != INVALID_HANDLE_VALUE, "module snapshot");
  MODULEENTRY32W entry {}; entry.dwSize = sizeof(entry);
  std::set<HMODULE> result;
  require(Module32FirstW(snapshot, &entry) != FALSE, "first module");
  do { result.insert(entry.hModule); } while (Module32NextW(snapshot, &entry));
  CloseHandle(snapshot); return result;
}
static HRESULT apartment(APTTYPE* result = nullptr) {
  APTTYPE type {}; APTTYPEQUALIFIER qualifier {};
  const HRESULT hr = CoGetApartmentType(&type, &qualifier);
  if (result) { *result = type; }
  return hr;
}
static void realTest(const std::string& mode) {
  require(apartment() == CO_E_NOTINITIALIZED, "fresh process starts without COM apartment");
  if (mode == "disabled") {
    const auto before = modules();
    unsigned logs = 0;
    SetLastError(0x1234);
    { l4d2_overlay::ImeThread ime(false, [&](const char*) { ++logs; });
      require(!ime.active() && ime.comResult() == E_NOTIMPL, "disabled has no activation");
      require(GetLastError() == 0x1234, "disabled init preserves LastError"); }
    require(GetLastError() == 0x1234, "disabled teardown preserves LastError");
    require(logs == 0 && modules() == before, "disabled has no logs or newly loaded modules");
    require(apartment() == CO_E_NOTINITIALIZED, "disabled leaves COM uninitialized");
    return;
  }
  const bool foreign = mode == "sta" || mode == "mta";
  const bool mta = mode == "mta";
  require(foreign || mode == "fresh" || mode == "nested", "known native mode");
  if (foreign) { require(CoInitializeEx(nullptr, mta ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED) == S_OK, "foreign COM reference acquired"); }
  const HMODULE ctfBefore = GetModuleHandleW(L"msctf.dll");
  unsigned logs = 0;
  SetLastError(0x1234);
  {
    l4d2_overlay::ImeThread ime(true, [&](const char* line) {
      require(std::strstr(line, "L4D2_STEAM_IME event=thread-init") != nullptr, "initialization metadata");
      ++logs; SetLastError(0x9999);
    });
    require(GetLastError() == 0x1234, "native init and log preserve LastError");
    require(logs == 1, "one initialization log");
    require(ime.comResult() == (mta ? RPC_E_CHANGED_MODE : foreign ? S_FALSE : S_OK), "correct apartment ownership result");
    require(ime.active() == !mta, "TSF active only for compatible apartment");
    if (mta) { require(GetModuleHandleW(L"msctf.dll") == ctfBefore, "foreign MTA does not load TSF"); }
    if (mode == "nested") {
      { l4d2_overlay::ImeThread nested(true);
        require(nested.comResult() == S_FALSE && nested.active(), "nested STA and TSF ownership"); }
      require(SUCCEEDED(apartment()) && ime.active(), "nested teardown preserves outer owner");
    }
    SetLastError(0x1234);
  }
  require(GetLastError() == 0x1234, "native teardown preserves LastError");
  if (foreign) {
    APTTYPE type {};
    require(SUCCEEDED(apartment(&type)), "foreign COM reference survives helper teardown");
    require(mta ? type == APTTYPE_MTA : type == APTTYPE_STA || type == APTTYPE_MAINSTA, "foreign apartment type unchanged");
    CoUninitialize();
  }
  require(apartment() == CO_E_NOTINITIALIZED, "all owned COM references balanced");
  require(CoInitializeEx(nullptr, COINIT_MULTITHREADED) == S_OK, "teardown allows fresh opposite apartment");
  CoUninitialize();
}
#else
static void faultTest(const std::string& mode) {
  using namespace injected;
  injected::mode = mode;
  const bool noInit = mode == "missing-ole" || mode == "missing-init" || mode == "missing-uninit";
  const bool comOwned = !noInit && mode != "init-fail";
  const bool haveCtf = comOwned && mode != "missing-ctf";
  const bool haveManager = haveCtf && mode != "create-fail" && mode != "missing-create";
  const bool active = haveManager && mode != "activate-fail";
  const std::set<std::string> known { "missing-ole", "missing-init", "missing-uninit", "init-fail", "missing-ctf",
    "missing-get", "get-fail", "missing-create", "create-fail", "activate-fail", "success", "existing-sta" };
  require(known.count(mode) == 1, "known injected mode");
  unsigned logs = 0;
  SetLastError(0x1234);
  {
    l4d2_overlay::ImeThread ime(true, [&](const char*) { ++logs; SetLastError(0x9999); });
    require(ime.active() == active, "activation failure is nonfatal");
    require(ime.comResult() == (noInit ? E_NOTIMPL : mode == "init-fail" ? E_OUTOFMEMORY : mode == "existing-sta" ? S_FALSE : S_OK), "injected COM result");
    require(GetLastError() == 0x1234, "failure setup preserves LastError");
  }
  require(GetLastError() == 0x1234 && logs == 1, "failure cleanup and logging preserve LastError");
  require(initCalls == (noInit ? 0u : 1u), "initialize requires both COM exports");
  require(uninitCalls == (comOwned ? 1u : 0u), "balance only successful COM reference");
  require(activateCalls == (haveManager ? 1u : 0u), "activate only a manager reference");
  require(deactivateCalls == (active ? 1u : 0u), "deactivate only successful activation");
  require(releaseCalls == (haveManager ? 1u : 0u), "release manager even after failed activation");
  require(getCalls == (haveCtf && mode != "missing-get" ? 1u : 0u), "query existing manager when available");
  require(createCalls == (haveCtf && (mode == "missing-get" || mode == "get-fail" || mode == "create-fail") ? 1u : 0u), "create only when no existing manager");
  std::vector<std::string> expected;
  expected.emplace_back("load-ole");
  if (!noInit) { expected.emplace_back("initialize"); }
  if (comOwned) { expected.emplace_back("load-ctf"); }
  if (haveManager) { expected.emplace_back("activate"); }
  if (active) { expected.emplace_back("deactivate"); }
  if (haveManager) { expected.emplace_back("release"); }
  if (comOwned) { expected.emplace_back("uninitialize"); }
  if (haveCtf) { expected.emplace_back("unload-ctf"); }
  if (mode != "missing-ole") { expected.emplace_back("unload-ole"); }
  require(events == expected, "owned teardown follows activation/reference/module order");
}
#endif

int main(int argc, char** argv) {
  require(argc == 2, "test mode required");
#ifdef IME_FAULT_INJECTION
  faultTest(argv[1]);
  const char* coverage = "injected-system-boundaries";
#else
  realTest(argv[1]);
  const char* coverage = "real-COM-TSF";
#endif
  std::printf("IME_NATIVE_PASS mode=%s bits=%zu assertions=%u coverage=%s\n", argv[1], sizeof(void*) * 8, assertions, coverage);
  return 0;
}
