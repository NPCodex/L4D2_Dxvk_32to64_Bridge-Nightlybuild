// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
#include <chrono>
#include <psapi.h>

template<typename T> class MappedTestBuffer : public LockableBuffer<T> {
  using Desc = std::conditional_t<std::is_same_v<T, IDirect3DVertexBuffer9>, D3DVERTEXBUFFER_DESC, D3DINDEXBUFFER_DESC>;
public:
  explicit MappedTestBuffer(const Desc& desc) : LockableBuffer<T>(nullptr, nullptr, desc) {}
  using LockableBuffer<T>::lock;
  using LockableBuffer<T>::unlock;
};
static void requireMapped(bool value, const char* text) {
  if (!value) { std::fprintf(stderr, "MAPPED_BUFFER_FAIL %s\n", text); std::exit(2); }
}
template<typename T> void mappedContracts(D3DFORMAT format, D3DPOOL pool) {
  using Desc = std::conditional_t<std::is_same_v<T, IDirect3DVertexBuffer9>, D3DVERTEXBUFFER_DESC, D3DINDEXBUFFER_DESC>;
  Desc desc {}; desc.Size = 262144; desc.Format = format; desc.Pool = pool;
  mappedBudgetTest = 0;
  for (bool whole : { false, true }) {
    wholeStaticTest = whole;
    updates.clear();
    {
      MappedTestBuffer<T> buffer(desc);
      requireMapped(l4d2_buffer::MappedBufferShadow::stats().mappedCount == 0, "creation mapped a persistent view");
      void* pointer = nullptr;
      requireMapped(buffer.lock(0, 0, &pointer, 0) == S_OK, "first map");
      for (unsigned i = 0; i < desc.Size; ++i) {
        requireMapped(static_cast<uint8_t*>(pointer)[i] == 0, "initial zero contents");
      }
      std::memset(pointer, 0x57, desc.Size);
      requireMapped(buffer.unlock() == S_OK, "initial upload");
      requireMapped(l4d2_buffer::MappedBufferShadow::stats().mappedCount == 0, "zero-cache retained VA");
      requireMapped(buffer.lock(0, 0, &pointer, 0) == S_OK, "second map");
      static_cast<uint8_t*>(pointer)[12] = 0xaa;
      requireMapped(buffer.unlock() == S_OK, "partial modification upload");
      requireMapped(updates.back().bytes[11] == 0x57 && updates.back().bytes[12] == 0xaa
        && updates.back().bytes[13] == 0x57, "historical contents lost");
      void* first = nullptr; void* second = nullptr;
      requireMapped(buffer.lock(8, 16, &first, 0) == S_OK && buffer.lock(12, 8, &second, 0) == S_OK, "overlap map");
      requireMapped(static_cast<uint8_t*>(first) + 4 == second, "overlapping pointers do not alias");
      static_cast<uint8_t*>(second)[0] = 0xbb;
      requireMapped(buffer.unlock() == S_OK, "FIFO first unlock");
      requireMapped(l4d2_buffer::MappedBufferShadow::stats().locks == 1
        && static_cast<uint8_t*>(second)[0] == 0xbb, "first unlock invalidated second pointer");
      requireMapped(buffer.unlock() == S_OK, "FIFO last unlock");
      requireMapped(l4d2_buffer::MappedBufferShadow::stats().mappedCount == 0, "last unlock did not unmap");
      const auto count = updates.size();
      requireMapped(buffer.lock(12, 8, &pointer, D3DLOCK_READONLY) == S_OK
        && static_cast<uint8_t*>(pointer)[0] == 0xbb, "READONLY history");
      requireMapped(buffer.unlock() == S_OK && updates.size() == count, "READONLY uploaded");
      requireMapped(buffer.unlock() == S_OK, "benign unlock changed");
      requireMapped(buffer.lock(desc.Size - 8, 0, &pointer, 0) == S_OK, "offset-to-end map");
      std::memset(pointer, 0x4c, 8);
      failSubmissionTest = true;
      requireMapped(buffer.unlock() == D3DERR_DEVICELOST, "failed submit accepted");
      requireMapped(l4d2_buffer::MappedBufferShadow::stats().locks == 1
        && static_cast<uint8_t*>(pointer)[0] == 0x4c, "failure released source");
      failSubmissionTest = false;
      requireMapped(buffer.unlock() == S_OK, "retained failure retry");
      requireMapped(buffer.lock(0, 8, &pointer, 0) == S_OK, "destroy-while-locked setup");
    }
    const auto after = l4d2_buffer::MappedBufferShadow::stats();
    requireMapped(!after.backingBytes && !after.mappedVaBytes && !after.locks, "destroy leaked mapped storage");
  }
  wholeStaticTest = false;
  {
    MappedTestBuffer<T> buffer(desc); void* pointer = reinterpret_cast<void*>(1);
    mappedMapFailure = true;
    requireMapped(buffer.lock(0, 0, &pointer, 0) == E_OUTOFMEMORY && !pointer, "map failure returned pointer");
    requireMapped(!l4d2_buffer::MappedBufferShadow::stats().locks, "map failure left pin");
    mappedMapFailure = false;
    requireMapped(buffer.lock(0, 8, &pointer, 0) == S_OK, "map retry after failure");
    requireMapped(buffer.unlock() == S_OK, "map retry unlock");
  }
  mappedCreateFailure = true;
  bool threw = false;
  try { MappedTestBuffer<T> discarded(desc); } catch (const std::bad_alloc&) { threw = true; }
  mappedCreateFailure = false;
  requireMapped(threw && !l4d2_buffer::MappedBufferShadow::stats().backingCount, "creation failure contract");
}

static uint64_t freeVa(uint64_t& largest) {
  SYSTEM_INFO system {}; GetSystemInfo(&system);
  const uint64_t limit = reinterpret_cast<uintptr_t>(system.lpMaximumApplicationAddress) + uint64_t(1);
  uint64_t address = 0, total = 0; largest = 0;
  while (address < limit) {
    MEMORY_BASIC_INFORMATION region {};
    if (!VirtualQuery(reinterpret_cast<void*>(static_cast<uintptr_t>(address)), &region, sizeof(region))) { break; }
    if (region.State == MEM_FREE) { total += region.RegionSize; largest = (std::max)(largest, uint64_t(region.RegionSize)); }
    const auto next = reinterpret_cast<uintptr_t>(region.BaseAddress) + uint64_t(region.RegionSize);
    if (next <= address) { break; }
    address = next;
  }
  requireMapped(address >= limit, "incomplete VA scan");
  return total;
}
static void printCost(const char* variant, const char* mode, unsigned bytes, unsigned count,
                      ULONG64 cycles, uint64_t ns, uint64_t beforeFree, uint64_t afterFree,
                      uint64_t beforeLargest, uint64_t afterLargest) {
  const auto stats = l4d2_buffer::MappedBufferShadow::stats();
  std::printf("{\"variant\":\"%s\",\"mode\":\"%s\",\"bytes\":%u,\"count\":%u,"
    "\"cycles\":%llu,\"ns\":%llu,\"va_free_before\":%llu,\"va_free_after\":%llu,"
    "\"largest_before\":%llu,\"largest_after\":%llu,\"backing\":%llu,\"mapped\":%llu,"
    "\"maps\":%llu,\"unmaps\":%llu,\"idle_budget\":%llu}\n",
    variant, mode, bytes, count, cycles, ns, beforeFree, afterFree, beforeLargest, afterLargest,
    stats.backingBytes, stats.mappedVaBytes, stats.maps, stats.unmaps, stats.idleBudgetBytes);
}
static void benchmark(const char* variant) {
  mappedShadowTest = std::strcmp(variant, "off") != 0 && std::strcmp(variant, "old") != 0;
  mappedBudgetTest = std::strcmp(variant, "zero") == 0 ? 0 : 32 * 1024 * 1024;
  constexpr unsigned HotCount = 8192;
  for (unsigned bytes : { 65536u, 262144u, 1048576u, 8388608u }) {
    D3DVERTEXBUFFER_DESC desc {}; desc.Size = bytes; desc.Format = D3DFMT_VERTEXDATA; desc.Pool = D3DPOOL_MANAGED;
    for (const char* mode : { "static-hot", "dynamic-hot" }) {
      desc.Usage = std::strcmp(mode, "dynamic-hot") == 0 ? D3DUSAGE_DYNAMIC : 0;
      MappedTestBuffer<IDirect3DVertexBuffer9> buffer(desc);
      void* pointer = nullptr;
      requireMapped(buffer.lock(0, 0, &pointer, 0) == S_OK, "benchmark warmup Lock");
      std::memset(pointer, 0x5b, bytes); requireMapped(buffer.unlock() == S_OK, "warmup Unlock");
      updates.clear();
      ULONG64 beginCycles = 0, endCycles = 0;
      QueryThreadCycleTime(GetCurrentThread(), &beginCycles);
      const auto begin = std::chrono::steady_clock::now();
      for (unsigned i = 0; i < HotCount; ++i) {
        requireMapped(buffer.lock(0, 64, &pointer, 0) == S_OK, "hot Lock");
        static_cast<uint8_t*>(pointer)[0] = static_cast<uint8_t>(i);
        requireMapped(buffer.unlock() == S_OK, "hot Unlock");
        updates.clear();
      }
      const auto elapsed = std::chrono::steady_clock::now() - begin;
      QueryThreadCycleTime(GetCurrentThread(), &endCycles);
      printCost(variant, mode, bytes, HotCount, endCycles - beginCycles,
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count(), 0, 0, 0, 0);
    }
    desc.Usage = 0;
    // A large rotating working set distinguishes cache hits from mapping churn.
    {
      std::vector<std::unique_ptr<MappedTestBuffer<IDirect3DVertexBuffer9>>> buffers;
      const unsigned count = (std::min)(256u, (64u * 1024 * 1024) / bytes);
      for (unsigned i = 0; i < count; ++i) {
        buffers.push_back(std::make_unique<MappedTestBuffer<IDirect3DVertexBuffer9>>(desc));
        void* pointer = nullptr;
        requireMapped(buffers.back()->lock(0, 0, &pointer, 0) == S_OK, "churn warmup Lock");
        std::memset(pointer, 0x5b, bytes);
        requireMapped(buffers.back()->unlock() == S_OK, "churn warmup Unlock"); updates.clear();
      }
      ULONG64 beginCycles = 0, endCycles = 0;
      QueryThreadCycleTime(GetCurrentThread(), &beginCycles);
      const auto begin = std::chrono::steady_clock::now();
      for (unsigned i = 0; i < HotCount; ++i) {
        auto& buffer = *buffers[i % count]; void* pointer = nullptr;
        requireMapped(buffer.lock(0, 64, &pointer, 0) == S_OK, "churn Lock");
        static_cast<uint8_t*>(pointer)[0] = static_cast<uint8_t>(i);
        requireMapped(buffer.unlock() == S_OK, "churn Unlock"); updates.clear();
      }
      const auto elapsed = std::chrono::steady_clock::now() - begin;
      QueryThreadCycleTime(GetCurrentThread(), &endCycles);
      printCost(variant, "static-churn", bytes, HotCount, endCycles - beginCycles,
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count(), 0, 0, 0, 0);
    }
    constexpr unsigned ColdCount = 16;
    ULONG64 beginCycles = 0, endCycles = 0;
    QueryThreadCycleTime(GetCurrentThread(), &beginCycles);
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < ColdCount; ++i) {
      MappedTestBuffer<IDirect3DVertexBuffer9> buffer(desc); void* pointer = nullptr;
      requireMapped(buffer.lock(0, 0, &pointer, 0) == S_OK, "cold Lock");
      std::memset(pointer, 0x5b, bytes);
      requireMapped(buffer.unlock() == S_OK, "cold Unlock"); updates.clear();
    }
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    QueryThreadCycleTime(GetCurrentThread(), &endCycles);
    printCost(variant, "create-upload-destroy", bytes, ColdCount, endCycles - beginCycles,
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count(), 0, 0, 0, 0);
    uint64_t beforeLargest = 0, afterLargest = 0;
    const auto beforeFree = freeVa(beforeLargest);
    {
      std::vector<std::unique_ptr<MappedTestBuffer<IDirect3DVertexBuffer9>>> buffers;
      const unsigned count = (std::min)(128u, (128u * 1024 * 1024) / bytes);
      for (unsigned i = 0; i < count; ++i) {
        buffers.push_back(std::make_unique<MappedTestBuffer<IDirect3DVertexBuffer9>>(desc));
        void* pointer = nullptr; requireMapped(buffers.back()->lock(0, 0, &pointer, 0) == S_OK, "idle setup Lock");
        std::memset(pointer, 0x5b, bytes);
        requireMapped(buffers.back()->unlock() == S_OK, "idle setup Unlock"); updates.clear();
      }
      const auto afterFree = freeVa(afterLargest);
      printCost(variant, "loaded-idle", bytes, count, 0, 0, beforeFree, afterFree, beforeLargest, afterLargest);
      void* pointer = nullptr; const auto n = buffers.size() / 2;
      requireMapped(buffers[n]->lock(0, 0, &pointer, D3DLOCK_READONLY) == S_OK, "idle remap");
      for (unsigned i = 0; i < bytes; ++i) { requireMapped(static_cast<uint8_t*>(pointer)[i] == 0x5b, "idle history corruption"); }
      requireMapped(buffers[n]->unlock() == S_OK, "idle READONLY Unlock");
    }
  }
}
int main(int argc, char** argv) {
  if (argc == 2) { benchmark(argv[1]); return 0; }
  mappedContracts<IDirect3DVertexBuffer9>(D3DFMT_VERTEXDATA, D3DPOOL_DEFAULT);
  mappedContracts<IDirect3DVertexBuffer9>(D3DFMT_VERTEXDATA, D3DPOOL_MANAGED);
  mappedContracts<IDirect3DIndexBuffer9>(D3DFMT_INDEX16, D3DPOOL_DEFAULT);
  mappedContracts<IDirect3DIndexBuffer9>(D3DFMT_INDEX32, D3DPOOL_MANAGED);
  mappedBudgetTest = 262144;
  {
    l4d2_buffer::MappedBufferShadow first(262144, mappedBudgetTest), second(262144, mappedBudgetTest);
    auto* p = first.acquire(); requireMapped(p != nullptr, "LRU first acquire"); p[0] = 0x9c; first.release();
    auto* q = second.acquire(); requireMapped(q != nullptr, "LRU second acquire"); second.release();
    requireMapped(l4d2_buffer::MappedBufferShadow::stats().mappedCount == 1, "LRU budget not enforced");
    p = first.acquire(); requireMapped(p && p[0] == 0x9c, "LRU remap lost content"); first.release();
  }
  const auto stats = l4d2_buffer::MappedBufferShadow::stats();
  requireMapped(!stats.backingCount && !stats.mappedCount && !stats.idleBudgetBytes, "final storage imbalance");
  std::puts("VBIB_MAPPED_CONTRACT_PASS");
  return 0;
}
