// SPDX-License-Identifier: MIT
// Exercise unchanged production initialization with recording boundary adapters.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <remixapi/bridge_remix_api.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <type_traits>

static unsigned assertions = 0;
static void check(bool value, const char* message) {
  ++assertions;
  if (!value) throw std::runtime_error(message);
}
struct Logger {
  template<class T> static void err(const T&) {}
  template<class T> static void info(const T&) {}
  template<class T> static void warn(const T&) {}
};
template<class... T> static std::string format_string(const char* text, T...) { return text; }
struct GlobalOptions {
  inline static bool expose = false;
  static bool getExposeRemixApi() { return expose; }
  static uint32_t getAckTimeout() { return 137; }
};
struct ServerOptions {
  inline static bool vanilla = true;
  static bool getUseVanillaDxvk() { return vanilla; }
};
struct BridgeState {
  enum class ProcessState { NotStarted, Starting, Running, Exited };
  inline static ProcessState state = ProcessState::NotStarted;
  static ProcessState getServerState() { return state; }
};
namespace Commands { enum Command { RemixApi_GetInitializationStatus, Bridge_Response }; }
using UID = uint32_t;
UID currentUID = 0;
enum class Result { Success, Timeout, Failure };
static void dispatchStatus();
struct ServerMessage {
  inline static unsigned replies = 0;
  inline static UID uid = 0;
  inline static uint32_t status = 0;
  ServerMessage(Commands::Command command, UID value) {
    check(command == Commands::Bridge_Response, "host status response command changed");
    ++replies; uid = value;
  }
  void send_data(uint32_t value) { status = value; }
};
struct ClientMessage {
  inline static unsigned requests = 0;
  inline static UID nextUID = 0x76543210;
  UID uid;
  ClientMessage(Commands::Command command) : uid(nextUID++) {
    check(command == Commands::RemixApi_GetInitializationStatus, "client query command changed");
    ++requests;
    currentUID = uid;
  }
  UID get_uid() const { return uid; }
};
struct DeviceBridge {
  inline static unsigned waits = 0, reads = 0, pops = 0;
  inline static Result result = Result::Success;
  static Result waitForCommand(Commands::Command command, uint32_t timeout, void* data, bool match, UID uid) {
    ++waits;
    check(command == Commands::Bridge_Response && timeout == GlobalOptions::getAckTimeout() &&
          data == nullptr && match && uid == currentUID, "client response wait lost timeout or UID correlation");
    if (result != Result::Success) return result;
    dispatchStatus();
    check(ServerMessage::uid == uid, "host response UID does not match request");
    return Result::Success;
  }
  static uint32_t get_data() { ++reads; return ServerMessage::status; }
  static void pop_front() { ++pops; }
};
namespace remixapi {
  uint64_t g_appRemixApiVersion = 0;
  bool g_bInterfaceInitialized = false;
  bool g_remix_initialized = false;
  remixapi_Interface g_remix = {};
  HMODULE g_remix_dll = nullptr;
  // Non-null sentinels distinguish exposed slots from zeroed unsupported slots.
  // They are never invoked: dispatch implementations are outside this contract.
#define ENTRY_SENTINEL(name, member) \
  decltype(remixapi_Interface{}.member) name = reinterpret_cast<decltype(remixapi_Interface{}.member)>(uintptr_t(1))
  ENTRY_SENTINEL(remixapi_CreateMaterial, CreateMaterial);
  ENTRY_SENTINEL(remixapi_DestroyMaterial, DestroyMaterial);
  ENTRY_SENTINEL(remixapi_CreateMesh, CreateMesh);
  ENTRY_SENTINEL(remixapi_DestroyMesh, DestroyMesh);
  ENTRY_SENTINEL(remixapi_DrawInstance, DrawInstance);
  ENTRY_SENTINEL(remixapi_CreateLight, CreateLight);
  ENTRY_SENTINEL(remixapi_DestroyLight, DestroyLight);
  ENTRY_SENTINEL(remixapi_DrawLightInstance, DrawLightInstance);
  ENTRY_SENTINEL(remixapi_SetConfigVariable, SetConfigVariable);
  ENTRY_SENTINEL(remixapi_dxvk_CreateD3D9, dxvk_CreateD3D9);
  ENTRY_SENTINEL(remixapi_dxvk_RegisterD3D9Device, dxvk_RegisterD3D9Device);
#undef ENTRY_SENTINEL
}
#define ASSERT_REMIXAPI_PFN_TYPE(name) static_assert(std::is_same<decltype(&name), PFN_##name>::value)

struct Loader {
  inline static bool pathOk = true, moduleOk = true, exportOk = true, deviceOk = true;
  inline static unsigned loads = 0, creates = 0, remixLoads = 0, queries = 0, fixes = 0;
  inline static std::string path;
  inline static DWORD flags = 0;
  inline static remixapi_ErrorCode status = REMIXAPI_ERROR_CODE_SUCCESS;
};
static DWORD adapterGetModuleFileNameA(HMODULE, LPSTR dest, DWORD size) {
  const char path[] = "C:\\fixture\\L4D2Bridge.exe";
  if (!Loader::pathOk) return 0;
  check(sizeof(path) < size, "test host path does not fit");
  std::memcpy(dest, path, sizeof(path));
  return sizeof(path) - 1;
}
static HMODULE adapterLoadLibraryExA(LPCSTR path, HANDLE, DWORD flags) {
  ++Loader::loads; Loader::path = path; Loader::flags = flags;
  return Loader::moduleOk ? reinterpret_cast<HMODULE>(uintptr_t(1)) : nullptr;
}
using D3DC9 = IDirect3D9* (__stdcall*)(UINT);
static IDirect3D9* __stdcall adapterCreate9(UINT version) {
  ++Loader::creates;
  check(version == D3D_SDK_VERSION, "D3D SDK version changed");
  return Loader::deviceOk ? reinterpret_cast<IDirect3D9*>(uintptr_t(1)) : nullptr;
}
static FARPROC adapterGetProcAddress(HMODULE, LPCSTR name) {
  if (std::strcmp(name, "Direct3DCreate9") == 0)
    return Loader::exportOk ? reinterpret_cast<FARPROC>(&adapterCreate9) : nullptr;
  ++Loader::queries;
  return nullptr;
}
static remixapi_ErrorCode adapterLoadRemix(const wchar_t* path, remixapi_Interface*, HMODULE*) {
  ++Loader::remixLoads;
  check(std::wcscmp(path, L"d3d9.dll") == 0, "Remix initialization library changed");
  check(Loader::creates == 1, "Remix initialization happened before D3D creation");
  return Loader::status;
}
HMODULE ghModule = nullptr;
IDirect3D9* gpD3D = nullptr;
bool bDxvkModuleLoaded = false;
static void FixD3DRecordHRESULT(const char*, HMODULE) { ++Loader::fixes; }
static void CheckD3D9Type(HMODULE) {}
namespace version {
  enum Feature { MessageChannel, FileSys, nFeatures };
  using QueryFunc = uint64_t (*)(Feature);
  constexpr const char* QueryFuncName = "fixtureFeatureQuery";
  constexpr uint64_t messageChannelV = 1, fileSysV = 1;
}
#define GetModuleFileNameA adapterGetModuleFileNameA
#define LoadLibraryExA adapterLoadLibraryExA
#define GetProcAddress adapterGetProcAddress
#define remixapi_lib_loadRemixDllAndInitialize adapterLoadRemix
#include "production_remix_init.h"
#undef GetModuleFileNameA
#undef LoadLibraryExA
#undef GetProcAddress
#undef remixapi_lib_loadRemixDllAndInitialize

static void reset() {
  GlobalOptions::expose = false; ServerOptions::vanilla = true;
  BridgeState::state = BridgeState::ProcessState::NotStarted;
  ClientMessage::requests = 0; ServerMessage::replies = 0;
  DeviceBridge::waits = DeviceBridge::reads = DeviceBridge::pops = 0;
  DeviceBridge::result = Result::Success;
  remixapi::g_appRemixApiVersion = 0; remixapi::g_bInterfaceInitialized = false;
  remixapi::g_remix_initialized = false; remixapi::g_initStatus = REMIXAPI_ERROR_CODE_NOT_INITIALIZED;
  Loader::pathOk = Loader::moduleOk = Loader::exportOk = Loader::deviceOk = true;
  Loader::loads = Loader::creates = Loader::remixLoads = Loader::queries = Loader::fixes = 0;
  Loader::status = REMIXAPI_ERROR_CODE_SUCCESS;
  ghModule = nullptr; gpD3D = nullptr; bDxvkModuleLoaded = false;
}
static void noIPC() {
  check(ClientMessage::requests == 0 && DeviceBridge::waits == 0 && DeviceBridge::reads == 0 &&
        DeviceBridge::pops == 0 && ServerMessage::replies == 0, "early rejection emitted IPC");
}
static void unchanged(const remixapi_Interface& out, const remixapi_Interface& before) {
  check(std::memcmp(&out, &before, sizeof(out)) == 0 && !remixapi::g_bInterfaceInitialized &&
        remixapi::g_appRemixApiVersion == 0, "failed initialization exposed interface or version");
}
int main() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  try {
    const auto version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);
    remixapi_InitializeLibraryInfo info{REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO, nullptr, version};
    remixapi_Interface before{};
    before.CreateMaterial = remixapi::remixapi_CreateMaterial;
    auto out = before;
    reset();
    check(remixapi::g_initStatus == REMIXAPI_ERROR_CODE_NOT_INITIALIZED, "initial Host status must be not initialized");
    check(remixapi::remixapi_InitializeLibrary(&info, &out) == REMIXAPI_ERROR_CODE_NOT_INITIALIZED, "disabled API status changed");
    noIPC(); unchanged(out, before);
    GlobalOptions::expose = true;
    check(remixapi::remixapi_InitializeLibrary(nullptr, &out) == REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS, "null info accepted");
    check(remixapi::remixapi_InitializeLibrary(&info, nullptr) == REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS, "null output accepted");
    auto bad = info; bad.sType = static_cast<remixapi_StructType>(-1);
    check(remixapi::remixapi_InitializeLibrary(&bad, &out) == REMIXAPI_ERROR_CODE_INVALID_ARGUMENTS, "invalid info type accepted");
    bad = info; bad.version = REMIXAPI_VERSION_MAKE(255, 0, 0);
    check(remixapi::remixapi_InitializeLibrary(&bad, &out) == REMIXAPI_ERROR_CODE_INCOMPATIBLE_VERSION, "incompatible API version accepted");
    noIPC(); unchanged(out, before);
    for (auto state : {BridgeState::ProcessState::NotStarted, BridgeState::ProcessState::Starting, BridgeState::ProcessState::Exited}) {
      BridgeState::state = state;
      const auto result = remixapi::remixapi_InitializeLibrary(&info, &out);
      check(ClientMessage::requests == 0, "before-start request emitted IPC");
      check(result == REMIXAPI_ERROR_CODE_NOT_INITIALIZED, "non-running Host was accepted");
      noIPC(); unchanged(out, before);
    }
    for (auto failure : {Result::Timeout, Result::Failure}) {
      reset(); GlobalOptions::expose = true; BridgeState::state = BridgeState::ProcessState::Running;
      DeviceBridge::result = failure; out = before;
      check(remixapi::remixapi_InitializeLibrary(&info, &out) == REMIXAPI_ERROR_CODE_GENERAL_FAILURE, "failed IPC wait was accepted");
      check(ClientMessage::requests == 1 && DeviceBridge::waits == 1 && DeviceBridge::reads == 0 &&
            DeviceBridge::pops == 0 && ServerMessage::replies == 0, "failed wait consumed a response");
      unchanged(out, before);
    }
    for (bool vanilla : {true, false}) {
      reset(); ServerOptions::vanilla = vanilla;
      check(InitializeD3D(), "API-disabled D3D initialization failed");
      check(Loader::creates == 1 && Loader::remixLoads == 0 && Loader::queries == 0 &&
            remixapi::g_initStatus == REMIXAPI_ERROR_CODE_NOT_INITIALIZED && !remixapi::g_remix_initialized,
            "API-disabled backend attempted Remix initialization");
      if (vanilla) {
        const auto filename = std::filesystem::path(Loader::path).filename().string();
        check(filename == (sizeof(void*) == 8 ? "d3d9vk_x64.dll" : "d3d9vk_x86.dll"), "default backend architecture changed");
        check(Loader::fixes == 0 && Loader::flags == (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32),
              "default backend loader contract changed");
      }
      for (auto status : {REMIXAPI_ERROR_CODE_SUCCESS, REMIXAPI_ERROR_CODE_NOT_INITIALIZED,
                          REMIXAPI_ERROR_CODE_GENERAL_FAILURE, REMIXAPI_ERROR_CODE_GET_PROC_ADDRESS_FAILURE}) {
        reset(); ServerOptions::vanilla = vanilla; GlobalOptions::expose = true; Loader::status = status;
        check(InitializeD3D(), "Remix status changed successful D3D startup result");
        check(Loader::remixLoads == 1 && remixapi::g_initStatus == status, "host initialization result was not propagated");
        check(remixapi::g_remix_initialized == (status == REMIXAPI_ERROR_CODE_SUCCESS), "Host initialized flag disagrees with status");
        BridgeState::state = BridgeState::ProcessState::Running; out = before;
        check(remixapi::remixapi_InitializeLibrary(&info, &out) == status, "client did not return Host initialization status");
        check(ClientMessage::requests == 1 && DeviceBridge::waits == 1 && DeviceBridge::reads == 1 &&
              DeviceBridge::pops == 1 && ServerMessage::replies == 1, "initialization query was not consumed exactly once");
        if (status == REMIXAPI_ERROR_CODE_SUCCESS) {
          check(remixapi::g_bInterfaceInitialized && remixapi::g_appRemixApiVersion == version &&
                out.CreateMaterial == remixapi::remixapi_CreateMaterial && out.DestroyMaterial == remixapi::remixapi_DestroyMaterial &&
                out.CreateMesh == remixapi::remixapi_CreateMesh && out.DestroyMesh == remixapi::remixapi_DestroyMesh &&
                out.DrawInstance == remixapi::remixapi_DrawInstance && out.CreateLight == remixapi::remixapi_CreateLight &&
                out.DestroyLight == remixapi::remixapi_DestroyLight && out.DrawLightInstance == remixapi::remixapi_DrawLightInstance &&
                out.SetConfigVariable == remixapi::remixapi_SetConfigVariable && out.dxvk_CreateD3D9 == remixapi::remixapi_dxvk_CreateD3D9 &&
                out.dxvk_RegisterD3D9Device == remixapi::remixapi_dxvk_RegisterD3D9Device && out.Startup == nullptr && out.Shutdown == nullptr,
                "successful initialization did not expose exactly supported API slots");
        } else unchanged(out, before);
      }
    }
    for (unsigned failure = 0; failure < 4; ++failure) {
      reset(); GlobalOptions::expose = true;
      if (failure == 0) Loader::pathOk = false;
      if (failure == 1) Loader::moduleOk = false;
      if (failure == 2) Loader::exportOk = false;
      if (failure == 3) Loader::deviceOk = false;
      check(!InitializeD3D(), "backend startup failure was accepted");
      check(Loader::remixLoads == 0 && remixapi::g_initStatus == REMIXAPI_ERROR_CODE_NOT_INITIALIZED &&
            !remixapi::g_remix_initialized, "failed backend startup exposed Remix");
    }
    std::printf("REMIX_INIT_PASS bits=%u assertions=%u disabled/arguments/before-start/timeout/failure/status/UID/default-backend\n",
                unsigned(sizeof(void*) * 8), assertions);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "REMIX_INIT_FAIL: %s\n", error.what());
    return 1;
  }
}
