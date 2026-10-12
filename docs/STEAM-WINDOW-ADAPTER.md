# Steam renderer window adapter (experimental)

Status: **AWAITING GAME VALIDATION**. This is a narrow compatibility candidate,
not a claim that Steam Overlay input works. It adds no IPC, renderer-thread,
keyboard-state, focus, capture or ReShade behavior.

## Hardware evidence and scope

Matched diagnostic build 15dd529becdee47f captured the known Steam input selector
at return RVAs 0xA6AD4/0xA6AE2. With Presenter disabled, 30 sampled selections
returned NULL active then the game's foreground window (Client PID25208 versus
Host PID22080). With Presenter enabled in the subsequent session, the Host child
0x440A08 belonged to Host PID23664/thread8508 and was used for backend focus and
presentation. Nevertheless all six sampled selector pairs on thread10664 still
returned NULL active then game HWND0x1715C0 (Client PID23640). The two additional
foreground checks also saw other-process windows. The selector's verified
same-process predicate rejects these candidates. This supports the selected
compatibility boundary, not a unique cause for all Steam input failure.

Both sessions had attached mask7 and no dropped observations. Their after-attach
hook-install counters were zero; pre-existing or dynamic hooks remain outside
that conclusion. The latter session ended cleanly after about20 seconds. The
user has not independently reported all shortcut/mouse actions for that session.
Raw supplied logs are not redistributed.

## Behavior

`server.steamWindowAdapter=False` by default, independent of diagnostics.
When enabled, the existing named-IAT wrappers in the already loaded matching
GameOverlayRenderer[64].dll apply this policy:

- GetForegroundWindow: substitute the original returned game window with the
  existing Host Presenter child only when the returned foreground window belongs
  to the configured game process/tree and the child currently belongs to Host
  and is a WS_CHILD descendant of that game window.
- GetActiveWindow: preserve NULL and authentic Host/other-window results. A
  game-owned active window can be substituted under the same checks, with an
  additional foreground snapshot from Host USER32 confirming the game tree.
- Missing, destroyed, unrelated or wrong-process Presenter/game windows fail
  closed to the original result. Other applications' foreground windows are
  unchanged. Validate ownership/hierarchy for each eligible query.
- All renderer named imports of these APIs use this policy, including the
  observed selector and two other foreground checks. No private RVA patching.
  Dynamic GetProcAddress/unnamed/delay imports remain outside coverage.
- SetWindowsHookExW is only observed, never redirected. No PID query is altered;
  the returned child genuinely belongs to Host, with its existing GUI thread.
- Backend calls, Host/game imports outside that renderer and API global entry
  points are unaffected. The original query target is called once, retaining
  prior interceptors. LastError is preserved, including across eligibility
  checks and diagnostic logging.

The renderer module reference and saved targets remain alive until Host exit;
cleanup first disables adaptation then conditionally restores our IAT slots.
As in the diagnostic build, enabling it prevents renderer unload until Host exit.
No new worker thread, global hook, private Steam API, synthetic key or focus
operation is introduced. Diagnostic logging remains bounded and optional.

## Install and controlled comparison

Exit game and Hosts, back up the previous matched binaries, and replace all
three bridge binaries from the new ZIP. Preserve the backend DLL and actual
bin/.l4d2bridge/bridge.conf. Merge config/STEAM-WINDOW-ADAPTER.conf without duplicate
keys; it retains all settings of the latest Presenter-enabled C8 control. Keep
C8 and unrelated memory/resource settings. The only new behavioral flag is:

```ini
server.steamWindowAdapter = True
server.steamWindowDiagnostics = True
```

On this same new binary, compare adapter False then True, restarting both game
and Host between runs. Confirm matching build identities and mode=window-adapter
in the True run. Initially keep diagnostics enabled and do not interpret FPS.

Test Shift+Tab opening before using the game entry, opening through that entry,
mouse interaction, closing with Shift+Tab, focus restoration to game, Alt+Tab and
return, Reset/resizing, and repeated map transitions. Supply paired logs, actual
config and which actions succeeded. Do not change other input flags in this
comparison; remove failed experiments individually only after compatibility
is established.

Expected evidence: selector foreground query result=mapped, real_pid=Client,
owner_pid=Host, same_process=1, followed by authentic hook-install records on the
Presenter thread. A successful hook installation alone is not proof of working
UI. If binding succeeds but input remains broken, inspect hook callbacks and
Steam's next input transport stage. Background results must remain unchanged.

Rollback restores all three previous matched binaries and disables the new flag.
Normal shipped defaults remain adapter False, diagnostics False, C1. No main,
VERSION, tags, releases or earlier ZIPs are changed by this candidate.

## Native verification

Production-header tests retain the diagnostic real USER32 and prior-IAT-chain
checks on x86/x64. The adapter suite creates a genuine foreign-process game root
using an opposite-architecture helper and a real Host-owned child. Controlled
previous-IAT query results exercise actual renderer wrappers without depending
on CI foreground privileges. They test disabled/enabled return, original call
count, LastError, mapped logging, NULL/background/other-window policy, Host-owned
results, wrong parent/owner, missing/destroyed Presenter and cleanup restoration.
These controlled inputs are not evidence of actual Steam or gameplay behavior.
All existing IPC/native regressions must also pass before a ZIP is offered.
