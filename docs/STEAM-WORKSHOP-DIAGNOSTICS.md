# Add-ons Workshop guard and request diagnostic

Status: **AWAITING WORKSHOP DIAGNOSTIC RUN**. The retained input adapter passed
user shortcuts/mouse/Join Server testing on 294eafb0349ce3aa. This build adds only
an optional observer and does not claim a Workshop fix or broader game stability.

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
