# Add-ons View Workshop: static API audit and next diagnostic

Status: **INVESTIGATING**. Steam input on 294eafb0349ce3aa is accepted in the
reported session. No new runtime behavior or bridge binary accompanies this
static API audit. Preserve that matched build and existing successful settings.

## Supplied API sample

The user supplied the game's steam_api.dll: 263080 bytes, x86 PE32, image base
0x3B400000, SHA-256
`25a1975363e587f7c64eee7cf2fdcb02eb7511e178f1ae0b38783322a2bd6339`.
Read-only PE/string/disassembly inspection was performed; the DLL was never
loaded/executed, modified or redistributed.

The sample contains SteamFriends017. Its flat export
SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage is at RVA0x20A0. The actual
thunk puts self in ECX, pushes the third argument (mode), pushes the second
argument (URL) and calls [vtable+0x78]. Thus the concrete sample's flat ABI is
cdecl(self, URL, int32 mode), and its virtual method has thiscall(URL, int32 mode)
at slot30 (0x78/4). It must not be replaced using an assumed older one-argument
web method or a guessed generic SteamFriends vtable index.

Other evidenced sample exports:

- SteamAPI_SteamFriends_v017, RVA0x5800: retrieves a cached/runtime interface via
  SteamInternal_ContextInit; the interface implementation is in the installed
  Steam client, not this API file.
- SteamInternal_FindOrCreateUserInterface, RVA0xA3A0: versioned interface factory.
- SteamAPI_ISteamClient_GetISteamFriends, RVA0x1980: flat call via SteamClient
  virtual offset0x20, with user, pipe and version arguments.
- SteamAPI_ISteamUtils_IsOverlayEnabled, RVA0x5660, and the v010 Utils accessor.

These are static sample facts, not evidence of which entrypoint/interface the
installed game menu uses. DLL strings/exports alone do not establish its caller,
requested interface version, early availability checks or request routing.

## Hardware scope

User confirms: shortcuts open/close, mouse and Join Server UI work; the Overlay's
own browser can open web pages; native DXVK's View Workshop button works. On the
candidate, clicking View Workshop does nothing (no Overlay) and other actions
remain responsive. Existing callback331 diagnostics record only activity, not
web requests/URLs. Neither provided log traces the menu's web request or any
availability predicate. Their successful hook/callback evidence cannot prove a
particular Workshop click dispatched a request.

## Required caller audit and implementation boundary

Inspect the actual menu module, normally game bin/gameui.dll, to identify the
View Workshop handler, availability guards and actual Steam accessor/call path.
In particular, a possible IsOverlayEnabled check must be established from the
caller rather than inferred as the cause. The supplied API module is sufficient
to describe its modern flat thunk but not the menu's control flow. If the handler
is delegated to another game module, follow that actual call rather than assume
a fixed DLL or interface. The menu module has not yet been supplied.

Then add a default-off observer at the verified caller/API boundaries that:

1. Records actual interface version, getter/availability result and immediate
   caller PID/thread/module RVA, without creating a second Steam context.
2. Captures both the concrete flat entrypoint and any verified C++ route; cached
   interfaces initialized before observation and unobserved routes are explicit
   coverage gaps. Zero flat calls is not proof of zero game requests.
3. Logs pre/post web invocation, original borrowed URL pointer classification,
   bounded Steam Workshop target metadata and mode. Do not log arbitrary URL
   credentials/query tokens, or mutate the pointer/mode/return/LastError.
4. Correlates the user action with activation/Host logs. Request instance/target
   PID is not an argument of this public method and must not be invented from
   the invoking PID. Separate evidence is needed for actual routing.
5. Rejects unknown interface signatures instead of applying slot30 universally.
   No inferred private Steam ABI, forced IsOverlayEnabled=True, duplicate page
   activation, browser fallback or navigation forwarding without evidence.

Native fixtures must exercise the exact real calling conventions/argument count,
original calls and LastError, interface mismatch refusal, cached-interface gaps,
bounded URL classification and observation cleanup. Keep the working input build
as the control. A new runtime ZIP is offered only after that observer is tested;
this audit does not claim to fix Workshop navigation.

## Reproduction

```sh
python scripts/inspect_steam_api.py /path/to/steam_api.dll
objdump -p /path/to/steam_api.dll
objdump -d -M intel --start-address=0x3b4020a0 --stop-address=0x3b4020b3 /path/to/steam_api.dll
```

The stdlib inspector's sample annotations require the complete SHA-256 match;
addresses do not apply to other Steam DLL versions. The
[derived report](steam-input-evidence/steam-api32-static-sample.json) contains
metadata, export addresses and a thunk hash, not proprietary binary bytes.
