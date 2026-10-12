// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>
#include "buffer_mapped_shadow.h"

#ifdef _WIN32
struct TestPlatform : l4d2_buffer::WindowsSectionPlatform {
  inline static bool failCreate = false, failMap = false, failUnmap = false;
  static Handle create(size_t bytes) { return failCreate ? nullptr : WindowsSectionPlatform::create(bytes); }
  static void* map(Handle h, size_t bytes) { return failMap ? nullptr : WindowsSectionPlatform::map(h, bytes); }
  static bool unmap(void* p) { return !failUnmap && WindowsSectionPlatform::unmap(p); }
};
#else
// Portable semantics checks, not a substitute for Windows VA or cost evidence.
struct TestPlatform {
  struct Section { std::vector<uint8_t> bytes; };
  using Handle = Section*;
  inline static bool failCreate = false, failMap = false, failUnmap = false;
  inline static std::unordered_map<void*, Handle> views;
  static size_t granularity() { return 65536; }
  static Handle create(size_t bytes) { return failCreate ? nullptr : new Section { std::vector<uint8_t>(bytes) }; }
  static void* map(Handle h, size_t bytes) {
    if (failMap) { return nullptr; }
    auto* p = std::malloc(bytes);
    if (p) { std::memcpy(p, h->bytes.data(), bytes); views.emplace(p, h); }
    return p;
  }
  static bool unmap(void* p) {
    if (failUnmap) { return false; }
    auto* h = views.at(p); std::memcpy(h->bytes.data(), p, h->bytes.size());
    views.erase(p); std::free(p); return true;
  }
  static void close(Handle h) { delete h; }
  static size_t mappedBytes(void*, size_t bytes) { return bytes; }
};
#endif
using Shadow = l4d2_buffer::SectionShadow<TestPlatform>;
static void require(bool value, const char* text) {
  if (!value) { std::fprintf(stderr, "SECTION_CORE_FAIL %s\n", text); std::exit(2); }
}
int main() {
  {
    Shadow s(262144, 0);
    auto* first = s.acquire(); require(first && !first[0], "initial zero");
    first[0] = 0xa5;
    auto* second = s.acquire(); require(first == second, "nested lock moved pointer");
    s.release(); require(Shadow::stats().locks == 1 && second[0] == 0xa5, "nested unlock unpinned");
    TestPlatform::failUnmap = true; s.release();
    require(Shadow::stats().unmapFailures == 1 && Shadow::stats().mappedCount == 1, "false VA release on unmap failure");
    TestPlatform::failUnmap = false;
    require(s.acquire()[0] == 0xa5, "failed-unmap recovery"); s.release();
    require(!Shadow::stats().mappedCount, "retry did not unmap");
    TestPlatform::failMap = true; require(!s.acquire() && !Shadow::stats().locks, "map failure pin");
    TestPlatform::failMap = false;
    require(s.acquire()[0] == 0xa5, "remap history"); s.release();
  }
  TestPlatform::failCreate = true;
  bool failed = false; try { Shadow s(262144, 0); } catch (const std::bad_alloc&) { failed = true; }
  TestPlatform::failCreate = false;
  require(failed && !Shadow::stats().backingCount, "create failure balance");
  std::atomic<unsigned> failures { 0 };
  std::vector<std::thread> workers;
  for (unsigned t = 0; t < 4; ++t) {
    workers.emplace_back([&failures] {
      for (unsigned n = 0; n < 64; ++n) {
        Shadow s(262144, 1048576);
        auto* p = s.acquire();
        if (!p || p[0]) { ++failures; }
        if (p) { p[0] = 0x83; s.release(); }
      }
    });
  }
  for (auto& worker : workers) { worker.join(); }
  const auto after = Shadow::stats();
  require(!failures && !after.backingBytes && !after.mappedVaBytes && !after.locks && !after.idleBudgetBytes, "concurrent lifecycle balance");
  std::puts("VBIB_SECTION_CORE_PASS");
}
