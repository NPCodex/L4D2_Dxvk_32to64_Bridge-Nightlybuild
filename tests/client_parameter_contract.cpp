// SPDX-License-Identifier: MIT
// Actual production methods; only transport, reference dispatch and diagnostics are adapted.
// No GPU performance or end-to-end rendering result is implied by these tests.
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>
#include "constant_caps.h"

void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
using UID = uint32_t;
struct BridgeReadFailure {};
std::atomic<bool> gbBridgeRunning { true };
namespace Commands {
enum : uint32_t {
  IDirect3DDevice9Ex_SetVertexShaderConstantF, IDirect3DDevice9Ex_SetVertexShaderConstantI,
  IDirect3DDevice9Ex_SetVertexShaderConstantB, IDirect3DDevice9Ex_SetPixelShaderConstantF,
  IDirect3DDevice9Ex_SetPixelShaderConstantI, IDirect3DDevice9Ex_SetPixelShaderConstantB,
  IDirect3DQuery9_GetData
};
}
namespace l4d2_data {
std::atomic<bool> enabled { false };
template<typename... T> void stateSet(T...) {}
}
namespace l4d2_api_wait {
enum class Api { UpdateTexture };
struct ApiScope { explicit ApiScope(Api) {} };
}
struct Capture {
  uint32_t sends = 0, start = 0, count = 0, bytes = 0;
  std::vector<uint8_t> payload;
} capture;
struct ClientMessage {
  ClientMessage(uint32_t, uint32_t) { ++capture.sends; }
  UID get_uid() const { return 1; }
  void send_many(uint32_t start, uint32_t count) { capture.start = start; capture.count = count; }
  void send_data(uint32_t) {}
  void send_data(size_t size, const void* data) {
    check(size <= 131072, "unbounded constant payload reached wire");
    capture.bytes = static_cast<uint32_t>(size);
    capture.payload.resize(size);
    if (size) std::memcpy(capture.payload.data(), data, size);
  }
};
namespace DeviceBridge {
HRESULT result = S_OK;
uint32_t returnedSize = 4, payloadReads = 0, pops = 0;
uint64_t answer = 0x0123456789abcdefull;
bool nullPayload = false, readFailure = false;
HRESULT get_data() { if (readFailure) throw BridgeReadFailure(); return result; }
uint32_t get_data(void** data) { ++payloadReads; *data = nullPayload ? nullptr : &answer; return returnedSize; }
void pop_front() { ++pops; }
}
#define LogFunctionCall() ((void)0)
#define ZoneScoped ((void)0)
#define BRIDGE_DEVICE_LOCKGUARD() ((void)0)
#define WAIT_FOR_OPTIONAL_SERVER_RESPONSE(name, failure, uid) do { (void)(uid); return S_OK; } while (0)
#define WAIT_FOR_SERVER_RESPONSE(name, failure, uid) do { (void)(uid); } while (0)

struct BaseDirect3DDevice9Ex_LSS {
#include "constant_layout.h"
  using ShaderType = ShaderConstants::ShaderType;
  using ConstantType = ShaderConstants::ConstantType;
  struct ConstantFlags {
    std::array<bool, caps::MaxFloatConstantsSoftware> fConsts {};
    std::array<bool, caps::MaxOtherConstantsSoftware> iConsts {}, bConsts {};
  };
  struct StateCaptureDirtyFlags { ConstantFlags vertexConstants, pixelConstants; };
  struct State {
    ShaderConstants::VertexConstants vertexConstants {};
    ShaderConstants::PixelConstants pixelConstants {};
  };
  struct Recording { State m_captureState; StateCaptureDirtyFlags m_dirtyFlags; };
  State m_state;
  Recording* m_stateRecording = nullptr;
  std::unique_ptr<StateCaptureDirtyFlags> m_dataKnown;
  template<ShaderType ShaderT, ConstantType ConstantT, typename T>
  std::tuple<HRESULT, size_t> commonGetSetConstants(uint32_t startRegister, const T* pConstantData, uint32_t count);
  template<ShaderType ShaderT, ConstantType ConstantT, typename T>
  HRESULT setShaderConstants(uint32_t startRegister, const T* pConstantData, uint32_t count);
  template<ShaderType ShaderT, ConstantType ConstantT, typename T>
  HRESULT getShaderConstants(uint32_t startRegister, T* pConstantData, uint32_t count);
};
using ShaderType = BaseDirect3DDevice9Ex_LSS::ShaderConstants::ShaderType;
using ConstantType = BaseDirect3DDevice9Ex_LSS::ShaderConstants::ConstantType;
using Vec4f = BaseDirect3DDevice9Ex_LSS::ShaderConstants::Vec4<float>;
using Vec4i = BaseDirect3DDevice9Ex_LSS::ShaderConstants::Vec4<int>;
struct Direct3DTexture9_LSS {};
struct Direct3DCubeTexture9_LSS {};
struct Direct3DVolumeTexture9_LSS {};
template<bool EnableSync> struct Direct3DDevice9Ex_LSS : BaseDirect3DDevice9Ex_LSS {
  uint32_t getId() const { return 7; }
  uint32_t textureCalls = 0;
#include "device_declarations.h"
  HRESULT UpdateTexture(IDirect3DBaseTexture9*, IDirect3DBaseTexture9*);
  template<typename T> HRESULT UpdateTextureImpl(IDirect3DBaseTexture9*, IDirect3DBaseTexture9*) {
    ++textureCalls; return S_OK;
  }
};
struct Direct3DQuery9_LSS {
  D3DQUERYTYPE m_type;
  explicit Direct3DQuery9_LSS(D3DQUERYTYPE type) : m_type(type) {}
  uint32_t getId() const { return 9; }
  DWORD GetDataSize();
  HRESULT GetData(void*, DWORD, DWORD);
};
#include "client_methods.h"

struct Texture final : IDirect3DBaseTexture9 {
  D3DRESOURCETYPE type;
  uint32_t typeCalls = 0;
  explicit Texture(D3DRESOURCETYPE t) : type(t) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
  ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void*, DWORD*) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return E_NOTIMPL; }
  DWORD STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
  DWORD STDMETHODCALLTYPE GetPriority() override { return 0; }
  void STDMETHODCALLTYPE PreLoad() override {}
  D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { ++typeCalls; return type; }
  DWORD STDMETHODCALLTYPE SetLOD(DWORD) override { return 0; }
  DWORD STDMETHODCALLTYPE GetLOD() override { return 0; }
  DWORD STDMETHODCALLTYPE GetLevelCount() override { return 1; }
  HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return S_OK; }
  D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override { return D3DTEXF_NONE; }
  void STDMETHODCALLTYPE GenerateMipSubLevels() override {}
};

uint32_t rangeCases = 0;
template<ShaderType ST, ConstantType CT, typename T>
void constants() {
  auto device = std::make_unique<Direct3DDevice9Ex_LSS<false>>();
  using Constants = BaseDirect3DDevice9Ex_LSS::ShaderConstants;
  constexpr uint32_t sw = Constants::getSoftwareRegCount<ST, CT>();
  const uint32_t hw = Constants::getHardwareRegCount<ST, CT>();
  const T data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  const auto common = [&](uint32_t start, uint32_t count, const T* pointer) {
    ++rangeCases;
    return device->template commonGetSetConstants<ST, CT>(start, pointer, count);
  };
  for (uint32_t start : {0u, 1u, hw - 1, hw, sw - 1, sw, sw + 1, UINT32_MAX - 1, UINT32_MAX}) {
    for (uint32_t count : {0u, 1u, 2u, hw, sw, UINT32_MAX - 1, UINT32_MAX}) {
      const auto [status, adjusted] = common(start, count, data);
      const uint64_t end = uint64_t(start) + count;
      const bool valid = start <= sw && end <= sw;
      check((status == S_OK) == valid, "wrapped constant range was accepted");
      if (valid) {
        const uint32_t expected = start < hw ? static_cast<uint32_t>((std::min)(end, uint64_t(hw)) - start) : 0;
        check(adjusted == expected, "hardware/software constant clamp changed");
      }
    }
  }
  check(std::get<0>(common(0, 1, nullptr)) == D3DERR_INVALIDCALL, "nonempty null constants accepted");
  check(std::get<0>(common(0, 0, nullptr)) == S_OK, "empty common constant semantics changed");
  check(device->template setShaderConstants<ST, CT>(hw - 1, data, 2) == (sw > hw ? S_OK : D3DERR_INVALIDCALL),
    "hardware boundary update differs");
  check(device->template setShaderConstants<ST, CT>(0, data, 2) == S_OK, "normal constant update failed");
  T read[8] {};
  check(device->template getShaderConstants<ST, CT>(0, read, 2) == S_OK, "normal constant read failed");
  if constexpr (CT == ConstantType::Bool) { check(read[0] == TRUE && read[1] == TRUE, "BOOL normalization changed"); }
  else { check(std::memcmp(read, data, sizeof(read)) == 0, "constant roundtrip changed"); }
  auto recording = std::make_unique<BaseDirect3DDevice9Ex_LSS::Recording>();
  device->m_stateRecording = recording.get();
  T zero[8] {};
  check(device->template setShaderConstants<ST, CT>(0, zero, 2) == S_OK, "recorded update failed");
  check(device->template getShaderConstants<ST, CT>(0, read, 2) == S_OK && read[0] == 1, "recording changed live state");
  const auto old = device->m_state;
  check(device->template setShaderConstants<ST, CT>(UINT32_MAX, data, 2) == D3DERR_INVALIDCALL,
    "invalid constant update succeeded");
  check(std::memcmp(&old, &device->m_state, sizeof(old)) == 0, "invalid update changed local state");
}

void publicConstants() {
  auto d = std::make_unique<Direct3DDevice9Ex_LSS<false>>();
  float f[8] {1,2,3,4,5,6,7,8}; int i[8] {1,2,3,4,5,6,7,8}; BOOL b[8] {1,2,0,1};
#define PUBLIC(shader, suffix, data, expectedBytes) \
  capture = {}; \
  check(d->Set##shader##ShaderConstant##suffix(UINT32_MAX, data, 2) == D3DERR_INVALIDCALL, "public wrapped constant range accepted"); \
  check(capture.sends == 0, "invalid constants reached transport"); \
  check(d->Set##shader##ShaderConstant##suffix(0, data, 2) == S_OK, "public constants update failed"); \
  check(capture.sends == 1 && capture.count == 2 && capture.bytes == expectedBytes, "constant wire contract changed"); \
  check(d->Get##shader##ShaderConstant##suffix(UINT32_MAX, data, 2) == D3DERR_INVALIDCALL, "public invalid constant read succeeded")
  PUBLIC(Vertex, F, f, 32); PUBLIC(Vertex, I, i, 32); PUBLIC(Vertex, B, b, 8);
  PUBLIC(Pixel, F, f, 32); PUBLIC(Pixel, I, i, 32); PUBLIC(Pixel, B, b, 8);
#undef PUBLIC
}

void queryContracts() {
  const std::pair<D3DQUERYTYPE, size_t> sizes[] = {
    { D3DQUERYTYPE_VCACHE, sizeof(D3DDEVINFO_VCACHE) },
    { D3DQUERYTYPE_RESOURCEMANAGER, sizeof(D3DDEVINFO_RESOURCEMANAGER) },
    { D3DQUERYTYPE_VERTEXSTATS, sizeof(D3DDEVINFO_D3DVERTEXSTATS) },
    { D3DQUERYTYPE_EVENT, sizeof(BOOL) },
    { D3DQUERYTYPE_OCCLUSION, sizeof(DWORD) },
    { D3DQUERYTYPE_TIMESTAMP, sizeof(UINT64) },
    { D3DQUERYTYPE_TIMESTAMPDISJOINT, sizeof(BOOL) },
    { D3DQUERYTYPE_TIMESTAMPFREQ, sizeof(UINT64) },
    { D3DQUERYTYPE_PIPELINETIMINGS, sizeof(D3DDEVINFO_D3D9PIPELINETIMINGS) },
    { D3DQUERYTYPE_INTERFACETIMINGS, sizeof(D3DDEVINFO_D3D9INTERFACETIMINGS) },
    { D3DQUERYTYPE_VERTEXTIMINGS, sizeof(D3DDEVINFO_D3D9STAGETIMINGS) },
    { D3DQUERYTYPE_PIXELTIMINGS, sizeof(D3DDEVINFO_D3D9STAGETIMINGS) },
    { D3DQUERYTYPE_BANDWIDTHTIMINGS, sizeof(D3DDEVINFO_D3D9BANDWIDTHTIMINGS) },
    { D3DQUERYTYPE_CACHEUTILIZATION, sizeof(D3DDEVINFO_D3D9CACHEUTILIZATION) }
  };
  for (const auto& item : sizes) {
    Direct3DQuery9_LSS typed(item.first);
    check(typed.GetDataSize() == item.second, "query SDK result size mismatch");
  }
  Direct3DQuery9_LSS pixelTiming(D3DQUERYTYPE_PIXELTIMINGS);
  D3DDEVINFO_D3D9STAGETIMINGS pixelOutput {};
  DeviceBridge::returnedSize = sizeof(pixelOutput);
  check(pixelTiming.GetData(&pixelOutput, sizeof(pixelOutput), 0) == S_OK,
    "valid pixel timing result rejected by exact size guard");
  DeviceBridge::returnedSize = 4; DeviceBridge::payloadReads = 0; DeviceBridge::pops = 0;
  Direct3DQuery9_LSS query(D3DQUERYTYPE_OCCLUSION);
  uint32_t output = 0xfeedface;
  DeviceBridge::result = S_FALSE; capture = {};
  check(query.GetData(&output, sizeof(output), 0) == S_FALSE, "query pending HRESULT changed");
  check(output == 0xfeedface, "pending query overwrote caller data");
  check(DeviceBridge::payloadReads == 0 && DeviceBridge::pops == 1, "pending query consumed payload");
  DeviceBridge::result = S_OK;
  check(query.GetData(&output, sizeof(output), D3DGETDATA_FLUSH) == S_OK && output == 0x89abcdef,
    "ready query result changed");
  const auto reads = DeviceBridge::payloadReads;
  check(query.GetData(nullptr, 0, 0) == S_OK && DeviceBridge::payloadReads == reads, "null poll consumed payload");
  const auto sends = capture.sends;
  check(query.GetData(nullptr, sizeof(output), 0) == D3DERR_INVALIDCALL, "null query destination accepted");
  check(query.GetData(&output, 1, 0) == D3DERR_INVALIDCALL, "wrong query size accepted");
  check(query.GetData(&output, sizeof(output), 2) == D3DERR_INVALIDCALL, "unknown query flags accepted");
  check(capture.sends == sends, "invalid query issued IPC");
  for (auto result : {S_FALSE, D3DERR_DEVICELOST, D3DERR_INVALIDCALL}) {
    DeviceBridge::result = result; output = 0xfeedface;
    check(query.GetData(&output, sizeof(output), 0) == result && output == 0xfeedface, "failed query changed output");
  }
  DeviceBridge::result = S_OK;
  for (uint32_t size : {0u, 1u, 8u, UINT32_MAX}) {
    output = 0xfeedface; DeviceBridge::returnedSize = size;
    const auto pops = DeviceBridge::pops;
    check(query.GetData(&output, sizeof(output), 0) == D3DERR_INVALIDCALL && output == 0xfeedface,
      "malformed query payload copied");
    check(DeviceBridge::pops == pops + 1, "malformed reply was not popped once");
  }
  DeviceBridge::returnedSize = 4; DeviceBridge::nullPayload = true; output = 0xfeedface;
  check(query.GetData(&output, 4, 0) == D3DERR_INVALIDCALL && output == 0xfeedface, "null query payload copied");
  DeviceBridge::nullPayload = false; DeviceBridge::readFailure = true;
  check(query.GetData(&output, 4, 0) == D3DERR_DEVICELOST, "query read failure escaped HRESULT boundary");
}

void textureContracts() {
  auto d = std::make_unique<Direct3DDevice9Ex_LSS<false>>();
  Texture texture(D3DRTYPE_TEXTURE), cube(D3DRTYPE_CUBETEXTURE), volume(D3DRTYPE_VOLUMETEXTURE);
  check(d->UpdateTexture(nullptr, &texture) == D3DERR_INVALIDCALL, "null texture source accepted");
  check(d->UpdateTexture(&texture, nullptr) == D3DERR_INVALIDCALL, "null texture destination accepted");
  check(d->UpdateTexture(&texture, &cube) == D3DERR_INVALIDCALL && d->textureCalls == 0,
    "mismatched texture reached wrapper cast");
  for (Texture* t : {&texture, &cube, &volume}) {
    t->typeCalls = 0;
    check(d->UpdateTexture(t, t) == S_OK && t->typeCalls == 2, "valid texture type dispatch changed");
  }
  check(d->textureCalls == 3, "valid texture dispatch did not execute exactly once");
}

int main() {
  try {
    constants<ShaderType::Vertex, ConstantType::Float, float>();
    constants<ShaderType::Vertex, ConstantType::Int, int>();
    constants<ShaderType::Vertex, ConstantType::Bool, BOOL>();
    constants<ShaderType::Pixel, ConstantType::Float, float>();
    constants<ShaderType::Pixel, ConstantType::Int, int>();
    constants<ShaderType::Pixel, ConstantType::Bool, BOOL>();
    publicConstants(); queryContracts(); textureContracts();
    std::printf("PASS: %u production range cases, six public constant pairs, local/recorded state, query pending/ready/failure/malformed replies, texture null/type dispatch.\n", rangeCases);
    return 0;
  } catch (const std::exception& error) {
    std::printf("FAIL: %s\n", error.what());
    return 1;
  }
}
