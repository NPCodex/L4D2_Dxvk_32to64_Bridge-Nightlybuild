// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using fspath = fs::path;
static USHORT testNativeMachine = IMAGE_FILE_MACHINE_AMD64;
static fs::path modulePath, testExecutablePath;
static unsigned assertions = 0;
static BOOL WINAPI testIsWow64Process2(HANDLE, USHORT* process, USHORT* native) {
  *process = IMAGE_FILE_MACHINE_I386;
  *native = testNativeMachine;
  return TRUE;
}
static DWORD testGetModuleFileName(HMODULE, char* value, size_t count) {
  const auto text = testExecutablePath.string();
  if (text.empty() || text.size() >= count) { return 0; }
  std::memcpy(value, text.c_str(), text.size() + 1);
  return static_cast<DWORD>(text.size());
}
static fs::path getModuleFilePath(HMODULE) { return modulePath; }
static std::vector<char> createPathVec() { return std::vector<char>(32768); }

namespace bridge_util {
struct Logger {
  static void info(const std::string&) {}
  static void warn(const std::string&) {}
  static void err(const std::string&) {}
  template<class F> static void debugLazy(F&&) {}
};
struct Config {
  enum class App { Client, Server };
  static inline fs::path s_userConfigPath;
  static inline bool s_clientConfig = false;
  bool loaded = false;
  std::string payload;
  static Config getUserConfig(App app);
  static Config loadFromFile(const fs::path& path) {
    Config result;
    std::ifstream file(path);
    result.loaded = file.good();
    std::getline(file, result.payload);
    return result;
  }
};
}

#undef GetModuleFileName
#define GetModuleFileName testGetModuleFileName
#define IsWow64Process2 testIsWow64Process2
#include "runtime_filesys_generated.h"
#undef IsWow64Process2
#undef GetModuleFileName

static void check(bool condition, const char* message) {
  ++assertions;
  if (!condition) { throw std::runtime_error(message); }
}

int main(int argc, char** argv) {
  if (argc != 2) { return 2; }
  const std::string mode = argv[1];
  const fs::path game = fs::current_path() / "game";
  const fs::path bin = game / "bin";
  const fs::path x64 = bin / ".l4d2bridge";
  const fs::path native = bin / ".trex_arm64";
  try {
    // All directories created by the production initializer stay in this case's
    // working directory, regardless of the developer's inherited environment.
    SetEnvironmentVariableA("DXVK_CAPTURE_PATH", nullptr);
    SetEnvironmentVariableA("DXVK_LOG_PATH", nullptr);
    fs::create_directories(x64);
    modulePath = bin / "d3d9.dll";
    testExecutablePath = game / "left4dead2.exe";
    std::ofstream(x64 / "bridge.conf") << "selected-x64\n";
    check(!dxvk::util::RtxFileSys::isInitialized(), "fresh process is uninitialized");
    if (mode == "root-client") { modulePath = game / "d3d9.dll"; }
    if (mode == "native-preferred" || mode == "native-fallback") {
      testNativeMachine = IMAGE_FILE_MACHINE_ARM64;
      if (mode == "native-preferred") {
        fs::create_directories(native);
        std::ofstream(native / "bridge.conf") << "selected-native\n";
      }
    }
    if (mode == "missing-root") { testExecutablePath = game / "absent" / "left4dead2.exe"; }
    if (mode == "missing-remix") { modulePath = game / "absent" / "d3d9.dll"; }
    if (mode == "missing-executable") { testExecutablePath.clear(); }
    const bool initialized = initFileSys(reinterpret_cast<HMODULE>(1));
    if (mode == "missing-root" || mode == "missing-remix" || mode == "missing-executable") {
      check(!initialized, "failed startup must return false");
      check(!dxvk::util::RtxFileSys::isInitialized(), "failed startup must stay uninitialized");
    } else {
      check(initialized, "startup succeeds");
      check(dxvk::util::RtxFileSys::isInitialized(), "filesystem initialized before config");
      check(dxvk::util::RtxFileSys::rootPath() == fs::weakly_canonical(game), "game root preserved");
      check(dxvk::util::RtxFileSys::remixPath() == fs::weakly_canonical(bin), "root and bin clients resolve package bin");
      const auto expected = mode == "native-preferred" ? native : x64;
      check(dxvk::util::RtxFileSys::runtimePath() == fs::weakly_canonical(expected), "native preference and x64 fallback");
      auto client = bridge_util::Config::getUserConfig(bridge_util::Config::App::Client);
      check(client.loaded, "client reads selected runtime config");
      check(client.payload == (mode == "native-preferred" ? "selected-native" : "selected-x64"), "selected config content");
      check(bridge_util::Config::s_userConfigPath == expected / "bridge.conf", "persistent config path matches runtime");
      check(bridge_util::Config::s_clientConfig, "client owns persistent config");
      auto server = bridge_util::Config::getUserConfig(bridge_util::Config::App::Server);
      check(server.payload == client.payload, "host and client select same config");
      check(!bridge_util::Config::s_clientConfig, "host is not persistent config writer");
      if (mode == "reinitialize") {
        const auto other = game / "other";
        fs::create_directory(other);
        dxvk::util::RtxFileSys::init(other.string(), other.string());
        check(dxvk::util::RtxFileSys::runtimePath() == expected, "reinit does not redirect runtime");
        check(dxvk::util::RtxFileSys::rootPath() == game, "reinit does not redirect root");
      }
    }
    std::printf("RUNTIME_FILESYS_PASS mode=%s assertions=%u bits=%zu\n", mode.c_str(), assertions, sizeof(void*) * 8);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "RUNTIME_FILESYS_FAIL mode=%s assertion=%u reason=%s\n", mode.c_str(), assertions, error.what());
    return 1;
  }
}
