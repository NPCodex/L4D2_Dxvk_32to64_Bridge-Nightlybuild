// SPDX-License-Identifier: MIT
// Actual material source runs with recording IPC and backend boundaries.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static unsigned assertions = 0;
static void check(bool condition, const char* message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}
struct GuardedEnd {
  uint8_t* allocation = nullptr;
  uint8_t* data = nullptr;
  size_t size;
  explicit GuardedEnd(size_t bytes) : size(bytes) {
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    const size_t accessible = ((bytes + page - 1) / page) * page;
    allocation = static_cast<uint8_t*>(VirtualAlloc(nullptr, accessible + page,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(allocation != nullptr, "VirtualAlloc failed");
    DWORD old = 0;
    check(VirtualProtect(allocation + accessible, page, PAGE_NOACCESS, &old) != FALSE,
          "VirtualProtect failed");
    data = allocation + accessible - bytes;
    memset(data, 0xcc, bytes);
  }
  ~GuardedEnd() { if (allocation) VirtualFree(allocation, 0, MEM_RELEASE); }
  GuardedEnd(const GuardedEnd&) = delete;
};
static LONG WINAPI guardFailure(EXCEPTION_POINTERS* exception) {
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    fputs("REMIX_MATERIAL_FAIL: guard page access\n", stderr); fflush(stderr);
    ExitProcess(1);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

#include "util_remixapi.h"
#include "production_material_serializers.h"
using namespace bridge_util;
using namespace remixapi::util;

struct Logger {
  static void warn(const char*) { throw std::runtime_error("unexpected production warning"); }
  static void err(const char*) { throw std::runtime_error("unexpected production error"); }
};
namespace Commands { enum Command : uint32_t { RemixApi_CreateMaterial = 0xabc031, FollowingCommand = 0xc071a55 }; }
struct Packet { uint32_t kind; std::vector<uint8_t> bytes; };
static std::vector<Packet> packets;
static size_t readIndex = 0;
static std::vector<std::unique_ptr<GuardedEnd>> decodedStorage;
static uint32_t currentVersion = 0, currentScale = 0, currentPaths = 0;
static unsigned backendCalls = 0;
static uint32_t lastBridgeHandle = 0;

struct ClientMessage {
  explicit ClientMessage(Commands::Command command) {
    check(command == Commands::RemixApi_CreateMaterial, "unexpected material command");
  }
  void send_data(uint32_t value) {
    Packet packet{1, std::vector<uint8_t>(4)};
    memcpy(packet.bytes.data(), &value, 4); packets.push_back(std::move(packet));
  }
  void send_data(uint32_t bytes, const void* data) {
    Packet packet{2, std::vector<uint8_t>(bytes)};
    memcpy(packet.bytes.data(), data, bytes); packets.push_back(std::move(packet));
  }
};
struct DeviceBridge {
  static uint32_t get_data() {
    check(readIndex < packets.size(), "read past command boundary");
    auto& packet = packets[readIndex++];
    check(packet.kind == 1 && packet.bytes.size() == 4, "scalar packet framing mismatch");
    uint32_t value; memcpy(&value, packet.bytes.data(), 4); return value;
  }
  static uint32_t get_data(void** data) {
    check(readIndex < packets.size(), "read past blob boundary");
    auto& packet = packets[readIndex++];
    check(packet.kind == 2 && packet.bytes.size() >= 4, "blob packet framing mismatch");
    uint32_t declared; memcpy(&declared, packet.bytes.data(), 4);
    check(declared == packet.bytes.size(), "serialized size differs from IPC payload");
    auto storage = std::make_unique<GuardedEnd>(packet.bytes.size());
    memcpy(storage->data, packet.bytes.data(), packet.bytes.size());
    *data = storage->data; decodedStorage.push_back(std::move(storage));
    return declared;
  }
};
namespace remixapi { remixapi_Interface g_remix{}; }
#include "production_material_path.h"

static remixapi_MaterialInfo baseFixture() {
  remixapi_MaterialInfo value{};
  value.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
  value.hash = 0xfedcba9876543210ull;
  value.albedoTexture = currentPaths ? L"a\\base.dds" : nullptr;
  value.normalTexture = currentPaths ? L"n\\\u732b.dds" : nullptr;
  value.tangentTexture = nullptr;
  value.emissiveTexture = currentPaths ? L"" : nullptr;
  value.emissiveIntensity = 2.125f; value.emissiveColorConstant = {3.25f,4.5f,5.75f};
  value.spriteSheetRow = 7; value.spriteSheetCol = 11; value.spriteSheetFps = 13;
  value.filterMode = 2; value.wrapModeU = 1; value.wrapModeV = 3;
  return value;
}
static remixapi_MaterialInfoOpaqueEXT opaqueFixture(bool legacyDefaults) {
  remixapi_MaterialInfoOpaqueEXT value{};
  value.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
  value.roughnessTexture = currentPaths ? L"rough.dds" : nullptr;
  value.metallicTexture = nullptr; value.heightTexture = currentPaths ? L"height.dds" : nullptr;
  value.anisotropy = .125f; value.albedoConstant = {.25f,.5f,.75f};
  value.opacityConstant = .875f; value.roughnessConstant = .375f; value.metallicConstant = .625f;
  value.thinFilmThickness_hasvalue = 1; value.thinFilmThickness_value = 31.5f;
  value.alphaIsThinFilmThickness = 1; value.displaceIn = 1.75f;
  value.useDrawCallAlphaState = 1; value.blendType_hasvalue = 1; value.blendType_value = 3;
  value.invertedBlend = 1; value.alphaTestType = 5; value.alphaReferenceValue = 127;
  value.displaceOut = 2.75f;
  value.enableDlssControlMask = 0; value.dlssControlMaskIntensity = .3125f;
  value.dlssControlMaskToneStrength = .5625f; value.dlssControlMaskStructuralStrength = .8125f;
  if (legacyDefaults && currentVersion == 4) {
    value.enableDlssControlMask = 1; value.dlssControlMaskIntensity = 1;
    value.dlssControlMaskToneStrength = 1; value.dlssControlMaskStructuralStrength = 1;
  }
  if (!legacyDefaults || currentVersion == 6) {
    value.textureScrollRate = 7.125f; value.textureScrollAngle = 19.25f;
    value.textureScale_hasvalue = currentScale; value.textureScale_value = {2.5f,3.75f};
    value.textureOffset = {-4.25f,5.5f}; value.textureRotation = 41.125f;
  }
  return value;
}
static remixapi_MaterialInfoTranslucentEXT translucentFixture(bool legacyDefaults) {
  remixapi_MaterialInfoTranslucentEXT value{};
  value.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_TRANSLUCENT_EXT;
  value.transmittanceTexture = currentPaths ? L"trans.dds" : nullptr;
  value.refractiveIndex = 1.5f; value.transmittanceColor = {.1875f,.4375f,.6875f};
  value.transmittanceMeasurementDistance = 8.75f; value.thinWallThickness_hasvalue = 1;
  value.thinWallThickness_value = 6.5f; value.useDiffuseLayer = 1;
  if (!legacyDefaults || currentVersion == 6) {
    value.textureScrollRate = -3.25f; value.textureScrollAngle = 29.5f;
    value.textureScale_hasvalue = currentScale; value.textureScale_value = {6.25f,7.5f};
    value.textureOffset = {8.75f,-9.125f}; value.textureRotation = 53.75f;
  }
  return value;
}
static remixapi_MaterialInfoPortalEXT portalFixture() {
  remixapi_MaterialInfoPortalEXT value{};
  value.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_PORTAL_EXT;
  value.rayPortalIndex = 2; value.rotationSpeed = 3.125f; return value;
}
static void equalPath(remixapi_Path actual, remixapi_Path expected, const char* message) {
  check((actual == nullptr) == (expected == nullptr), message);
  if (expected) check(wcscmp(actual, expected) == 0, message);
}
template<class T> static void equalValue(const T& actual, const T& expected, const char* message) {
  check(memcmp(&actual, &expected, sizeof(T)) == 0, message);
}
#define FIELD(name) equalValue(actual.name, expected.name, "field mismatch: " #name)
#define PATH(name) equalPath(actual.name, expected.name, "path mismatch: " #name)
static void verifyBase(const remixapi_MaterialInfo& actual) {
  auto expected = baseFixture();
  FIELD(sType); FIELD(hash); PATH(albedoTexture); PATH(normalTexture); PATH(tangentTexture); PATH(emissiveTexture);
  FIELD(emissiveIntensity); FIELD(emissiveColorConstant); FIELD(spriteSheetRow); FIELD(spriteSheetCol);
  FIELD(spriteSheetFps); FIELD(filterMode); FIELD(wrapModeU); FIELD(wrapModeV);
}
static void verifyOpaque(const remixapi_MaterialInfoOpaqueEXT& actual) {
  auto expected = opaqueFixture(true);
  FIELD(sType); PATH(roughnessTexture); PATH(metallicTexture); FIELD(anisotropy); FIELD(albedoConstant);
  FIELD(opacityConstant); FIELD(roughnessConstant); FIELD(metallicConstant); FIELD(thinFilmThickness_hasvalue);
  FIELD(thinFilmThickness_value); FIELD(alphaIsThinFilmThickness); PATH(heightTexture); FIELD(displaceIn);
  FIELD(useDrawCallAlphaState); FIELD(blendType_hasvalue); FIELD(blendType_value); FIELD(invertedBlend);
  FIELD(alphaTestType); FIELD(alphaReferenceValue); FIELD(displaceOut); FIELD(enableDlssControlMask);
  FIELD(dlssControlMaskIntensity); FIELD(dlssControlMaskToneStrength); FIELD(dlssControlMaskStructuralStrength);
  FIELD(textureScrollRate); FIELD(textureScrollAngle); FIELD(textureScale_hasvalue); FIELD(textureScale_value);
  FIELD(textureOffset); FIELD(textureRotation);
}
static void verifyTranslucent(const remixapi_MaterialInfoTranslucentEXT& actual) {
  auto expected = translucentFixture(true);
  FIELD(sType); PATH(transmittanceTexture); FIELD(refractiveIndex); FIELD(transmittanceColor);
  FIELD(transmittanceMeasurementDistance); FIELD(thinWallThickness_hasvalue); FIELD(thinWallThickness_value);
  FIELD(useDiffuseLayer); FIELD(textureScrollRate); FIELD(textureScrollAngle); FIELD(textureScale_hasvalue);
  FIELD(textureScale_value); FIELD(textureOffset); FIELD(textureRotation);
}
static void verifyPortal(const remixapi_MaterialInfoPortalEXT& actual) {
  auto expected = portalFixture(); FIELD(sType); FIELD(rayPortalIndex); FIELD(rotationSpeed);
}
static remixapi_ErrorCode REMIXAPI_CALL backendMaterial(const remixapi_MaterialInfo* info,
                                                        remixapi_MaterialHandle* handle) {
  ++backendCalls; verifyBase(*info);
  auto* opaque = static_cast<const remixapi_MaterialInfoOpaqueEXT*>(info->pNext);
  check(opaque != nullptr, "missing opaque extension"); verifyOpaque(*opaque);
  auto* translucent = static_cast<const remixapi_MaterialInfoTranslucentEXT*>(opaque->pNext);
  check(translucent != nullptr, "missing translucent extension"); verifyTranslucent(*translucent);
  auto* portal = static_cast<const remixapi_MaterialInfoPortalEXT*>(translucent->pNext);
  check(portal != nullptr, "missing portal extension"); verifyPortal(*portal);
  check(portal->pNext == nullptr, "extension chain did not terminate");
  *handle = reinterpret_cast<remixapi_MaterialHandle>(uintptr_t(0x1000 + backendCalls));
  return REMIXAPI_ERROR_CODE_SUCCESS;
}
#undef FIELD
#undef PATH

static void put32(std::ostream& stream, uint32_t value) { stream.write(reinterpret_cast<const char*>(&value), 4); }
static uint32_t get32(std::istream& stream) {
  uint32_t value = 0; stream.read(reinterpret_cast<char*>(&value), 4);
  check(stream.good(), "truncated fixture scalar"); return value;
}
static void writePackets(std::ostream& stream) {
  put32(stream, currentVersion); put32(stream, currentScale); put32(stream, currentPaths);
  put32(stream, static_cast<uint32_t>(packets.size()));
  for (const auto& packet : packets) {
    put32(stream, packet.kind); put32(stream, static_cast<uint32_t>(packet.bytes.size()));
    stream.write(reinterpret_cast<const char*>(packet.bytes.data()), packet.bytes.size());
  }
  check(stream.good(), "fixture write failed");
}
static void readPackets(std::istream& stream) {
  currentVersion = get32(stream); currentScale = get32(stream); currentPaths = get32(stream);
  check(currentVersion >= 4 && currentVersion <= 6 && currentScale <= 1 && currentPaths <= 1,
        "invalid fixture scenario");
  const auto count = get32(stream); check(count == 14, "material packet count changed");
  packets.clear(); readIndex = 0; decodedStorage.clear();
  for (uint32_t i = 0; i < count; ++i) {
    const auto kind = get32(stream), size = get32(stream);
    check((kind == 1 || kind == 2) && size >= 4 && size <= 4096, "invalid fixture packet");
    Packet packet{kind, std::vector<uint8_t>(size)};
    stream.read(reinterpret_cast<char*>(packet.bytes.data()), size);
    check(stream.good(), "truncated fixture packet"); packets.push_back(std::move(packet));
  }
}

#if defined(MATERIAL_CLIENT)
static void createGuardedMaterial() {
  auto base = baseFixture(); auto opaque = opaqueFixture(false);
  auto translucent = translucentFixture(false); auto portal = portalFixture();
  const size_t opaqueBytes = currentVersion == 4 ? offsetof(remixapi_MaterialInfoOpaqueEXT, enableDlssControlMask) :
    currentVersion == 5 ? offsetof(remixapi_MaterialInfoOpaqueEXT, textureScrollRate) : sizeof(opaque);
  const size_t translucentBytes = currentVersion < 6 ? offsetof(remixapi_MaterialInfoTranslucentEXT, textureScrollRate) : sizeof(translucent);
  GuardedEnd opaqueMemory(opaqueBytes), translucentMemory(translucentBytes), portalMemory(sizeof(portal));
  memcpy(opaqueMemory.data, &opaque, opaqueBytes); memcpy(translucentMemory.data, &translucent, translucentBytes);
  memcpy(portalMemory.data, &portal, sizeof(portal));
  base.pNext = opaqueMemory.data;
  reinterpret_cast<remixapi_MaterialInfoOpaqueEXT*>(opaqueMemory.data)->pNext = translucentMemory.data;
  reinterpret_cast<remixapi_MaterialInfoTranslucentEXT*>(translucentMemory.data)->pNext = portalMemory.data;
  remixapi::g_appRemixApiVersion = REMIXAPI_VERSION_MAKE(0, 6, currentVersion);
  remixapi_MaterialHandle handle = nullptr;
  packets.clear();
  check(remixapi::remixapi_CreateMaterial(&base, &handle) == REMIXAPI_ERROR_CODE_SUCCESS, "CreateMaterial failed");
  check(handle != nullptr, "CreateMaterial did not return handle");
  check(packets.size() == 13, "CreateMaterial emitted unexpected packet count");
  check(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle)) > lastBridgeHandle, "handle did not advance");
  lastBridgeHandle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
  ClientMessage tail(Commands::RemixApi_CreateMaterial); tail.send_data(Commands::FollowingCommand);
  // Actual serialized blobs must use size words equal to exact payload bounds.
  // Fixed no-path sizes independently count public wire primitives, not sizeof(struct).
  const uint32_t baseSize = 42 + (currentPaths ? 40 : 0);
  const uint32_t opaqueSize = 128 + (currentPaths ? 42 : 0);
  const uint32_t translucentSize = 73 + (currentPaths ? 20 : 0);
  printf("MATERIAL_WIRE version=0.6.%u scale=%u paths=%u base=%zu opaque=%zu translucent=%zu portal=%zu\n",
    currentVersion, currentScale, currentPaths, packets[1].bytes.size(), packets[4].bytes.size(),
    packets[7].bytes.size(), packets[10].bytes.size());
  check(packets[1].bytes.size() == baseSize, "base wire size mismatch");
  check(packets[4].bytes.size() == opaqueSize, "opaque wire size mismatch");
  check(packets[7].bytes.size() == translucentSize, "translucent wire size mismatch");
  check(packets[10].bytes.size() == 13, "portal wire size mismatch");
  for (size_t i : {size_t(1), size_t(4), size_t(7), size_t(10)}) {
    uint32_t declared; memcpy(&declared, packets[i].bytes.data(), 4);
    check(declared == packets[i].bytes.size(), "producer blob exceeded declared size");
  }
}
#endif

int main(int argc, char** argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  SetUnhandledExceptionFilter(guardFailure);
  try {
    check(argc == 2, "expected fixture path");
    printf("MATERIAL_LAYOUT pointer=%zu opaque=%zu prefix064=%zu prefix065=%zu translucent=%zu prefix065=%zu\n",
      sizeof(void*), sizeof(remixapi_MaterialInfoOpaqueEXT), offsetof(remixapi_MaterialInfoOpaqueEXT, enableDlssControlMask),
      offsetof(remixapi_MaterialInfoOpaqueEXT, textureScrollRate), sizeof(remixapi_MaterialInfoTranslucentEXT),
      offsetof(remixapi_MaterialInfoTranslucentEXT, textureScrollRate));
#if defined(MATERIAL_CLIENT)
    std::ofstream file(argv[1], std::ios::binary); check(file.good(), "cannot write fixture");
    put32(file, 0x4d415436); put32(file, 12);
    for (currentVersion = 4; currentVersion <= 6; ++currentVersion)
      for (currentScale = 0; currentScale <= 1; ++currentScale)
        for (currentPaths = 0; currentPaths <= 1; ++currentPaths) {
          createGuardedMaterial(); writePackets(file);
        }
    printf("REMIX_MATERIAL_PASS role=client scenarios=12 assertions=%u\n", assertions);
#else
    std::ifstream file(argv[1], std::ios::binary); check(file.good(), "cannot read fixture");
    check(get32(file) == 0x4d415436 && get32(file) == 12, "wrong fixture header");
    remixapi::g_remix.CreateMaterial = backendMaterial;
    for (unsigned scenario = 0; scenario < 12; ++scenario) {
      readPackets(file);
      const unsigned before = backendCalls;
      dispatchMaterial();
      check(backendCalls == before + 1, "backend dispatch count mismatch");
      check(readIndex == 13, "material command consumed wrong packet count");
      check(DeviceBridge::get_data() == Commands::FollowingCommand, "following command was corrupted");
      check(readIndex == packets.size(), "material command left unread payload");
    }
    check(file.peek() == EOF, "unexpected trailing fixture bytes");
    printf("REMIX_MATERIAL_PASS role=server scenarios=12 assertions=%u\n", assertions);
#endif
    return 0;
  } catch (const std::exception& error) {
    fprintf(stderr, "REMIX_MATERIAL_FAIL: %s\n", error.what()); return 1;
  }
}
