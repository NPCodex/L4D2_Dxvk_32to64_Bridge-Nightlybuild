// Pure calls to the extracted production WndProc. No window or input creation.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <imm.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "overlay_input_diagnostics.h"
using l4d2_overlay::InputDiagnostics;

static unsigned assertions = 0;
static void check(bool condition, const char* reason) {
  ++assertions;
  if (!condition) { throw std::runtime_error(reason); }
}
static const HWND presenterWindow = reinterpret_cast<HWND>(uintptr_t(0x1234));
static const HWND unknownWindow = reinterpret_cast<HWND>(uintptr_t(0x5678));
static void* activePresenter = nullptr;
static constexpr LRESULT defaultResult = -0x3456;
struct Message { HWND window; UINT message; WPARAM key; LPARAM flags; };
static std::vector<Message> defaultMessages;
static LONG_PTR fixtureGetWindowLongPtrW(HWND window, int index) {
  return window == presenterWindow && index == GWLP_USERDATA ? reinterpret_cast<LONG_PTR>(activePresenter) : 0;
}
static LRESULT fixtureDefWindowProcW(HWND window, UINT message, WPARAM key, LPARAM flags) {
  defaultMessages.push_back({window, message, key, flags});
  return defaultResult;
}
#undef GetWindowLongPtrW
#define GetWindowLongPtrW fixtureGetWindowLongPtrW
#define DefWindowProcW fixtureDefWindowProcW
#include "production_ime_result.h"
#undef GetWindowLongPtrW
#undef DefWindowProcW

static void setup(Presenter& self, bool native = true, bool steam = true, bool capture = true) {
  activePresenter = &self;
  defaultMessages.clear();
  self.m_window.store(presenterWindow);
  self.m_steamInput = true;
  self.m_steamNativeInput = native;
  self.m_steamCapture.store(steam);
  self.m_capture.store(capture);
}
static void blocked(UINT message, WPARAM key, LPARAM flags, const char* reason) {
  const auto before = defaultMessages.size();
  const auto result = Presenter::windowProc(presenterWindow, message, key, flags);
  check(result == 0 && defaultMessages.size() == before, reason);
}
static void preserved(UINT message, WPARAM key, LPARAM flags, const char* reason, HWND window = presenterWindow) {
  const auto before = defaultMessages.size();
  const auto result = Presenter::windowProc(window, message, key, flags);
  check(result == defaultResult && defaultMessages.size() == before + 1, reason);
  const auto& received = defaultMessages.back();
  check(received.window == window && received.message == message && received.key == key && received.flags == flags,
    "default message parameters changed");
}

int main() {
  try {
    {
      Presenter self; setup(self);
      // Exact pre-fix source must fail here: this was an unwanted second route.
      blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR,
        "native Steam result reached DefWindowProc");
      blocked(WM_IME_COMPOSITION, 0x6587, GCS_RESULTSTR | GCS_COMPSTR,
        "mixed result/composition reached DefWindowProc");
      blocked(WM_IME_COMPOSITION, 0, GCS_RESULTSTR | GCS_COMPSTR | GCS_COMPATTR | GCS_CURSORPOS,
        "mixed result metadata reached DefWindowProc");
      blocked(WM_IME_COMPOSITION, 0xffff, GCS_RESULTSTR | GCS_RESULTCLAUSE | GCS_RESULTREADSTR,
        "result reading metadata reached DefWindowProc");
      blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR,
        "second equal result reopened default text route");
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_COMPSTR, "unconfirmed composition was blocked");
      preserved(WM_IME_COMPOSITION, 0, GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS,
        "composition metadata was blocked");
      preserved(WM_IME_COMPOSITION, 0, GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE,
        "composition reading metadata was blocked");
      preserved(WM_IME_COMPOSITION, 0, GCS_RESULTCLAUSE, "result clause without result string was blocked");
      preserved(WM_IME_COMPOSITION, 0, 0, "composition cancellation was blocked");
      preserved(WM_IME_COMPOSITION, 'x', CS_INSERTCHAR | CS_NOMOVECARET, "composition insertion flags were blocked");
      preserved(WM_IME_STARTCOMPOSITION, 17, 23, "composition start was blocked");
      preserved(WM_IME_ENDCOMPOSITION, 19, 29, "composition end was blocked");
      preserved(WM_IME_CHAR, 0x4e2d, 0x20001, "IME character was blocked");
      preserved(WM_IME_CHAR, 0x4e2d, 0x20001, "repeated IME character was filtered");
      preserved(WM_CHAR, 'a', 1, "ASCII character was blocked");
      preserved(WM_CHAR, 'a', 1, "repeated ASCII character was filtered");
      preserved(WM_CHAR, 0x4e2d, 1, "Unicode character was blocked");
      preserved(WM_CHAR, 0xd83d, 1, "surrogate high character was blocked");
      preserved(WM_CHAR, 0xde42, 1, "surrogate low character was blocked");
      preserved(WM_UNICHAR, 0x1f642, 1, "full Unicode character was blocked");
      preserved(WM_KEYDOWN, 'A', 1, "key down was blocked");
      preserved(WM_KEYUP, 'A', 0xc01e0001, "key up was blocked");
      preserved(WM_SYSKEYDOWN, VK_MENU, 0x20380001, "system key was blocked");
      preserved(WM_USER + 44, 0x4e2d, GCS_RESULTSTR, "non-composition flags were misclassified");
    }
    {
      Presenter self; setup(self, false, true, true);
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "non-native Steam result was blocked");
      setup(self, true, false, false);
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "inactive Steam result was blocked");
      setup(self, true, false, true);
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "ReShade-only result was blocked");
      setup(self, false, false, true);
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "non-native ReShade result was blocked");
      setup(self, true, true, false);
      blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "aggregate transition defeated Steam ownership");
      setup(self);
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "unbound window was intercepted", unknownWindow);
      activePresenter = nullptr;
      preserved(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "absent Presenter was intercepted");
    }
    {
      Presenter self(false); setup(self);
      unsigned logCalls = 0;
      self.m_log = [&](const char*) { ++logCalls; SetLastError(900); };
      SetLastError(123);
      blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "disabled logging changed routing");
      check(logCalls == 0 && GetLastError() == 123, "disabled diagnostic observed or changed last error");
    }
    {
      Presenter self(true); setup(self);
      SetLastError(124);
      blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "empty logger changed routing");
      check(GetLastError() == 124, "empty logger changed last error");
    }
    {
      Presenter self(true); setup(self);
      std::vector<std::string> logs;
      self.m_log = [&](const char* line) { logs.emplace_back(line); SetLastError(901); };
      for (unsigned i = 0; i < 40; ++i) {
        SetLastError(200 + i);
        blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "logged result reached default processing");
        check(GetLastError() == 200 + i, "diagnostic changed caller last error");
      }
      check(logs.size() == 32, "diagnostic did not stop after 32 results");
      for (size_t i = 0; i < logs.size(); ++i) {
        const std::string prefix = "L4D2_STEAM_IME event=result-owned-by-steam sequence=" + std::to_string(i + 1)
          + " flags=0x800 default_forwarded=0 tick=";
        check(logs[i].rfind(prefix, 0) == 0, "diagnostic contains unexpected metadata or text");
        const auto suffix = logs[i].substr(prefix.size());
        check(!suffix.empty() && suffix.find_first_not_of("0123456789") == std::string::npos,
          "diagnostic tick suffix contains non-metadata data");
      }
    }
    {
      Presenter self(true); setup(self);
      unsigned logCalls = 0;
      self.m_log = [&](const char*) { ++logCalls; SetLastError(902); throw std::runtime_error("fixture logger failure"); };
      for (unsigned i = 0; i < 36; ++i) {
        SetLastError(300 + i);
        blocked(WM_IME_COMPOSITION, 0x4e2d, GCS_RESULTSTR, "throwing logger changed result routing");
        check(GetLastError() == 300 + i, "throwing logger changed caller last error");
      }
      check(logCalls == 32, "throwing logger defeated bounded observations");
    }
    std::printf("IME_RESULT_BOUNDARY_PASS assertions=%u bits=%zu scope=production-wndproc-boundary-no-UI-no-Steam\n", assertions, sizeof(void*) * 8);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "IME_RESULT_BOUNDARY_FAIL: %s assertions=%u\n", error.what(), assertions);
    return 1;
  }
}
