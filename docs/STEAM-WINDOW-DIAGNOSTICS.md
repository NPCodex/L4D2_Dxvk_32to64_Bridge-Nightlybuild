# Steam window selection diagnostic (experimental)

This build observes Steam's own window queries and hook-install requests. It
**does not repair Steam input** and does not replace any HWND, PID, focus,
capture, keyboard state, API argument or return value. ReShade is outside this
experiment. Ordinary `config/bridge.conf` leaves it disabled.

## Install and run

1. Exit game and both Hosts. Back up the current matched bridge binaries.
2. Replace all three bridge binaries from this ZIP, keeping your DXVK backend
   and actual `bin/.l4d2bridge/bridge.conf`.
3. Merge `config/STEAM-WINDOW-DIAGNOSTICS.conf` into that actual config; avoid
   duplicate keys. Keep your currently tested C8 and other unrelated settings.
   The snippet preserves all three switches of the ineffective combined run.
4. Start from Steam, try Shift+Tab while the game is foreground, then open the
   overlay by the usual game entry and try mouse/Shift+Tab again. Exit normally.
5. Supply `bridge64.log`, `bridge32.log` and the actual config. Do not use this
   diagnostic run for an FPS comparison; diagnostic queries/logging add cost.

Only `server.steamWindowDiagnostics=True` is newly required. Existing input
options are kept for comparability, not treated as successful repairs.
Restart is required to switch the diagnostic off. Rollback restores **all three**
previous binaries and removes/disables the new option.

## Evidence to look for

`L4D2_STEAM_WINDOW` lines in the Host log:

- `event=waiting`: renderer not yet loaded; existing Present checks retry at most
  once per second, without a new polling thread.
- `event=module` / `event=image`: actual module path, base, Host PID, architecture,
  PE timestamp and image size. These identifiers are not a cryptographic hash.
- `event=import`: attached named import, its actual slot RVA and previous target.
  `event=attached mask=7` means all three imports were observed; bits 1/2/4 mean
  GetActiveWindow / GetForegroundWindow / SetWindowsHookExW. Partial/zero coverage
  is reported explicitly; it cannot establish absence of Steam activity.
- `event=window-query`: authentic API result, immediate caller **return-site** RVA,
  calling thread, HWND owner PID/thread, Host PID, game/presenter handles and
  owners/threads, `same_process`, last error, tick, `result=unchanged`.
- `event=hook-install`: authentic installation result, type, requested thread,
  hook module/procedure, actual caller/thread and last error. A NULL result is
  a failure. LastError after successful calls may be stale; do not infer failure
  from it. `target_thread=0` has the API's ordinary global-hook meaning.
- `event=summary`: total intercepted API calls and dropped observations.

For the user's confirmed renderer sample SHA-256
`59057f983ae6f02b7eb2495bf65e0430129dd81dff8d9628c764bd1a07e23fee`,
window-selector return sites are **0xA6AD4** (GetActiveWindow) and **0xA6AE2**
(GetForegroundWindow fallback). Foreground checks also use **0xA5132 / 0xA5F64**.
At the selector, a non-NULL game-owned candidate with `same_process=0` supports
that path's same-process rejection. A NULL active result followed by a game-owned
foreground result supports the fallback rejection. No observation of these sites
means the suspected path remains unconfirmed, not that it succeeded. See
[static analysis](STEAM-RENDERER-WINDOW-GATE.md) for the exact branch and limits.

This diagnostic does not intercept Steam's PID comparison itself, trace every
hook callback, or prove all reasons for rejection. It collects the real selector
inputs and authentic hook installations. It must not be labelled an input fix.

## Implementation and cost

Only the already loaded architecture-matching GameOverlayRenderer[64].dll's
three **named IAT slots** are wrapped. PE import names/RVAs are checked; no hardcoded
sample addresses, exported private ABI calls, on-disk DLL edits, or code patches.
Steam/USER32 global entrypoints and other modules' imports are untouched. Each
saved original target is called exactly once, including earlier interceptors.
Observation restores LastError and preserves return/arguments; logger exceptions
are contained. A process-lifetime module reference protects saved originals and
in-flight wrappers during cleanup. This deliberately prevents renderer unload
while enabled and is confined to this temporary diagnostic.

State is bounded to 128 caller/thread/API sites, 4096 emitted observation lines,
non-blocking trace locking and once-per-second unchanged window samples (handle
changes are immediate). Therefore very short transitions at a saturated/busy
site can be missed. Counts remain available. Hook installations share the line
limit. There are no key/text/cursor payload logs. No hook is installed while the
option is off. Dynamic GetProcAddress/delay-loaded/unnamed imports are outside
coverage. Cleanup restores a slot only if it still contains our wrapper, keeping
subsequent third-party changes intact. Original targets/state remain valid until
Host process exit.

Windows tests compile genuine x86/x64 fixtures and the production diagnostic
header. They check authentic local Windows hook success/failure, prior-IAT-chain
arguments/return/LastError, once-only original calls, caller/thread attribution,
throttling, disabled/missing-module startup, repeated poll, restoration, preserved
third-party replacement and isolation of unrelated Host imports. The uploaded
proprietary Steam DLL is neither executed by these tests nor redistributed.

Status: **BLOCKED — awaiting actual Steam window-selection evidence**. Native
fixtures can validate observation semantics, not actual gameplay or Steam input.
