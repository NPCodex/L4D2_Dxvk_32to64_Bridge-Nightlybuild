// Native regression for production Host device destruction and live-command gate.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>

static unsigned assertions = 0;
static void check(bool value, const char* message) {
  ++assertions;
  if (!value) { throw std::runtime_error(message); }
}
constexpr uint32_t IDirect3DDevice9Ex_Destroy = 1;
constexpr uint32_t IDirect3DDevice9Ex_TestCooperativeLevel = 2;
namespace Commands { constexpr uint32_t Bridge_Response = 3; }
constexpr uint32_t currentUID = 0x12345678;
struct Header { uint32_t command = 0, pHandle = 0; } rpcHeader;
struct Backend final : IUnknown {
  LONG remaining = 1;
  bool negativeRelease = false;
  unsigned releases = 0, liveCalls = 0;
  HRESULT liveResult = S_OK;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++remaining); }
  ULONG STDMETHODCALLTYPE Release() override {
    ++releases;
    check(releases <= 8, "safeDestroy failed to stop at nonpositive refcount");
    return negativeRelease ? static_cast<ULONG>(-1) : static_cast<ULONG>(--remaining);
  }
  HRESULT TestCooperativeLevel() { ++liveCalls; return liveResult; }
};
std::unordered_map<uint32_t, Backend*> gpD3DDevices;
struct DeviceBridge {
  inline static unsigned faults = 0;
  inline static uint32_t lastCommand = 0, lastUID = 0;
  inline static std::string lastReason;
  static void reportFault(const char* reason, uint32_t command, uint32_t uid) {
    ++faults; lastCommand = command; lastUID = uid; lastReason = reason;
  }
};
struct ServerMessage {
  inline static unsigned replies = 0;
  inline static HRESULT result = E_FAIL;
  ServerMessage(uint32_t command, uint32_t uid) {
    check(command == Commands::Bridge_Response && uid == currentUID, "live response identity changed");
    ++replies;
  }
  void send_data(HRESULT value) { result = value; }
};
#include "production_device_destroy.h"

static void destroy(uint32_t id) { rpcHeader = {IDirect3DDevice9Ex_Destroy, id}; dispatchDestroy(); }
static void live(uint32_t id) { rpcHeader = {IDirect3DDevice9Ex_TestCooperativeLevel, id}; dispatchLiveCall(); }
static void checkLiveFault(unsigned before, unsigned replies) {
  check(DeviceBridge::faults == before + 1, "unknown live command did not fault exactly once");
  check(DeviceBridge::lastReason == "host-resource-handle" &&
        DeviceBridge::lastCommand == IDirect3DDevice9Ex_TestCooperativeLevel &&
        DeviceBridge::lastUID == currentUID, "live fault attribution changed");
  check(ServerMessage::replies == replies, "unknown live command replied as successful");
}
int main() {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  try {
    Backend initial;
    Backend survivor;
    gpD3DDevices[7] = &initial;
    gpD3DDevices[13] = &survivor;
    destroy(7);
    check(initial.releases == 1 && gpD3DDevices.size() == 1 && DeviceBridge::faults == 0,
          "valid device cleanup changed");
    destroy(7);
    check(DeviceBridge::faults == 0, "duplicate device destroy faulted channel");
    check(initial.releases == 1 && gpD3DDevices.size() == 1, "duplicate cleanup released backend or inserted handle");
    destroy(0x7fffffff);
    check(DeviceBridge::faults == 0 && gpD3DDevices.size() == 1, "absent cleanup faulted or inserted handle");
    gpD3DDevices[9] = nullptr;
    destroy(9);
    check(DeviceBridge::faults == 0 && gpD3DDevices.size() == 1, "null backend cleanup left stale handle or faulted");
    live(13);
    check(survivor.liveCalls == 1 && survivor.releases == 0 && initial.liveCalls == 0 &&
          ServerMessage::replies == 1 && ServerMessage::result == S_OK && DeviceBridge::faults == 0,
          "cleanup interrupted unrelated live device");
    destroy(13);
    check(survivor.releases == 1 && gpD3DDevices.empty() && DeviceBridge::faults == 0,
          "surviving device cleanup changed");

    Backend recreated;
    recreated.liveResult = E_FAIL;
    gpD3DDevices[7] = &recreated;
    live(7);
    check(recreated.liveCalls == 1 && initial.liveCalls == 0 && ServerMessage::result == E_FAIL &&
          ServerMessage::replies == 2 && DeviceBridge::faults == 0,
          "recreated live device dispatch or HRESULT changed");
    destroy(7); destroy(7);
    check(recreated.releases == 1 && initial.releases == 1 && DeviceBridge::faults == 0,
          "recreated device cleanup reached old backend");

    Backend multiple;
    multiple.remaining = 3;
    gpD3DDevices[11] = &multiple;
    destroy(11);
    check(multiple.releases == 3 && gpD3DDevices.empty(), "safeDestroy did not preserve release-until-zero");
    Backend negative;
    negative.negativeRelease = true;
    gpD3DDevices[12] = &negative;
    destroy(12);
    check(negative.releases == 1 && gpD3DDevices.empty() && DeviceBridge::faults == 0,
          "safeDestroy did not preserve negative refcount handling");

    // Intentional live-command faults are last; the adapter does not exercise
    // real IPC channel shutdown or claim that a faulted channel can recover.
    const auto replies = ServerMessage::replies;
    auto faults = DeviceBridge::faults;
    live(7); checkLiveFault(faults, replies);
    check(gpD3DDevices.empty() && initial.liveCalls == 0 && recreated.liveCalls == 1,
          "stale live call mutated map or reached backend");
    gpD3DDevices[9] = nullptr;
    faults = DeviceBridge::faults;
    live(9); checkLiveFault(faults, replies);
    check(gpD3DDevices.size() == 1 && gpD3DDevices.at(9) == nullptr, "null live call mutated map");
    std::printf("DEVICE_DESTROY_PASS bits=%u assertions=%u normal/duplicate/absent/null/recreated/multi-ref/negative-ref/live-gate\n",
                unsigned(sizeof(void*) * 8), assertions);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "DEVICE_DESTROY_FAIL: %s\n", error.what());
    return 1;
  }
}
