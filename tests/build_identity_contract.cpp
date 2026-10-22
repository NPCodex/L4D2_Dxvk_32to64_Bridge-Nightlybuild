// SPDX-License-Identifier: MIT
// The transport and startup guard are extracted unchanged from prepared source.
#include "transport.h"
#include <cstdio>
#include <cwchar>
#include "identities.h"

static void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
struct Logger { static void err(const std::string&) {} };
template<typename... T> std::string format_string(const char*, T...) { return {}; }
// The startup fixture supplies stack arguments, so only logging/freeing are adapters.
static void fixtureFree(const wchar_t**) {}
#define LocalFree fixtureFree
#include "startup_guard.h"
#undef LocalFree

ldb_ipc_v2::Config config(unsigned identity, unsigned kind, unsigned direction) {
  require(identity < kIdentityCount, "invalid fixture identity");
  ldb_ipc_v2::Config value;
  value.blockCount = 2;
  value.blockBytes = 128;
  value.kind = kind;
  value.direction = direction;
  std::memcpy(value.build, kIdentities[identity].words, sizeof(value.build));
  return value;
}

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::string(argv[1]) == "startup") {
      const unsigned self = std::stoul(argv[2]), peer = std::stoul(argv[3]);
      require(self < kIdentityCount && peer < kIdentityCount, "startup fixture identity");
      const bool accepted = std::stoul(argv[4]) != 0;
      require((startupGuard(kIdentities[self].startup, kIdentities[peer].startup) == 0) == accepted,
              "startup identity admission");
      std::printf("IDENTITY_STARTUP_PASS accepted=%u bits=%u\n", accepted, unsigned(sizeof(void*) * 8));
      return 0;
    }
    require(argc == 7, "fixture arguments");
    const std::string mode = argv[1], name = argv[2];
    const unsigned identity = std::stoul(argv[3]), kind = std::stoul(argv[4]), direction = std::stoul(argv[5]);
    const bool expected = std::stoul(argv[6]) != 0;
    if (mode == "owner") {
      ldb_ipc_v2::Channel channel(name, config(identity, kind, direction), true);
      ldb_ipc_v2::Writer writer(channel);
      auto message = writer.begin(73, 1, 9, 4);
      require(message.scalar(0x76c5ab19u) && message.finish() == ldb_ipc_v2::Result::Success,
              "queued sentinel publication");
      std::puts("IDENTITY_OWNER_READY");
      std::fflush(stdout);
      require(std::getchar() == '\n', "owner completion");
      return 0;
    }
    require(mode == "peer", "unknown fixture mode");
    unsigned dispatches = 0;
    bool rejected = false;
    try {
      ldb_ipc_v2::Channel channel(name, config(identity, kind, direction), false);
      ldb_ipc_v2::Reader reader(channel);
      require(reader.consume([&](ldb_ipc_v2::Message& message) {
        ++dispatches;
        uint32_t value = 0;
        return message.header.command == 73 && message.scalar(value) && value == 0x76c5ab19u;
      }) == ldb_ipc_v2::Result::Success, "matched sentinel consumption");
    } catch (const std::runtime_error& error) {
      if (std::string(error.what()) != "IPC v2 protocol/build/config mismatch") throw;
      rejected = true;
    }
    require(expected ? !rejected && dispatches == 1 : rejected && dispatches == 0,
            "identity must reject before payload dispatch");
    std::printf("IDENTITY_CHANNEL_PASS accepted=%u dispatches=%u bits=%u\n",
                expected, dispatches, unsigned(sizeof(void*) * 8));
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "IDENTITY_FAIL: %s\n", error.what());
    return 1;
  }
}
