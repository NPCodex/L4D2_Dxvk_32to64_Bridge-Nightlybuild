// SPDX-License-Identifier: MIT
// Exercise the production allocator against an independent first-fit model.
#include <atomic>
#include <cstdio>
#include <chrono>
#include <random>
#include <stdexcept>
#include <vector>
#define LDB_IPC_V2_TEST
#include "ipc_full/upload_pool.h"

void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
std::string uniqueName() {
  static unsigned id = 0;
  return "optimization45-pool-" + std::to_string(ldb_ipc_v2::processId()) + "-" + std::to_string(++id);
}
constexpr uint32_t Page = ldb_full::PageBytes;
struct Item {
  ldb_full::Descriptor d{};
  uint32_t pages = 0, state = 0, refs = 0;
  uint8_t value = 0;
};
void modelTest() {
  constexpr unsigned Pages = 256, Items = 64;
  const auto name = uniqueName();
  ldb_ipc_v2::Config c;
  ldb_full::UploadPool w(name, ldb_full::PoolMetadata + Pages * Page, true, c);
  ldb_full::UploadPool r(name, ldb_full::PoolMetadata + Pages * Page, false, c);
  std::array<Item, Items> items{};
  std::array<unsigned, Pages> owner{};
  std::mt19937 rng(0x45a8);
  unsigned allocations = 0, grows = 0, moves = 0, pinnedFailures = 0, failures = 0;
  auto releasePages = [&](unsigned id) {
    auto &v = items[id];
    for (unsigned p = v.d.offset / Page; p < v.d.offset / Page + v.pages; ++p) owner[p] = 0;
    v.state = 0;
  };
  auto reap = [&] { for (unsigned id = 0; id < Items; ++id) if (items[id].state == 3) releasePages(id); };
  auto firstFit = [&](unsigned pages) {
    unsigned run = 0;
    for (unsigned p = 0; p < Pages; ++p) { run = owner[p] ? 0 : run + 1; if (run == pages) return p + 1 - pages; }
    return Pages;
  };
  auto verify = [&] {
    for (const auto &v : items) if (v.state == 1 || v.state == 2) {
      const auto *ptr = w.pointer(v.d);
      for (unsigned b = 0; b < v.d.bytes; ++b) check(ptr[b] == v.value, "active lease bytes changed");
    }
  };
  for (unsigned iteration = 0; iteration < 30000; ++iteration) {
    const unsigned id = rng() % Items;
    auto &v = items[id];
    if (!v.state) {
      reap();
      const unsigned pages = 1 + rng() % 24, bytes = pages * Page - rng() % 64;
      const unsigned expected = firstFit(pages);
      ldb_full::Descriptor d{};
      const bool ok = w.allocate(bytes, d);
      check(ok == (expected != Pages), "allocation differs from first-fit model");
      if (!ok) { ++failures; continue; }
      check(d.offset == expected * Page, "allocation changed first-fit offset");
      v = {d, pages, 1, 0, static_cast<uint8_t>(iteration)};
      v.d.bytes = bytes;
      std::memset(w.pointer(v.d), v.value, bytes);
      for (unsigned p = expected; p < expected + pages; ++p) owner[p] = id + 1;
      ++allocations;
    } else if (v.state == 1) {
      const unsigned action = rng() % 4;
      if (!action) { w.abort(v.d); releasePages(id); }
      else if (action == 1) { w.publish(v.d); r.acquire(v.d); v.state = 2; v.refs = 1; }
      else {
        const unsigned pages = v.pages + 1 + rng() % 16;
        const unsigned oldFirst = v.d.offset / Page, oldPages = v.pages;
        const bool borrowed = (rng() % 2) != 0;
        reap();
        bool inPlace = oldFirst + pages <= Pages;
        if (inPlace) for (unsigned p = oldFirst; p < oldFirst + pages; ++p) inPlace &= !owner[p] || owner[p] == id + 1;
        const auto expected = inPlace ? oldFirst : borrowed ? Pages : firstFit(pages);
        auto *before = w.pointer(v.d);
        const bool ok = w.grow(v.d, pages * Page, borrowed);
        check(ok == (expected != Pages), "growth differs from first-fit model");
        if (!ok) { ++failures; if (borrowed) ++pinnedFailures; continue; }
        check(v.d.offset == expected * Page, "growth changed expected offset");
        if (borrowed) check(before == w.pointer(v.d), "borrowed pointer moved");
        for (unsigned p = oldFirst; p < oldFirst + oldPages; ++p) owner[p] = 0;
        for (unsigned p = expected; p < expected + pages; ++p) owner[p] = id + 1;
        for (unsigned b = 0; b < v.d.bytes; ++b) check(w.pointer(v.d)[b] == v.value, "relocation lost contents");
        v.pages = pages;
        v.d.bytes = pages * Page;
        std::memset(w.pointer(v.d), v.value, v.d.bytes);
        ++grows; moves += oldFirst != expected;
      }
    } else if (v.state == 2) {
      if (v.refs < 3 && rng() % 3 == 0) { r.retain(v.d); ++v.refs; }
      else { check(r.release(v.d) == (v.refs == 1), "reference release mismatch"); if (!--v.refs) v.state = 3; }
    }
    if (!(iteration % 127)) verify();
  }
  verify();
  for (unsigned id = 0; id < Items; ++id) {
    auto &v = items[id];
    if (v.state == 1) w.abort(v.d);
    else if (v.state == 2) while (v.refs--) r.release(v.d);
  }
  ldb_full::Descriptor all{};
  check(w.allocate(Pages * Page, all) && all.offset == 0, "full capacity not recovered");
  w.abort(all);
  check(allocations > 1000 && grows > 100 && moves > 10 && pinnedFailures > 100, "insufficient model coverage");
  std::printf("POOL_MODEL_PASS steps=30000 allocations=%u grows=%u relocations=%u borrowed_failures=%u total_failures=%u\n", allocations, grows, moves, pinnedFailures, failures);
}
void benchmark(unsigned prefixPages, unsigned batch) {
  const auto name = uniqueName();
  ldb_ipc_v2::Config c;
  const unsigned capacityPages = prefixPages + 1024;
  ldb_full::UploadPool w(name, ldb_full::PoolMetadata + capacityPages * Page, true, c);
  ldb_full::Descriptor prefix{};
  if (prefixPages) check(w.allocate(prefixPages * Page, prefix), "benchmark prefix");
  std::array<ldb_full::Descriptor, 128> leases{};
  constexpr unsigned Count = 20000;
  ULONG64 cyclesBefore = 0, cyclesAfter = 0;
  QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore);
  const auto begin = std::chrono::steady_clock::now();
  unsigned operations = 0;
  for (unsigned n = 0; n < Count; n += batch) {
    for (unsigned i = 0; i < batch; ++i) {
      check(w.allocate(4, leases[i]), "benchmark allocate");
      check(leases[i].offset == (prefixPages + i) * Page, "benchmark first-fit offset");
      w.pointer(leases[i])[0] = static_cast<uint8_t>(i);
    }
    for (unsigned i = 0; i < batch; ++i) w.abort(leases[i]);
    operations += batch;
  }
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
  QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter);
  if (prefixPages) w.abort(prefix);
  std::printf("POOL_TIMING prefix_pages=%u batch=%u operations=%u cycles=%llu wall_ns=%lld\n", prefixPages, batch, operations, static_cast<unsigned long long>(cyclesAfter - cyclesBefore), static_cast<long long>(ns));
}
int main(int argc, char **argv) {
  try {
    if (argc > 1) {
      for (unsigned prefix : {0u, 64u, 16384u}) for (unsigned batch : {1u, 128u}) benchmark(prefix, batch);
    } else modelTest();
    return 0;
  } catch (const std::exception &e) { std::fprintf(stderr, "POOL_FAIL %s\n", e.what()); return 1; }
}
