// Exercises the extracted production hook with deterministic Win32 adapters.
// No SendInput: injected events intentionally bypass the production hook.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
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

static unsigned assertions = 0;
static void check(bool condition, const char* reason) {
  ++assertions;
  if (!condition) { throw std::runtime_error(reason); }
}
static HWND handle(uintptr_t value) { return reinterpret_cast<HWND>(value); }
static const HWND game = handle(1), presenter = handle(2), ownChild = handle(3),
  foreignChild = handle(4), unrelated = handle(5), transientIme = handle(6),
  otherThreadChild = handle(7), otherProcessChild = handle(8);
static HWND currentForeground = game, currentFocus = presenter;
static constexpr DWORD presenterThread = 30, presenterProcess = 40;
static std::array<bool, 256> physical {};
static unsigned nextCalls = 0;
struct Posted { HWND window; UINT message; WPARAM key; LPARAM bits; };
static std::vector<Posted> postedEvents;
static HWND mockGetForegroundWindow() { return currentForeground; }
[[maybe_unused]] static HWND mockGetFocus() { return currentFocus; }
static HWND mockGetAncestor(HWND window, UINT) {
  return window == presenter || window == ownChild || window == foreignChild ? game : window;
}
[[maybe_unused]] static BOOL mockIsChild(HWND parent, HWND child) {
  return parent == presenter && (child == ownChild || child == foreignChild || child == otherThreadChild || child == otherProcessChild);
}
[[maybe_unused]] static BOOL mockIsWindow(HWND window) { return window != nullptr; }
[[maybe_unused]] static DWORD mockGetCurrentThreadId() { return presenterThread; }
[[maybe_unused]] static DWORD mockGetCurrentProcessId() { return presenterProcess; }
[[maybe_unused]] static DWORD mockGetWindowThreadProcessId(HWND window, LPDWORD process) {
  const bool owned = window == presenter || window == ownChild || window == transientIme;
  if (process) { *process = owned || window == otherThreadChild ? presenterProcess : 90; }
  return owned || window == otherProcessChild ? presenterThread : 80;
}
static SHORT mockGetAsyncKeyState(int key) {
  return key >= 0 && key < 256 && physical[static_cast<size_t>(key)] ? SHORT(-32768) : 0;
}
static LRESULT mockCallNextHookEx(HHOOK, int, WPARAM, LPARAM) { ++nextCalls; return 123; }
static BOOL mockPostMessageW(HWND window, UINT message, WPARAM key, LPARAM bits) {
  postedEvents.push_back({window, message, key, bits}); return TRUE;
}
struct InputDiagnostics {};
#define GetForegroundWindow mockGetForegroundWindow
#define GetFocus mockGetFocus
#define GetAncestor mockGetAncestor
#define IsChild mockIsChild
#define IsWindow mockIsWindow
#define GetCurrentThreadId mockGetCurrentThreadId
#define GetCurrentProcessId mockGetCurrentProcessId
#define GetWindowThreadProcessId mockGetWindowThreadProcessId
#define GetAsyncKeyState mockGetAsyncKeyState
#define CallNextHookEx mockCallNextHookEx
#define PostMessageW mockPostMessageW
#include "production_overlay_keyboard_route.h"
#undef GetForegroundWindow
#undef GetFocus
#undef GetAncestor
#undef IsChild
#undef IsWindow
#undef GetCurrentThreadId
#undef GetCurrentProcessId
#undef GetWindowThreadProcessId
#undef GetAsyncKeyState
#undef CallNextHookEx
#undef PostMessageW

static void setup(Presenter& self, bool native = false) {
  currentForeground = game; currentFocus = presenter;
  physical.fill(false); postedEvents.clear(); nextCalls = 0;
  self.m_parent = game; self.m_window.store(presenter);
  self.m_threadId.store(presenterThread); self.m_capture.store(true);
  self.m_steamNativeInput = native; self.m_steamCapture.store(native);
  Presenter::s_active.store(&self);
}
struct Route { LRESULT result; unsigned next; size_t posts; };
static Route event(UINT key, bool up = false, DWORD extraFlags = 0, int code = HC_ACTION) {
  const auto beforeNext = nextCalls;
  const auto beforePosts = postedEvents.size();
  KBDLLHOOKSTRUCT input {};
  input.vkCode = key; input.scanCode = key == VK_SPACE ? 0x39 : key == VK_TAB ? 0x0f : 0x1e;
  input.flags = extraFlags | (up ? LLKHF_UP : 0);
  const auto result = Presenter::keyHook(code,
    (extraFlags & LLKHF_ALTDOWN) ? (up ? WM_SYSKEYUP : WM_SYSKEYDOWN) : (up ? WM_KEYUP : WM_KEYDOWN),
    reinterpret_cast<LPARAM>(&input));
  // WH_KEYBOARD_LL runs before the current event updates asynchronous state.
  // Our downstream adapter accepts events. Forwarded injected events also
  // update the modeled OS state, including a synthetic Windows-key release.
  if (result == 123 && code == HC_ACTION && key < 256) {
    physical[key] = !up;
    if (key == VK_LSHIFT || key == VK_RSHIFT) { physical[VK_SHIFT] = physical[VK_LSHIFT] || physical[VK_RSHIFT]; }
  }
  return {result, nextCalls - beforeNext, postedEvents.size() - beforePosts};
}
static void passed(const Route& route, const char* reason) {
  check(route.result == 123 && route.next == 1 && route.posts == 0, reason);
}
static void forwarded(const Route& route, const char* reason) {
  check(route.result == 1 && route.next == 0 && route.posts == 1, reason);
}

int main() {
  try {
    // This first sequence is also the exact, unmodified old-source control.
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "left Windows key was intercepted");
      passed(event(VK_SPACE), "Win+Space down was swallowed or reposted");
      passed(event(VK_SPACE), "Win+Space repeat was swallowed or reposted");
      passed(event(VK_SPACE, true), "Win+Space up was swallowed or reposted");
      passed(event(VK_LWIN, true), "left Windows key release was intercepted");
      forwarded(event('A'), "ordinary fallback stopped suppressing captured input");
      forwarded(event('A', true), "ordinary fallback stopped forwarding key release");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_RWIN), "right Windows key was intercepted");
      passed(event(VK_SPACE), "right Win+Space was intercepted");
      passed(event(VK_RWIN, true), "right Windows release was intercepted");
      passed(event(VK_SPACE), "system key repeat lost routing after Windows release");
      passed(event(VK_SPACE, true), "system key release lost routing after Windows release");
      forwarded(event(VK_SPACE), "released system key was incorrectly latched forever");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "left Windows down failed in dual-Windows sequence");
      passed(event(VK_RWIN), "right Windows down failed in dual-Windows sequence");
      passed(event(VK_LWIN, true), "left Windows release failed in dual-Windows sequence");
      passed(event('D'), "remaining right Windows modifier was lost");
      passed(event('D', true), "Windows chord key release was intercepted");
      passed(event(VK_RWIN, true), "right Windows release failed in dual-Windows sequence");
      forwarded(event('D'), "both Windows modifiers remained stuck");
    }
    {
      Presenter self; setup(self);
      physical[VK_LWIN] = true; // Held before the hook began observing events.
      passed(event(VK_SPACE), "already-held Windows key was not recognized");
      passed(event(VK_LWIN, true), "already-held Windows release was intercepted");
      passed(event(VK_SPACE, true), "preexisting chord key release was intercepted");
      forwarded(event('A'), "preexisting Windows modifier remained stuck");
    }
    {
      Presenter self; setup(self);
      physical[VK_RWIN] = true;
      passed(event(VK_RWIN, true), "first observed Windows release was intercepted");
      forwarded(event('A'), "first observed Windows release left a stale modifier");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "Windows down failed before foreground transition");
      passed(event(VK_SPACE), "Space down failed before foreground transition");
      currentForeground = unrelated;
      passed(event(VK_LWIN, true), "background Windows release was swallowed");
      currentForeground = game;
      passed(event(VK_SPACE, true), "chord release was swallowed after foreground return");
      forwarded(event('A'), "background Windows release left stale modifier");
    }
    {
      Presenter self; setup(self); currentForeground = unrelated;
      passed(event(VK_LWIN), "background Windows down was intercepted");
      passed(event('Q'), "background ordinary key was intercepted");
      currentForeground = game;
      passed(event('Q', true), "background system chord release was reposted on foreground return");
      passed(event(VK_LWIN, true), "background-held Windows release was intercepted");
      forwarded(event('A'), "foreground return left Windows modifier stale");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN, false, LLKHF_INJECTED), "injected Windows down was intercepted");
      passed(event(VK_SPACE, false, LLKHF_INJECTED), "injected Space was intercepted");
      passed(event(VK_SPACE, true, LLKHF_INJECTED), "injected release was intercepted");
      passed(event(VK_LWIN, true, LLKHF_INJECTED), "injected Windows release was intercepted");
      forwarded(event('B'), "injected shortcut left routing state");
      passed(event(VK_TAB, false, LLKHF_ALTDOWN), "Alt+Tab was intercepted");
      passed(event('A', false, 0, -1), "negative hook code was handled");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "physical Windows down failed before injected release");
      passed(event(VK_LWIN, true, LLKHF_INJECTED), "injected Windows release was intercepted after physical down");
      check(!physical[VK_LWIN], "adapter failed to apply injected release to OS asynchronous state");
      forwarded(event('A'), "injected Windows release left a cached physical modifier");
      forwarded(event('A', true), "mixed Windows release corrupted ordinary key-up");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "physical Windows down failed before injected chord release");
      passed(event(VK_SPACE), "physical chord down failed before injected release");
      passed(event(VK_LWIN, true), "physical Windows up failed before injected chord release");
      passed(event(VK_SPACE, true, LLKHF_INJECTED), "injected chord release was intercepted");
      forwarded(event(VK_SPACE), "injected chord release left stale per-key routing");
      forwarded(event(VK_SPACE, true), "post-injection ordinary Space release was misrouted");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_RWIN, false, LLKHF_INJECTED), "injected Windows down failed before physical chord");
      check(physical[VK_RWIN], "adapter failed to apply injected Windows down to OS state");
      passed(event(VK_SPACE), "physical Space ignored OS-held injected Windows modifier");
      passed(event(VK_RWIN, true, LLKHF_INJECTED), "injected Windows up failed during physical chord");
      passed(event(VK_SPACE, true), "physical chord release was lost after injected Windows release");
      forwarded(event('A'), "injected Windows lifecycle left shortcut routing active");
    }
    {
      Presenter self; setup(self);
      passed(event(VK_LWIN), "Windows down failed before injected unrelated key");
      passed(event('Q', false, LLKHF_INJECTED), "injected chord key down was intercepted");
      passed(event(VK_LWIN, true), "Windows release failed before injected unrelated key-up");
      forwarded(event('Q', true), "injected key-down improperly began shortcut tracking");
    }
    {
      Presenter self; setup(self, true);
      passed(event('A'), "native presenter key was synthesized");
      passed(event('A', true), "native presenter key release was synthesized");
      currentFocus = ownChild;
      passed(event('B'), "native owned child key was synthesized");
      passed(event('B', true), "native owned child release was synthesized");
      currentFocus = transientIme;
      passed(event('C'), "native IME transition fell back to synthetic input");
      passed(event('C', true), "native IME transition release fell back to synthetic input");
      currentFocus = nullptr;
      passed(event('D'), "native missing-focus transition fell back to synthetic input");
      currentFocus = foreignChild;
      passed(event('E'), "native capture with foreign focus synthesized input");
      self.m_steamCapture.store(false); self.m_capture.store(true);
      forwarded(event('F'), "non-Steam fallback was disabled by native setting");
    }
#if OVERLAY_ROUTE_HAS_NATIVE_FOCUS
    {
      Presenter self; setup(self, true);
      check(self.nativeFocus(presenter), "presenter native focus was rejected");
      currentFocus = ownChild;
      check(self.nativeFocus(presenter), "owned Presenter child focus was rejected");
      currentFocus = foreignChild;
      check(!self.nativeFocus(presenter), "foreign child focus was accepted");
      currentFocus = otherThreadChild;
      check(!self.nativeFocus(presenter), "another Host thread's child focus was accepted");
      currentFocus = otherProcessChild;
      check(!self.nativeFocus(presenter), "foreign process child focus was accepted");
      currentFocus = transientIme;
      check(!self.nativeFocus(presenter), "unparented IME window was accepted as Presenter focus");
      currentFocus = nullptr;
      check(!self.nativeFocus(presenter), "null focus was accepted");
      currentFocus = presenter;
      check(!self.nativeFocus(nullptr), "null Presenter was accepted");
    }
#endif
    {
      Presenter self; setup(self); self.m_capture.store(false);
      const auto route = event('A');
      check(route.result == 123 && route.next == 1 && route.posts == 1,
        "closed-overlay fallback no longer forwards without swallowing");
      self.m_fallback = true;
      forwarded(event(VK_HOME), "fallback hotkey failed to capture");
      check(self.testCaptureCalls == 1 && self.m_capture.load(), "fallback capture transition failed");
      forwarded(event(VK_HOME), "fallback hotkey repeat routing changed");
      check(self.testCaptureCalls == 1, "fallback hotkey repeat toggled capture twice");
      forwarded(event(VK_HOME, true), "fallback hotkey release routing changed");
    }
    Presenter::s_active.store(nullptr);
    passed(event('A'), "absent presenter did not pass event");
    std::printf("OVERLAY_KEYBOARD_ROUTE_PASS bits=%u assertions=%u\n", unsigned(sizeof(void*) * 8), assertions);
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "OVERLAY_KEYBOARD_ROUTE_FAIL: %s\n", error.what());
    return 1;
  }
}
