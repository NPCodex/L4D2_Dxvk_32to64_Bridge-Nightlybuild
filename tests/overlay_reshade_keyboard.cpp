// SPDX-License-Identifier: MIT
// A deterministic Win32 delivery model, not injected or real desktop input.
// ReShade function bodies are extracted from pinned BSD-3-Clause upstream source.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

static HWND game = reinterpret_cast<HWND>(1), host = reinterpret_cast<HWND>(2);
static HWND currentFocus = game, currentForeground = game;
static constexpr DWORD ownerThread = 30, ownerProcess = 40;
static std::array<bool, 256> asyncKeys {}, hostKeys {}, gameKeys {};
static bool blockKeyboard = false;
static unsigned assertions = 0, bridgeCalls = 0, nativeHost = 0, nativeGame = 0;
static unsigned steamChords = 0, hostTabWithoutShift = 0;
static DWORD ticks = 100;
struct Queued { HWND window; UINT message; WPARAM key; LPARAM bits; bool native; };
static std::vector<Queued> pending;
static void check(bool condition, const char *reason) {
  ++assertions; if (!condition) { throw std::runtime_error(reason); }
}
static HWND mockGetForegroundWindow() { return currentForeground; }
static HWND mockGetFocus() { return currentFocus; }
static HWND mockSetFocus(HWND value) { const auto old = currentFocus; currentFocus = value; return old; }
static HWND mockGetAncestor(HWND window, UINT) { return window == host ? game : window; }
static BOOL mockIsChild(HWND, HWND) { return FALSE; }
static BOOL mockIsWindow(HWND window) { return window != nullptr; }
static DWORD mockGetCurrentThreadId() { return ownerThread; }
static DWORD mockGetCurrentProcessId() { return ownerProcess; }
static DWORD mockGetWindowThreadProcessId(HWND window, LPDWORD process) {
  if (process) { *process = window == host ? ownerProcess : 90; }
  return window == host ? ownerThread : 80;
}
static SHORT WINAPI mockGetAsyncKeyState(int key) { return key >= 0 && key < 256 && asyncKeys[key] ? SHORT(-32768) : 0; }
static SHORT WINAPI mockGetKeyState(int key) { return key >= 0 && key < 256 && hostKeys[key] ? SHORT(-32768) : 0; }
static DWORD mockGetTickCount() { return ticks; }
static LRESULT mockCallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }
static BOOL mockPostMessageW(HWND window, UINT message, WPARAM key, LPARAM bits) {
  pending.push_back({window, message, key, bits, false}); return window != nullptr;
}
static BOOL mockEnableWindow(HWND, BOOL) { return TRUE; }
static int mockShowCursor(BOOL) { return 0; }
struct InputDiagnostics {};
#define GetForegroundWindow mockGetForegroundWindow
#define GetFocus mockGetFocus
#define SetFocus mockSetFocus
#define GetAncestor mockGetAncestor
#define IsChild mockIsChild
#define IsWindow mockIsWindow
#define GetCurrentThreadId mockGetCurrentThreadId
#define GetCurrentProcessId mockGetCurrentProcessId
#define GetWindowThreadProcessId mockGetWindowThreadProcessId
#define GetAsyncKeyState mockGetAsyncKeyState
#define GetKeyState mockGetKeyState
#define GetTickCount mockGetTickCount
#define CallNextHookEx mockCallNextHookEx
#define PostMessageW mockPostMessageW
#define EnableWindow mockEnableWindow
#define ShowCursor mockShowCursor
#include "production_overlay_reshade_keyboard.h"

namespace reshade {
struct input {
  using window_handle = void *;
  static constexpr unsigned key_ctrl = VK_CONTROL, key_shift = VK_SHIFT, key_alt = VK_MENU, key_escape = VK_ESCAPE;
  uint8_t _keys[256] {}, _last_keys[256] {};
  unsigned _keys_time[256] {}, _mouse_position[2] {}, _last_mouse_position[2] {};
  uint64_t _frame_count = 0; short _mouse_wheel_delta = 0; std::wstring _text_input;
  bool is_key_down(unsigned) const;
  bool is_key_pressed(unsigned) const;
  bool is_key_pressed(unsigned, bool, bool, bool, bool) const;
  bool is_key_pressed(const unsigned key[4], bool force) const { return is_key_pressed(key[0], key[1] != 0, key[2] != 0, key[3] != 0, force); }
  bool is_key_repeated(unsigned) const;
  bool is_blocking_keyboard_input() const { return blockKeyboard; }
  bool is_blocking_mouse_input() const { return false; }
  bool is_blocking_mouse_cursor_warping() const { return false; }
  static bool is_blocking_any_keyboard_input(window_handle) { return blockKeyboard; }
  void consume(const MSG &);
  void next_frame();
};
namespace hooks {
template<class T> static bool is_hooked(T) { return false; }
template<class T> static T call(T, T original) { return original; }
}
}
static SHORT WINAPI HookGetKeyState(int key) { return mockGetKeyState(key); }
static SHORT WINAPI HookGetAsyncKeyState(int key) { return mockGetAsyncKeyState(key); }
// Same lookup key/value ABI as ReShade; concurrency is outside this single-thread model.
static std::unordered_map<UINT64, std::pair<HHOOK, HOOKPROC>> s_windows_hooks;
static std::shared_mutex s_windows_mutex;
static std::unordered_map<HWND, std::weak_ptr<reshade::input>> s_windows;
static std::atomic<bool> s_block_mouse {false}, s_block_keyboard {false}, s_block_cursor_warping {false};
#include "production_reshade_input.h"

struct Runtime {
  using input = reshade::input;
  std::shared_ptr<input> ownedInput = std::make_shared<input>();
  input *_input = ownedInput.get();
  bool _show_overlay = false, _ignore_shortcuts = false, _force_shortcut_modifiers = false;
  unsigned _overlay_key_data[4] { VK_HOME, 0, 0, 0 };
  unsigned _fps_key_data[4] {}, _frametime_key_data[4] {};
  int _input_processing_mode = 2, _show_fps = 0, _show_frametime = 0;
  struct Context { struct IOState { bool NavVisible = false, WantCaptureMouse = false, WantCaptureKeyboard = false; } IO; unsigned ActiveId = 0; } context;
  Context *_imgui_context = &context;
  unsigned transitions = 0;
  void frame() {
    bool show_overlay = _show_overlay;
#include "production_reshade_gui_keyboard.h"
    if (show_overlay != _show_overlay) {
      // Model the documented accepted addon event, not arbitrary capture toggles.
      check(!Presenter::onOverlay(nullptr, show_overlay, 0), "Bridge vetoed ReShade state change");
      _show_overlay = show_overlay; ++transitions;
    }
    blockKeyboard = _show_overlay;
    _input->next_frame(); ++ticks;
  }
};
static Runtime *runtime = nullptr;
static LRESULT CALLBACK bridgeHook(int code, WPARAM message, LPARAM data) {
  ++bridgeCalls; return Presenter::keyHook(code, message, data);
}
static void setKey(std::array<bool, 256> &keys, unsigned key, bool down) {
  keys[key] = down;
  if (key == VK_LSHIFT || key == VK_RSHIFT) { keys[VK_SHIFT] = keys[VK_LSHIFT] || keys[VK_RSHIFT]; }
}
static void drain(Presenter &presenter) {
  for (size_t i = 0; i < pending.size(); ++i) {
    const auto q = pending[i];
    if (q.window == host && q.message == Presenter::kMode) { presenter.applyMode(); continue; }
    if (q.message < WM_KEYFIRST || q.message > WM_KEYLAST || q.key >= 256) { continue; }
    const bool down = q.message == WM_KEYDOWN || q.message == WM_SYSKEYDOWN;
    if (q.native) { setKey(q.window == host ? hostKeys : gameKeys, static_cast<unsigned>(q.key), down); }
    if (q.window != host) { continue; }
    // Observe genuine per-queue modifier state; PostMessage never writes it.
    if (down && q.key == VK_TAB) {
      if (hostKeys[VK_SHIFT]) { ++steamChords; } else { ++hostTabWithoutShift; }
    }
    MSG msg {}; msg.hwnd = host; msg.message = q.message; msg.wParam = q.key;
    if (msg.wParam == VK_LSHIFT || msg.wParam == VK_RSHIFT) { msg.wParam = VK_SHIFT; }
    msg.lParam = q.bits; msg.time = ticks;
    runtime->_input->consume(msg);
  }
  pending.clear();
}
static void event(Presenter &presenter, UINT key, bool up, bool wrapper) {
  KBDLLHOOKSTRUCT data {}; data.vkCode = key; data.scanCode = key == VK_TAB ? 15 : key == VK_HOME ? 71 : 42;
  data.flags = up ? LLKHF_UP : 0;
  const WPARAM message = up ? WM_KEYUP : WM_KEYDOWN;
  const LRESULT blocked = wrapper
    ? handle_windows_hook<WH_KEYBOARD_LL, reshade::input::is_blocking_any_keyboard_input>(HC_ACTION, message, reinterpret_cast<LPARAM>(&data))
    : bridgeHook(HC_ACTION, message, reinterpret_cast<LPARAM>(&data));
  // Windows updates asynchronous state only after the low-level hook returns.
  // Posted copies do not produce physical input or alter thread queue key state.
  if (!blocked) {
    setKey(asyncKeys, key, !up);
    pending.push_back({currentFocus, static_cast<UINT>(message), key, 0, true});
    if (currentFocus == host) { ++nativeHost; } else { ++nativeGame; }
  }
  drain(presenter);
}
static void frame(Presenter &presenter) { runtime->frame(); drain(presenter); }
static void setup(Presenter &presenter, Runtime &rt, bool steamEnabled) {
  runtime = &rt; currentFocus = game; currentForeground = game; blockKeyboard = false;
  asyncKeys.fill(false); hostKeys.fill(false); gameKeys.fill(false); pending.clear();
  bridgeCalls = nativeHost = nativeGame = steamChords = hostTabWithoutShift = 0;
  presenter.m_parent = game; presenter.m_window.store(host); presenter.m_threadId.store(ownerThread);
  presenter.m_input = true; presenter.m_steamInput = steamEnabled; presenter.m_steamNativeInput = steamEnabled;
  presenter.m_imeNativeWarmup = false; presenter.m_reshade.store(reinterpret_cast<HMODULE>(3));
  Presenter::s_active.store(&presenter);
  s_windows_hooks.clear();
  s_windows.clear(); s_windows[host] = rt.ownedInput;
  s_windows_hooks[static_cast<UINT64>(WH_KEYBOARD_LL) << 32] = {reinterpret_cast<HHOOK>(4), bridgeHook};
}
int main(int argc, char **argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  try {
    check(mode == "home-direct" || mode == "home-wrapper" || mode == "steam-direct" || mode == "steam-wrapper" || mode == "home-wrapper-no-steam"
      || mode == "home-wrapper-active-widget" || mode == "home-wrapper-ignore-shortcuts" || mode == "home-wrapper-repeat" || mode == "fallback-home", "unknown case");
    Presenter presenter; Runtime rt; setup(presenter, rt, mode != "home-wrapper-no-steam");
    if (mode == "fallback-home") {
      // Exercise the actual capture()/setCapture() bodies without a ReShade addon.
      // A ReShade-only focus rule must not disable the legacy fallback hotkey.
      presenter.m_reshade.store(nullptr); presenter.m_fallback = true;
      event(presenter, VK_HOME, false, false);
      check(presenter.m_capture.load(), "fallback first Home failed to open capture");
      event(presenter, VK_HOME, false, false);
      check(presenter.m_capture.load(), "fallback held Home toggled capture twice");
      event(presenter, VK_HOME, true, false);
      check(!presenter.m_keyDown, "fallback Home key-up failed to clear repeat guard");
      event(presenter, VK_HOME, false, false);
      check(!presenter.m_capture.load(), "fallback second Home failed to close capture");
      event(presenter, VK_HOME, true, false);
      check(!presenter.m_keyDown, "fallback close left the repeat guard stuck");
      event(presenter, VK_HOME, false, false);
      check(presenter.m_capture.load(), "fallback third Home failed to reopen capture");
      check(currentFocus == game && nativeHost == 0, "fallback stole native focus without ReShade");
      check(rt.transitions == 0, "fallback unexpectedly toggled a ReShade runtime");
      Presenter::s_active.store(nullptr);
      std::printf("RESHade_KEYBOARD_PASS case=%s bits=%u assertions=%u\n", mode.c_str(), unsigned(sizeof(void*)*8), assertions); return 0;
    }
    const bool wrapper = mode.find("wrapper") != std::string::npos;
    event(presenter, VK_HOME, false, wrapper); frame(presenter);
    check(rt._show_overlay && presenter.m_capture.load(), "first Home did not open ReShade");
    if (mode == "home-wrapper-repeat") {
      event(presenter, VK_HOME, false, wrapper); frame(presenter);
      check(rt._show_overlay && rt.transitions == 1, "held Home toggled the menu repeatedly");
    }
    event(presenter, VK_HOME, true, wrapper); frame(presenter);
    if (mode.find("home-") == 0) {
      if (mode == "home-wrapper-active-widget" || mode == "home-wrapper-ignore-shortcuts") {
        rt.context.ActiveId = mode == "home-wrapper-active-widget" ? 17 : 0;
        rt._ignore_shortcuts = mode == "home-wrapper-ignore-shortcuts";
        event(presenter, VK_HOME, false, wrapper); frame(presenter);
        check(rt._show_overlay && rt.transitions == 1, "Bridge bypassed ReShade's active-widget or shortcut guard");
        event(presenter, VK_HOME, true, wrapper); frame(presenter);
        rt.context.ActiveId = 0; rt._ignore_shortcuts = false;
      }
      const unsigned before = bridgeCalls;
      event(presenter, VK_HOME, false, wrapper); frame(presenter);
      std::printf("TRACE close bridge_calls=%u native_host=%u native_game=%u focus=%s menu=%d capture=%d\n", bridgeCalls-before, nativeHost, nativeGame, currentFocus == host ? "host" : "game", rt._show_overlay, presenter.m_capture.load());
      check(!rt._show_overlay && !presenter.m_capture.load(), "second Home failed to close ReShade");
      event(presenter, VK_HOME, true, wrapper); frame(presenter);
      event(presenter, VK_HOME, false, wrapper); frame(presenter);
      check(rt._show_overlay && rt.transitions == 3, "third Home failed to reopen exactly once");
    } else {
      event(presenter, VK_LSHIFT, false, wrapper);
      event(presenter, VK_TAB, false, wrapper);
      std::printf("TRACE chord async_shift=%d host_queue_shift=%d native_host=%u native_game=%u observed=%u missing_modifier=%u\n", asyncKeys[VK_SHIFT], hostKeys[VK_SHIFT], nativeHost, nativeGame, steamChords, hostTabWithoutShift);
      check(steamChords == 1 && hostTabWithoutShift == 0, "Shift+Tab lacks native Host queue modifier state");
      event(presenter, VK_TAB, true, wrapper); event(presenter, VK_LSHIFT, true, wrapper);
      check(!hostKeys[VK_SHIFT] && !hostKeys[VK_TAB], "native chord release leaves a stuck key");
    }
    Presenter::s_active.store(nullptr);
    std::printf("RESHade_KEYBOARD_PASS case=%s bits=%u assertions=%u\n", mode.c_str(), unsigned(sizeof(void*)*8), assertions); return 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "RESHade_KEYBOARD_FAIL case=%s reason=%s\n", mode.c_str(), e.what()); return 1;
  }
}
