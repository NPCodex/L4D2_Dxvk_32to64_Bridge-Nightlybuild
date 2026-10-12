// SPDX-License-Identifier: MIT
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "util_sharedheap.h"
#include "util_guid.h"
#include "util_devicecommand.h"
bridge_util::Guid gUniqueIdentifier;
using bridge_util::SharedHeap;
void checkHeap(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }

int wmain(int argc, wchar_t **argv) {
  try {
    if (argc > 1) checkHeap(gUniqueIdentifier.setGuid(&argv[1]), "heap GUID");
    SharedHeap::init();
#ifdef REMIX_BRIDGE_CLIENT
    checkHeap(SharedHeap::allocate(0) == SharedHeap::kInvalidId, "zero allocation accepted");
    checkHeap(SharedHeap::allocate(UINT32_MAX) == SharedHeap::kInvalidId, "oversized allocation accepted");
    const unsigned sizes[] = {3, 4097, 131073};
    for (unsigned i = 0; i < 3; ++i) {
      const auto id = SharedHeap::allocate(sizes[i]);
      checkHeap(id != SharedHeap::kInvalidId && SharedHeap::getAllocationSize(id) == sizes[i], "client logical capacity");
      auto *p = SharedHeap::getBuf(id);
      checkHeap(p != nullptr, "client pointer");
      std::memset(p, 0x31 + i, sizes[i]);
      checkHeap(heapMessages.back().command == Commands::Bridge_SharedHeap_Alloc &&
                heapMessages.back().handle == id && heapMessages.back().fields.size() == 2 &&
                heapMessages.back().fields[1] == sizes[i], "allocation wire size missing");
      std::printf("HEAP_ALLOCATION %u %u %u\n", id, heapMessages.back().fields[0], sizes[i]);
    }
    checkHeap(!SharedHeap::getBuf(123456) && SharedHeap::getAllocationSize(123456) == 0, "unknown client ID");
    std::puts("HEAP_READY"); std::fflush(stdout);
    if (argc > 1) checkHeap(std::getchar() == 'q', "client lifetime control");
#else
    SharedHeap::addNewHeapSegment(262144);
    if (argc > 2) {
      for (int at = 2; at + 2 < argc; at += 3) {
        const auto id = static_cast<uint32_t>(std::wcstoul(argv[at], nullptr, 10));
        const auto chunk = static_cast<uint32_t>(std::wcstoul(argv[at + 1], nullptr, 10));
        const auto bytes = static_cast<uint32_t>(std::wcstoul(argv[at + 2], nullptr, 10));
        checkHeap(SharedHeap::allocate(id, chunk, bytes), "peer allocation rejected");
        checkHeap(SharedHeap::getAllocationSize(id) == bytes, "peer capacity");
        const auto *p = SharedHeap::getBuf(id);
        for (unsigned b = 0; b < bytes; ++b) checkHeap(p[b] == 0x31 + id, "peer byte mismatch");
      }
      std::puts("HEAP_CROSS_PROCESS_PASS allocation-size/bytes/client-lifetime");
      return 0;
    }
    SharedHeap::addNewHeapSegment(262144);
    checkHeap(SharedHeap::allocate(1, 0, 3), "server allocation");
    checkHeap(SharedHeap::getAllocationSize(1) == 3 && SharedHeap::getBuf(1), "server logical capacity");
    checkHeap(!SharedHeap::allocate(1, 1, 3), "duplicate ID accepted");
    checkHeap(SharedHeap::allocate(2, 1, 4097), "multi-page allocation");
    checkHeap(!SharedHeap::allocate(3, 2, 4), "overlap accepted");
    checkHeap(!SharedHeap::allocate(3, 63, 4097), "cross-segment allocation accepted");
    checkHeap(SharedHeap::allocate(3, 63, 4096), "exact segment tail rejected");
    checkHeap(SharedHeap::allocate(4, 64, 3), "second segment start rejected");
    checkHeap(!SharedHeap::allocate(5, 128, 4) && !SharedHeap::allocate(5, UINT32_MAX, 4), "invalid chunk accepted");
    checkHeap(!SharedHeap::allocate(5, 4, 0) && !SharedHeap::allocate(5, 4, UINT32_MAX), "invalid byte count accepted");
    checkHeap(!SharedHeap::allocate(SharedHeap::kInvalidId, 4, 1), "sentinel ID accepted");
    checkHeap(!SharedHeap::getBuf(5) && SharedHeap::getAllocationSize(5) == 0, "unknown server ID");
    checkHeap(SharedHeap::allocate(5, 4, 1), "unknown lookup inserted phantom allocation");
    bridge_util::SharedMemory meta("SharedHeap_meta", (2u << 30) / 4096);
    auto *states = static_cast<std::atomic<uint8_t> *>(meta.data());
    states[0].store(1);
    checkHeap(SharedHeap::deallocate(1), "deallocate failed");
    checkHeap(!SharedHeap::getBuf(1) && !SharedHeap::getAllocationSize(1) && !SharedHeap::deallocate(1), "stale allocation accepted");
    checkHeap(states[0].load() == 2, "deallocation not published");
    checkHeap(SharedHeap::allocate(1, 0, 3), "freed range not reusable");
#endif
    std::puts("HEAP_BOUNDS_PASS unknown-id/logical-size/overlap/segment-tail/stale/oversize");
    return 0;
  } catch (const std::exception &e) { std::fprintf(stderr, "HEAP_FAIL %s\n", e.what()); return 1; }
}
