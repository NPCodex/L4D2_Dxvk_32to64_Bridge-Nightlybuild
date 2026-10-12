# Steam renderer window adapter (experimental)

Status: **INPUT VALIDATED IN REPORTED SESSION** for Shift+Tab, mouse and Join Server UI on 294eafb0349ce3aa. View Workshop web opening remains under investigation; wider map/Reset/performance and reduced-flag validation are pending. The build was originally delivered as a compatibility candidate; historical pending-validation statements below describe that delivery. It adds no IPC, renderer-thread,
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

## Verified candidate delivery

- Source: `e987a8c1832c3328e0326a99e901f4c2041d4a51`.
- Build: `ldb-ipc-c32-dev+294eafb0349ce3aa`; all three bridge binaries rebuilt and matched.
- [ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/196399e039ac139514db50a28d403be9df58b940/l4d2-bridge-steam-window-adapter-dev-294eafb0349ce3aa-r38198547689.zip), 1,386,399 bytes.
- SHA-256: `aed19c7249fda6ca9df19747ded086ddf622be81622f94d57c98194d4ba49ffa`.
- [Windows run](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38198547689):
  252 cross-process IPC pairs, 30 fail-closed cases, both policy fixtures, ten
  native suites and 39 Python contracts passed. New adapter checks passed
  x86 Host/x64 game helper and x64 Host/x86 game helper; diagnostic real USER32
  and prior-IAT-chain modes also passed both architectures.
- Independently inspected Release receipts, x86 Client/x86+x64 Hosts, shared
  identity, binary hashes, Host adapter code, x86 Host LAA, disabled defaults
  and C1/C2/C4/C8/C16/C32 configuration. Public ZIP SHA-256 verified after upload.
- [Native evidence](steam-input-evidence/38198547689-native-pass.log),
  [IPC results](steam-input-evidence/38198547689-ipc-raw.json),
  [fault results](steam-input-evidence/38198547689-ipc-faults.json),
  [inspection](steam-input-evidence/38198547689-artifact-inspection.json),
  [upload](steam-input-evidence/window-adapter-upload-receipt.json).

No actual Steam/game acceptance yet; no input/FPS improvement is claimed. main
and tested IPC development remain unchanged; candidate stays unmerged.

## Verified installer for repeated old-build logs

Two later paired uploads still report 15dd529becdee47f and mode=observe-only,
even though the config includes steamWindowAdapter=True. They cannot evaluate
the adapter. No attribution to a specific download/copy mistake is established.
The immutable public ZIP was verified again by a Windows installation fixture.

[Download installer](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/a0423edfa0bac8d6d807a6bb531c55599715026a/scripts/install_steam_window_adapter.ps1).
It defaults to the game directory shown in the supplied logs, refuses running
game/Hosts, downloads the exact immutable ZIP, verifies ZIP and all three binary
hashes/build IDs before writes, backs up all existing target bridge binaries
before replacement, and verifies installed files. Config/backend are untouched.
It does not alter script execution policy itself or kill any processes. Temp
files and backups are retained. If a copy fails, restore the printed backup.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\install_steam_window_adapter.ps1
# Read-only installed identities if troubleshooting:
powershell -NoProfile -ExecutionPolicy Bypass -File .\install_steam_window_adapter.ps1 -CheckOnly
```

A different game path can be specified with -GameRoot. Confirm new startup
logs show 294eafb0349ce3aa and mode=window-adapter before evaluating input.
[Windows installer verification](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38199532076)
passed actual immutable download/install, all three file hashes and embedded
identities, existing file backups and unchanged config/backend in a disposable
fixture. It did not start any Bridge/Steam/game binary. The bridge ZIP and
compiled source remain unchanged. Real-game installation/input still needs
user confirmation.
