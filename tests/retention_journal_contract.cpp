// SPDX-License-Identifier: MIT
// Exercise the actual Windows journal, CNG checksums, locking and corruption gates.
#define NOMINMAX
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "retention_database.h"

static void require(bool value, const char* label) {
  if (!value) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
static std::string key(unsigned value) {
  char result[65] {}; std::snprintf(result, sizeof(result), "%064x", value); return result;
}
static std::string signedLine(const std::string& fields) {
  l4d2_readback::Digest digest {};
  require(SUCCEEDED(l4d2_readback::hashBytes(reinterpret_cast<const uint8_t*>(fields.data()), fields.size(), digest)), "hash");
  return fields + '\t' + l4d2_readback::digestText(digest).data() + '\n';
}
static std::string record(unsigned id, std::string numbers = "1\t2\t3") {
  return signedLine(key(id) + '\t' + key(id + 100000) + "\tKEEP\tpost-upload-preserve-miss\t" + numbers);
}
static void write(const std::filesystem::path& path, const std::string& data) {
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(file != INVALID_HANDLE_VALUE, "create test journal");
  DWORD done = 0;
  const bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &done, nullptr)
    && done == data.size() && FlushFileBuffers(file);
  CloseHandle(file); require(ok, "write test journal");
}
static std::string read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "read test journal");
  return std::string(std::istreambuf_iterator<char>(file), {});
}
int main(int argc, char** argv) {
  using namespace l4d2_retention;
  const auto root = std::filesystem::current_path() / ("retention-journal-" + std::to_string(GetCurrentProcessId()));
  require(std::filesystem::create_directory(root), "unique test directory");
  const auto path = root / "contract.db";
  const std::string header = "L4D2_RETENTION_SHA256_V1\n";
  const auto prefix = key(1) + '\t' + key(100001) + "\tKEEP\tpost-upload-preserve-miss\t";
  unsigned cases = 0;
  const std::vector<std::string> validNumbers {
    "0\t0\t0", "00001\t00002\t00003", "18446744073709551615\t18446744073709551615\t18446744073709551615"
  };
  for (const auto& numbers : validNumbers) {
    write(path, header + record(1, numbers));
    Database db; require(db.open(path.wstring()) && db.records() == 1
      && db.lookup(key(1), key(100001)) == Match::Strong, "valid numeric boundary"); ++cases;
  }
  std::vector<std::string> invalid {
    "", "1\t2", "1\t2\t", "1\t2\t3\t4", "-1\t2\t3", "+1\t2\t3",
    " 1\t2\t3", "1 \t2\t3", "1\t2x\t3", "18446744073709551616\t2\t3",
    "1\t18446744073709551616\t3", "1\t2\t18446744073709551616"
  };
  for (const auto& numbers : invalid) {
    write(path, header + signedLine(prefix + numbers));
    const auto original = read(path);
    Database db; bool added = true;
    require(!db.open(path.wstring()) && !db.valid(), "reject invalid field/overflow");
    require(!db.keep(key(4), key(100004), "post-upload-preserve-miss", false, added)
      && !added && read(path) == original, "corrupt journal never appended"); ++cases;
  }
  for (unsigned kind = 0; kind < 6; ++kind) {
    auto data = header + record(1);
    if (kind == 0) { data[0] = 'X'; }
    if (kind == 1) { data.pop_back(); }
    if (kind == 2) { data[header.size()] = 'Z'; }
    if (kind == 3) { data = header + signedLine(key(1) + '\t' + key(100001) + "\tDROP\tpost-upload-preserve-miss\t1\t2\t3"); }
    if (kind == 4) { data = header + signedLine(key(1) + '\t' + key(100001) + "\tKEEP\tunknown\t1\t2\t3"); }
    if (kind == 5) { data = header + signedLine(prefix + std::string(1100, '0')); }
    write(path, data); Database db; require(!db.open(path.wstring()), "reject header/tail/checksum/kind/length"); ++cases;
  }
  // Two live journal handles must see the other writer's new records on keep().
  write(path, header);
  {
    Database first, second;
    require(first.open(path.wstring()) && second.open(path.wstring()), "shared journal open");
    bool added = false;
    require(first.keep(key(1), key(100001), "post-upload-preserve-miss", true, added) && added, "first writer");
    require(second.keep(key(2), key(100002), "recovery-failure", true, added) && added
      && second.lookup(key(1), key(100001)) == Match::Strong, "second sees first");
    require(first.keep(key(1), key(100001), "post-upload-preserve-miss", false, added) && !added
      && first.lookup(key(2), key(100002)) == Match::Strong, "first sees second");
    ++cases;
    // Same-size external edits must not be hidden by a size-only incremental cache.
    auto bytes = read(path); bytes[header.size()] = bytes[header.size()] == '0' ? '1' : '0';
    write(path, bytes);
    require(!first.keep(key(3), key(100003), "post-upload-preserve-miss", true, added)
      && !first.valid() && read(path) == bytes, "same-size corruption caught"); ++cases;
  }
  if (argc == 2 && std::string(argv[1]) == "--benchmark") {
    std::string data = header;
    for (unsigned i = 1; i <= 2000; ++i) { data += record(i); }
    write(path, data); LARGE_INTEGER frequency {}, begin {}, end {};
    require(QueryPerformanceFrequency(&frequency), "clock"); QueryPerformanceCounter(&begin);
    for (unsigned i = 0; i < 11; ++i) {
      Database db; require(db.open(path.wstring()) && db.records() == 2000, "benchmark journal");
    }
    QueryPerformanceCounter(&end);
    std::printf("JOURNAL_BENCHMARK records=2000 opens=11 ms=%.6f\n", 1000.0 * (end.QuadPart - begin.QuadPart) / frequency.QuadPart);
  }
  std::filesystem::remove(path); std::filesystem::remove(root);
  std::printf("RETENTION_JOURNAL_CONTRACT_PASS cases=%u\n", cases);
  return 0;
}
