# Add-ons Workshop guard and request diagnostic

Status: **PASSED — USER-REPORTED GAME VALIDATION** for the reported Steam
issues on build `ldb-ipc-c32-dev+83ec0fca6f5fb2f1`, branch
`codex/steam-web-request-diagnostics`. The user reported “测试通关。” after testing
the scoped Workshop availability fix and explicitly requested marking it passed
on2026-10-12. Accepted reported scenarios: Shift+Tab opening/closing, Overlay
mouse interaction, Join Server UI and the Add-ons View Workshop entry.

This records user acceptance, not new instrumented evidence: no post-fix logs or
performance data were supplied with that confirmation. Coverage remains the
reported x64 Host setup with required opt-in settings; other Steam versions,
x86 getter-based proxy support and ReShade coexistence are not newly validated.
Earlier diagnostic deliveries and their pending statuses below are historical.
See [acceptance record](steam-input-evidence/steam-hardware-acceptance.json).

## Exact caller evidence

User's x86 GameUI sample SHA256
321ec9b5865582330af61e81eb347da31cc213cc6c95b80d6d98170b8a5fbb40
routes VisitWorkshop at RVA0x20227 to helper0x199A0, and BrowseWorkshop at0x201F1
to0x19A80. Both check the existing Steam context/user/Utils/Friends pointers,
call Utils vtable offset0x44, test AL and immediately return if false. The
supplied Steam API IsOverlayEnabled export at0x5660 confirms that method's
identity. Availability guard return sites:0x199EA/0x19ACA. A duplicate browse
helper has return site0x6EA9A.

Only after the guard and a nonzero universe result does GameUI construct its
steamcommunity.com sharedfiles/filedetails or workshop/browse URL, then call
Friends017 offset0x78 with URL and mode0. This is a direct C++ call, not the API
flat web export. These paths and factories explicitly use SteamFriends017 and
SteamUtils010. The existing context pointer is at RVA0x1FA0F0; its Friends/Utils
members used by these instructions are at+8/+12. The complete uploaded files
were inspected statically and never executed, modified or redistributed.

The static guard does not prove its actual failing predicate at runtime. Other
pointer/universe/request-routing checks may matter. No predicate is forced true
by this observer.

## Implementation and coverage

Default `client.steamWebDiagnostics=False`. The observer runs from the existing
Client Present boundary, retries context readiness once per second and creates
no thread, Steam context or additional SDK invocation. It opens only the already
loaded GameUI file for SHA256 identification via system BCrypt (not uploaded DLL
execution). Unknown files are explicitly refused. Before reading the existing
context it checks the loaded caller's relevant invariant instruction bytes.
It reads existing interface objects using ReadProcessMemory and observes only
their evidenced methods (Utils slot17 bool thiscall, Friends slot30 void thiscall
with URL+int32 mode). Original targets must be executable and vtables loader
image-backed; code modules are pinned to protect saved targets. The already
loaded menu reference also remains alive until Client exit. No SDK exports,
private Steam code, on-disk game DLL or context fields are changed.

The two shared vtable entries are conditionally wrapped. Each original is called
exactly once with the original instance and borrowed URL/mode. Bool/void behavior
and LastError are preserved, including when logging throws. SDK methods on this
same table remain observed for the lifetime of this temporary diagnostic. Only
the three known GameUI guards emit availability detail, avoiding unrelated
per-frame availability log floods. Web requests include actual caller metadata,
mode and bounded URL category; arbitrary URL contents/query tokens are not
printed. A normal method return is not evidence of navigation success.

Logging is capped at512 attempts. Summary counters include calls not individually
logged. Partial attachment, file/layout refusal, initial pointer prerequisites,
other interface objects/tables, subsequent Steam reinitialization or a modified
menu path are coverage limitations: zero calls is not proof of zero clicks or
zero requests. Observer enables no input, web forwarding or browser fallback.
Cleanup restores a slot only if our wrapper is still there; saved state/code
remain valid for late/in-flight callbacks. Restart is required to switch modes.

## Test procedure

Exit game and Hosts, back up current matched bridge, replace all three binaries
from the ZIP and verify the new matching build identity. Preserve current C8,
working input settings, actual bin/.l4d2bridge/bridge.conf and backend. Add only:

```ini
client.steamWebDiagnostics = True
```

Keep server.steamWindowAdapter=True and current window/input diagnostics for
correlation. Do not change the successful input flags in this comparison.

1. Keep game foreground and reach Add-ons menu. Wait a few seconds for attachment.
2. Click the selected add-on's View/Visit Workshop once; note its approximate time.
3. If available, click Browse Workshop separately. Note which action opens UI.
4. Verify Shift+Tab/browser still work, close normally and supply paired logs plus
   action results. Do not use this diagnostic run to judge performance.

Look in Client bridge32.log for L4D2_STEAM_WEB:

- verified-menu and attached utils010=1 friends017=1: supported observer present.
- IsOverlayEnabled at guard return sites with overlay_enabled=0: that observed
  guard returns false, before the normal web call. No forced true fallback.
- overlay_enabled=1 but no web request: inspect other prerequisites/coverage;
  don't infer routing failure solely from no observed call.
- ActivateGameOverlayToWebPage with mode/url_class and web-return: request reached
  and returned from that real interface; correlate activity/navigation next.
- unsupported/waiting/partial attachment: report the gap, not a rejected request.

Native tests use authored objects, real x86 thiscall/fastcall bridging, disabled
mode, false guard with no web invocation, true guard, exact borrowed arguments,
URL categories without query logging, LastError, thrown logger, SHA256 match/
mismatch and conditional restoration after a third-party replacement. No actual
Steam/game DLL is executed in CI. Retained IPC/input regressions must pass.

Rollback restores all three prior 294eafb0349ce3aa binaries and removes/disables
client.steamWebDiagnostics. Current successful input configuration is preserved.
main, formal VERSION/tags/releases and previous ZIPs remain unchanged.

## Verified experimental delivery

Build `ldb-ipc-c32-dev+06abe08be638118c`, source14bf94b43887c287e1d336526fecb655fa2d81cc.
[ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/b148ca9f23f7f37be0f4de7bc9f150abd0220343/l4d2-bridge-steam-workshop-diagnostics-dev-06abe08be638118c-r38202917325.zip) (1,397,408 bytes).
SHA256 `2e400df86133c068f08d9a416e97542fd85e7d745d54d41f9b03159d3313cf3a`. Public download was independently verified after upload.

[Windows run](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38202917325)
passed the new x86 web observer ABI/hash/guard tests, all eleven native suites,
39 Python contracts, 252 IPC process pairs, 30 fail-closed cases and both batching
policy tests. Release receipts, Client x86 and both Host architectures, shared
embedded identity, Host LAA, binary hashes, Client web observer code, retained
Host adapter and disabled defaults were independently inspected.

[Native evidence](steam-input-evidence/38202917325-native-pass.log),
[IPC data](steam-input-evidence/38202917325-ipc-raw.json),
[fault data](steam-input-evidence/38202917325-ipc-faults.json),
[artifact inspection](steam-input-evidence/38202917325-artifact-inspection.json),
[upload receipt](steam-input-evidence/workshop-diagnostics-upload-receipt.json).

At that delivery there was no runtime Workshop guard/request observation. The uploaded Steam/GameUI
files are not executed or included. Current input fix is retained; no new
Workshop compatibility behavior is claimed.

## Verified installation helper

[Installer](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/096367f8436f8e37bd2488354caa569935a5e0e1/scripts/install_steam_workshop_diagnostics.ps1)
downloads exactly this immutable ZIP, checks ZIP and all three binary hashes/IDs,
backs up originals before replacement and leaves actual config/backend alone.
Close game and Hosts first. Use powershell -NoProfile -ExecutionPolicy Bypass
-File .\install_steam_workshop_diagnostics.ps1, then add
client.steamWebDiagnostics=True to the retained actual config. -CheckOnly prints
installed identities/hashes without writes. -GameRoot selects another library.

[Windows installer run](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38203507361)
passed immutable download, actual copy/verification of all three new files,
backup and config/backend preservation in a disposable fixture. No supplied
Steam/GameUI DLL or bridge binary was executed in this installer test.

## Hardware guard confirmed; Host getter evidence next

The subsequent matched 06abe08be638118c session attaches both interface observers.
All seven recorded VisitWorkshop guard calls at GameUI return0x199EA return
False. Summary web_calls=0; three actual Overlay on/off pairs and six successful
Host input-hook installs show the working input path persists. This confirms the
observed button path is stopped before web activation. It does not establish how
Steam classifies availability in the separate Host renderer.

The next observation adds Host sampling using the same client.steamWebDiagnostics
flag. In the supplied SHA59057f... renderer, the exported IsOverlayEnabled at
RVA0xB08A0 consists precisely of MOVZX EAX, byte[RIP+disp32], RET and reads
RVA0x17D2C2. After whole-file SHA and exact loaded instruction validation, the
Host diagnostic reproduces this one-byte read with ReadProcessMemory. It NEVER
invokes this undocumented export, creates an SDK context, modifies state or
changes the menu result. It samples at most once per second and logs the first
raw value and changes. Unknown files, getter layouts or targets outside the
module are explicitly rejected. A raw getter value is not a GPU fence, an
activation result, or proof of browser navigation.

Look for L4D2_STEAM_WEB_HOST event=verified and event=availability with
renderer_getter_value. Compare it with the Client's actual menu guard calls:
Host value1 versus Client false supports testing a narrowly scoped availability
proxy next; Host false means a blanket True replacement lacks that evidence.
The supplied renderer is x64 only; x86 Host reports this observer unsupported
and otherwise retains existing behavior. No x86 renderer offsets are guessed.

Native x86/x64 tests verify actual leaf decoding, ASLR changes, signed RIP
displacement, bounds, unknown-layout refusal and disabled behavior. Uploaded
proprietary DLLs are still never executed in CI or redistributed. Existing
web/guard/LastError/hash tests are retained after sharing the hash helper.

Use this next matched ZIP with the SAME working configuration/diagnostic flag,
click View Workshop, then open/close Shift+Tab and click View Workshop again.
Supply paired logs. The diagnostic does not fix the confirmed false guard.

## Verified paired Host/Client diagnostic delivery

- Build `ldb-ipc-c32-dev+4833cce93dfc4dc0`, compiled source03164796c1d9e9d235b61319522f35991340bec1.
- [New matched ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/cb92977856f73c5ca6534bf206ebfa36eff31460/l4d2-bridge-steam-workshop-host-dev-4833cce93dfc4dc0-r38205119024.zip), 1,402,044 bytes.
- SHA256 `63c27c56287c2cf603b1b69a0285b17a1114e16e607853238f501498aefe5093`; public download independently verified.
- [Native Windows run](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38205119024)
  passed twelve regression suites including x86/x64 getter decoding and retained
  actual web/guard ABI tests, 39 Python contracts, 252 IPC pairs, 30 faults and
  both batching policies. All three Release files and matched embedded IDs,
  hashes/architectures, retained adapter, new x64 Host observer and x86 Host LAA
  were independently checked. The first candidate run38204701237 stopped on an
  x86 constant-expression warning under /WX before packaging; explicit platform
  isolation fixed it and no binary from that failed run was published.
- [Native evidence](steam-input-evidence/38205119024-native-pass.log),
  [IPC data](steam-input-evidence/38205119024-ipc-raw.json),
  [fault data](steam-input-evidence/38205119024-ipc-faults.json),
  [inspection](steam-input-evidence/38205119024-artifact-inspection.json),
  [upload](steam-input-evidence/workshop-host-upload-receipt.json).

Replace all three files and verify4833cce93dfc4dc0. Keep current C8, input flags
and client.steamWebDiagnostics=True; no new setting is needed. This version still
preserves the false menu check. It supplies the missing Host getter measurement
for the next compatibility decision, not a Workshop fix. The earlier installer
targets the earlier06abe08be638118c package and must not be used to install this
new paired-observer package. main, tested IPC branch, VERSION/tags/releases and
all previous ZIPs remain unchanged.

## Scoped availability compatibility experiment

The matched4833cce93dfc4dc0 hardware session verifies the actual Host getter;
it changes0 to1 at22:45:50.846 with no later recorded change. Ten Client
VisitWorkshop calls at22:52:18.819 through22:52:20.711 all return0; summary
web_calls=0. Both processes shut down normally. This supports a menu availability
proxy experiment; it does not establish web request routing or navigation.

New option `client.steamWorkshopAvailabilityProxy=False` remains off by default.
Enable True in the same actual shared bridge.conf on the matched experimental
pair, keeping `client.steamWebDiagnostics=True` and the validated C8/input config.
Host creates a separate16-byte shared control mapping identified by the existing
session GUID only after the matching Client/Host build check. Client opens it only
after the successful handshake, validates protocol version and actual launched
Host PID, and duplicates its process handle. No render command protocol, command
ordering, rendering queue or resource lifetime is changed. Mapping view reservation
is at most one64KiB allocation granularity plus normal page backing; it exists only
when this experiment is enabled. The experiment adds no threads or Steam contexts.

Host's already verified observer publishes every sample, including unchanged
values, using one aligned InterlockedExchange64: timestamp and unknown/false/true
state form one atomic snapshot. The Client reads it atomically only when an
observed Workshop guard's original result is False. A zero-time process-handle
check must confirm the actual launched Host is alive. A sample is accepted only
if known and exactly1, not future-dated, and no more than3000ms old. Unknown
renderer/layout, unreadable sample, unsupported x86 renderer, missing control
mapping, wrong version/PID, exited Host and stale samples all preserve the
original SDK result. The x86 Host and rest of the bridge remain supported; only
this getter-based compatibility experiment requires the validated x64 sample.

GameUI full file hash and loaded instruction checks remain mandatory. Only the
three exact evidenced menu guard return addresses0x199EA/0x19ACA/0x6EA9A can
receive a True result based on fresh Host availability. Other callers on the
same shared SDK vtable retain the original result, and the original SDK method
is called exactly once. Original LastError is preserved. Observe logs retain
original result; L4D2_STEAM_WORKSHOP event=availability separately records original,
returned and host_ready. True original results are never suppressed. The existing
Friends017 web method still receives the original borrowed URL/mode exactly once.
There is no injected URL, Host web forwarding or keyboard emulation.

The channel endpoints and pinned code/targets are deliberately process-lifetime
objects so late/in-flight callbacks cannot touch unmapped status memory. OS
process exit closes handles/views; no stale mapping is reused across session GUIDs.
Client conditional hook restoration is retained. A stalled host can cause a
conservative refusal during slow loading, not permission to use stale state.
The freshness bound is a correctness choice, not a claimed optimal performance
parameter. Sampling occurs at existing Present boundaries and may pause when
there are no Presents. This experiment requires independent gameplay validation.

Native tests compile both actual x86/x64 layouts and exercise all four process
architecture pairs, initial unknown, false, stale/future, tick rollover, missing
channel, exclusive writer, version/PID refusal and Host death. The retained x86
ABI test authors a tiny executable caller at the evidenced relative call site,
checks scoped False-to-True and unrelated callers, disabled/unknown/stale refusal,
exactly one original call and normal web execution, borrowed arguments and
LastError under throwing logging. These are authored fixtures; supplied Steam
and GameUI DLLs are never executed or redistributed in tests.

For the next real run: replace all three matched bridge files, enable the new
option plus diagnostics, keep current C8/input/backend, click View Workshop once,
then Browse Workshop if available. Check Shift+Tab/mouse/browser/Join Server,
exit normally and send both logs plus visible results. Expected new evidence:
client-channel ready1, Host getter1, original0 returned1 host_ready1, then an
ActivateGameOverlayToWebPage request. A returned web call alone is not proof of
navigation. If it does not open, investigate routing after observing this call;
do not assume Host forwarding is required in advance.

Rollback all three binaries to4833cce93dfc4dc0 or disable the new option and
restart. Do not use either older installer for the new build. main, production
VERSION/tags/releases and prior packages are preserved.

## Verified scoped compatibility delivery

- Build `ldb-ipc-c32-dev+83ec0fca6f5fb2f1`, compiled source
  `e3d6d9110e05ab97dd57466a64d664dfa777705c`.
- [Matched ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/50e3b8bd2b0e09204a92ef250e592473c024464f/l4d2-bridge-steam-workshop-proxy-dev-83ec0fca6f5fb2f1-r38211096419.zip), 1,407,086 bytes.
- SHA256 `ee43849a978b2022675e73b25b0a0580742a823921ae62fc907d69ce78cbb1b7`; remote download independently checked after append-only upload.
- [Windows run](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38211096419)
  passed all13 native suites, including the actual x86 caller ABI/proxy fixture
  and all four x86/x64 process combinations, plus39 Python checks,252 IPC pairs
  and30 faults. Actual /O2 NDEBUG Release receipts, all three matching binaries,
  architecture/Host LAA, retained input adapter, proxy code, disabled default and
  enabled opt-in snippet were independently inspected.
- [Native evidence](steam-input-evidence/38211096419-native-pass.log),
  [IPC data](steam-input-evidence/38211096419-ipc-raw.json),
  [fault data](steam-input-evidence/38211096419-ipc-faults.json),
  [inspection](steam-input-evidence/38211096419-artifact-inspection.json),
  [upload](steam-input-evidence/workshop-proxy-upload-receipt.json).

The earlier candidate38210786826 passed the real x86 proxy ABI and both native
channel compilations, but the test harness stopped on PowerShell's reserved
$Host variable before process-pair execution and packaging. Task-specific
variable names fixed it; no binaries from that failed run were published.
The earlier preliminary candidate38210690775 was cancelled after source review
corrected the local child process handle binding; it produced no published ZIP.

Installation: exit game and Hosts; replace all three binaries from this ZIP and
verify83ec0fca6f5fb2f1 in both logs. Keep current C8 and working input/backend.
Set client.steamWorkshopAvailabilityProxy=True and client.steamWebDiagnostics=True
in the actual bin/.l4d2bridge/bridge.conf. Do not overwrite that actual config
with the packaged generic defaults, and do not use either older installer.
This validated getter experiment currently targets the verified x64 Host.
Click View Workshop and Browse Workshop, then verify Shift+Tab/mouse/Join Server;
supply paired logs and visible results. Keep the option disabled for rollback
or restore all three4833cce93dfc4dc0 files. Production main, tested IPC branch,
VERSION/tags/releases remain unchanged. At delivery this was awaiting game
validation; subsequent user acceptance supersedes that pending status for the
reported Steam scenarios. No FPS improvement or broader stability is claimed.
