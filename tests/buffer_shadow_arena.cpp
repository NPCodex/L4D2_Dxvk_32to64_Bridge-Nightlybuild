// Exercise the production allocator core; fake VA is only for deterministic
// exhaustion/commit-failure paths. Windows pressure tests measure real VA.
#include "buffer_shadow_arena.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#endif

struct TestPlatform {
  static constexpr bool freshPagesZero = false;
  using Mutex = std::mutex;
  inline static size_t reserved = 0, metadataCount = 0;
  inline static size_t reserveLimit = SIZE_MAX;
  inline static bool failMetadata = false, failCommit = false, failRelease = false;
  static size_t pageSize() { return 4096; }
  static void* metadata(size_t n) {
    if (failMetadata) return nullptr;
    void* p = std::malloc(n); if (p) ++metadataCount; return p;
  }
  static void freeMetadata(void* p) { --metadataCount; std::free(p); }
  // Header belongs to the test platform, never the production payload.
  struct alignas(16) Header { size_t bytes; uint8_t padding[16 - sizeof(size_t)]; };
  static void* reserve(size_t n) {
    if (n > reserveLimit) return nullptr;
#ifdef _WIN32
    auto* h = static_cast<Header*>(_aligned_malloc(sizeof(Header) + n, 16));
#else
    auto* h = static_cast<Header*>(std::aligned_alloc(16, sizeof(Header) + n));
#endif
    if (!h) return nullptr;
    h->bytes = n; reserved += n; return h + 1;
  }
  static bool commit(void*, size_t) { return !failCommit; }
  static bool release(void* p) {
    if (failRelease) return false;
    auto* h = static_cast<Header*>(p) - 1;
    reserved -= h->bytes;
#ifdef _WIN32
    _aligned_free(h);
#else
    std::free(h);
#endif
    return true;
  }
};
using Arena = l4d2_buffer::ShadowArena<TestPlatform>;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void badAllocation(F&& f) {
  bool caught = false;
  try { f(); } catch (const std::bad_alloc&) { caught = true; }
  require(caught, "failure did not throw bad_alloc");
}
int main() {
  try {
    Arena a;
    std::vector<Arena::Allocation> allocations;
    for (size_t size : {size_t(0), size_t(1), size_t(16), size_t(17), size_t(65535),
        size_t(65536), size_t(65537), size_t(196608), size_t(262144), size_t(262145), size_t(1048577)}) {
      auto p = a.allocate(size, true);
      require(reinterpret_cast<uintptr_t>(p.data) % 16 == 0, "payload alignment");
      require(std::all_of(p.data, p.data + size, [](uint8_t x) { return x == 0; }), "zero initialization");
      std::memset(p.data, 0xa5, size); allocations.push_back(p);
    }
    for (auto p : allocations) a.free(p);
    require(!a.stats().regions && !TestPlatform::reserved && !TestPlatform::metadataCount, "empty arena retained payload or metadata");
    auto first = a.allocate(64, true), second = a.allocate(64, true);
    std::memset(first.data, 0xf3, 64); std::memset(second.data, 0xa7, 64);
    auto* pointer = first.data;
    a.free(first);
    first = a.allocate(64, true);
    require(first.data == pointer && std::all_of(first.data, first.data + 64, [](uint8_t x) { return !x; }), "reuse did not clear old bytes");
    require(second.data[0] == 0xa7 && second.data[63] == 0xa7, "neighbor data changed");
    a.free(first); a.free(second);
    TestPlatform::failMetadata = true; badAllocation([&] { a.allocate(64, true); }); TestPlatform::failMetadata = false;
    TestPlatform::reserveLimit = 0; badAllocation([&] { a.allocate(64, true); }); TestPlatform::reserveLimit = SIZE_MAX;
    TestPlatform::failCommit = true; badAllocation([&] { a.allocate(64, true); }); TestPlatform::failCommit = false;
    require(!a.stats().regions && !TestPlatform::metadataCount, "fresh allocation failure retained arena");
    first = a.allocate(4096, true); std::memset(first.data, 0x4b, 4096);
    const auto before = a.stats();
    TestPlatform::failCommit = true; badAllocation([&] { a.allocate(4096, true); }); TestPlatform::failCommit = false;
    require(a.stats().allocations == before.allocations && a.stats().committed == before.committed && first.data[4095] == 0x4b, "existing commit failure changed ownership/content");
    second = a.allocate(4096, true); a.free(second); a.free(first);
    badAllocation([&] { a.allocate(SIZE_MAX, true); });
    TestPlatform::reserveLimit = 65536;
    first = a.allocate(16384, true); require(first.region->reserved == 65536, "fragmented reserve fallback"); a.free(first);
    TestPlatform::reserveLimit = SIZE_MAX;
    // Mixed lifetimes: free all but one in each region, preserve survivors, then
    // verify empty-region retirement rather than only a final global destroy.
    allocations.clear();
    for (unsigned i = 0; i < 256; ++i) { auto p = a.allocate(65536, true); p.data[0] = static_cast<uint8_t>(i); allocations.push_back(p); }
    const auto full = a.stats().reserved;
    for (unsigned i = 0; i < 256; ++i) if (i % 32) a.free(allocations[i]);
    require(a.stats().reserved < full && a.stats().allocations == 8, "empty regions did not retire with survivors");
    for (unsigned i = 0; i < 256; i += 32) { require(allocations[i].data[0] == static_cast<uint8_t>(i), "survivor moved or changed"); a.free(allocations[i]); }
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < 4; ++t) workers.emplace_back([&a, t] {
      for (unsigned i = 0; i < 2000; ++i) {
        auto p = a.allocate((i % 5 + 1) * 4096, true);
        p.data[0] = static_cast<uint8_t>(t); a.free(p);
      }
    });
    for (auto& worker : workers) worker.join();
    require(!a.stats().allocations && !a.stats().regions && !TestPlatform::reserved && !TestPlatform::metadataCount, "concurrent lifetime leak");
    first = a.allocate(64, true); TestPlatform::failRelease = true; a.free(first); TestPlatform::failRelease = false;
    require(a.stats().releaseFailures == 1 && a.stats().reserved, "failed release was charged as returned VA");
    first = a.allocate(64, true); a.free(first);
    require(!a.stats().regions && !TestPlatform::metadataCount, "failed release retry did not recover");
    first = a.allocate(1048577, true); TestPlatform::failRelease = true; a.free(first); TestPlatform::failRelease = false;
    require(a.stats().releaseFailures == 2 && a.stats().regions == 1, "dedicated failed release lost descriptor");
    first = a.allocate(64, true); a.free(first);
    require(!a.stats().regions && !TestPlatform::reserved && !TestPlatform::metadataCount, "dedicated release retry did not recover");
    allocations.clear();
    for (unsigned i = 0; i < 4; ++i) allocations.push_back(a.allocate(262144, true));
    for (unsigned i = 0; i < 3; ++i) a.free(allocations[i]);
    TestPlatform::failRelease = true; a.free(allocations[3]); TestPlatform::failRelease = false;
    allocations.clear();
    for (unsigned i = 0; i < 4; ++i) allocations.push_back(a.allocate(262144, true));
    require(a.stats().regions == 1, "failed full-region release lost free slots");
    for (auto p : allocations) a.free(p);
    require(!a.stats().regions && !TestPlatform::metadataCount, "full-region failed release cleanup");
    std::cout << "ARENA_PASS: initialization, boundaries, reuse, failures, fragmentation, survivors and concurrent lifetimes\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
