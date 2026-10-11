# Steam Overlay input investigation after v1.1

Status: **Steam remains unusable. Query-chain delegation is verified but ineffective. Its hardware run again has game mouse capture; a one-option combination of existing mechanisms is pending, without a new binary or claimed fix.**

The new hardware report changes the scope: normal Shift+Tab does nothing, but L4D2's Join Server / Steam Group action can open a visible overlay. The visible UI accepts no mouse input and Shift+Tab cannot close it. This establishes that activation and rendering can work in this installation. Do not replace them or add a second Steam API activation path as an input workaround.

## What the source establishes

- The original game HWND belongs to `left4dead2.exe`. D3D9 commands cross the Bridge to the selected Host, where the actual DXVK device/swapchain and Vulkan presentation run. A Steam Vulkan layer intercepting that presentation must run in that Host. This does **not** prove which Steam component is drawing the reported overlay, receiving its input, or tracking its active state.
- With `server.presenterWindow=True`, windowed vanilla-DXVK presentation uses a Host-owned child of the game HWND. This child has its own message thread and starts **disabled**, so mouse input reaches the game. With the option off, presentation retains the original game-owned HWND. The normal foreground root remains the game window.
- The implemented ReShade public event changes `Presenter::capture`, enables the child for mouse input and sends the existing `UWM_REMIX_UIACTIVE_MSG` to the client. The client then neutralizes game input through its existing Win32/DirectInput/raw-input hooks. None of this state is currently connected to a Steam active/inactive notification.
- The client's legacy forwarding calls `gpRemixMessageChannel->send`, implemented as `PostThreadMessage` to the Remix renderer handshake thread. It does not target the presenter HWND. Vanilla DXVK does not implement that Remix handshake. The independent Bridge server message thread handles focus/timeout notifications; it is not a Steam input dispatcher.
- The existing foreground-scoped low-level keyboard hook posts legacy key messages to the presenter. Successful `PostMessage` means a message was enqueued; it does not prove that Steam consumes it, uses that message queue, or accepts synthetic legacy keyboard messages. It does not replicate raw input, Steam's internal focus checks or a thread's keyboard state.
- Steam and ReShade do not share a public overlay interface. Module presence and a visible UI do not prove that Steam's input hooks are attached to the presenter, nor that ReShade capture should be reused blindly. Two overlays will also need separate active states whose union controls game input suppression.

[Earlier recorded evidence](V1.1-VALIDATION.md) includes `renderer64=1` in the Host; it establishes that the renderer DLL was loaded in that run, not its current input receiver.

The unconnected Steam state is an established integration gap. Its causal relationship to this failure still requires runtime evidence. Focus changes, game mouse capture/clip, WndProc hook ordering and which process receives input remain competing explanations. There is no basis yet for changing global focus, attaching thread input queues, injecting Steam into another process, or creating another overlay/input subsystem.

## Changes in the diagnostic build

New options, both off when omitted:

```ini
client.steamInputDiagnostics = True
server.steamInputDiagnostics = True
logLevel = Info
logApiCalls = False
logServerCommands = False
```

Merge [the snippet](../config/STEAM-INPUT-DIAGNOSTICS.conf) into the existing `bin/.l4d2bridge/bridge.conf`. Preserve the current Host selection, presenter, ReShade and retention options. The normal shipped configuration is unchanged.

The window diagnostic helper runs inside both processes. It samples at most once per second from Present, including when the game stops receiving ordinary input messages. It also observes the legacy public Steamworks activation callback in the client and the shortcut's keyboard state in the presenter thread. With only diagnostic flags enabled, it does not change capture or input. The optional correction flags below enable a separate hardware experiment. Neither mode activates Steam or changes resource policy.

- `event=steam-callback`: registers callback 331 only when `client.steamInputDiagnostics=True` and the game's existing `steam_api.dll` exports `SteamAPI_RegisterCallback`. The independent ABI declaration follows Valve's Steamworks SDK 1.51: three callback virtual methods and the one-byte legacy `GameOverlayActivated_t` payload. It includes no SDK implementation, modern optional payload fields or Steam binaries. The observer/code modules stay alive for the process lifetime to avoid a callback pointing into unloaded code. It uses the game's own callback pump; it never calls `SteamAPI_Init`, `RunCallbacks`, `Shutdown` or an activation API. Registration is not proof of callback delivery on this installation.
- `event=steam-active`: records real callback delivery with `active=1/0`. This is distinct from the combined ReShade/Steam Bridge `capture` flag. Diagnostic-only mode performs no capture transition.
- `event=tab-dispatch`: for presenter Tab keydown messages only, records Shift/Tab from `GetKeyState`, `GetKeyboardState` and `GetAsyncKeyState` immediately before TranslateMessage/DispatchMessage. A mismatch establishes a queue-state difference, not proof that Steam uses a particular Win32 API. It logs no text or unrelated key identities. Earlier GetMessage hooks may consume messages before this observation.

- `event=modules`: process role, PID/architecture and actual presence of `GameOverlayRenderer.dll`, `GameOverlayRenderer64.dll`, `SteamOverlayVulkanLayer.dll`, `SteamOverlayVulkanLayer64.dll`, `steam_api.dll` and `steam_api64.dll`. Delayed load/unload is detected. Presence alone is not activity or rendering proof.
- `event=state`: game/presenter HWND ownership, presenter enabled/capture state, foreground root, foreground thread active/focus/capture HWNDs, local game/presenter thread GUI state, cursor hit-test HWND ownership, clip rectangle size and local WndProc address/module. `downstream` / `downstream_module` identify the client's saved game WndProc. The client observes the actual WndProc through the existing original API trampoline, bypassing Bridge's virtualized GetWindowLong result. `gui_ok=0` or `local_gui_ok=0` means that thread observation was unavailable. Host `capture` denotes ReShade/Bridge capture, **not Steam's active state**.
- `event=route` (client): whether the legacy Remix channel has a handshake target, and current message-pump/custom-hook options. `presenter_delivery=0` describes this legacy channel, not the separate presenter keyboard hook.
- `event=messages`: counts since the previous sample of mouse, keyboard, raw-input and client forwarding attempts, plus Shift+Tab keydown observations. Counts do not log typed text, general key identities, mouse coordinates or raw-input contents. Auto-repeat can increase the shortcut count.

Client counts are observed at the existing Bridge message-processing entry. The forwarding count means the existing message channel's `send` was invoked, not that Steam received it. Host counts are messages that reach the presenter's WndProc; messages consumed earlier by a Steam/ReShade GetMessage hook can be absent. A zero count alone cannot identify a broken route. Without a presenter, no Host window-message counts are collected. The diagnostic reads GUI state for the relevant foreground/window threads but does not profile other applications or hook another application's input.

## Short hardware reproduction

### Opt-in correction build

The 22:31–22:32 paired logs establish callback delivery: client PID 17584 registered callback 331 at `22:31:03.592` and received `active=1` at `22:31:52.543`. Host PID 20776 nevertheless remained at `enabled=0 capture=0`, with mouse hit testing on the game. Two shortcut attempts at `22:32:05.722` and `22:32:08.554` both reported `shift_queue=1 tab_queue=0 shift_async=1 tab_async=1`; `GetKeyboardState` likewise reported Tab up. No deactivation callback was observed before shutdown. This proves the client activation notification is available and that the forwarded Tab message lacks matching queue state. It does not prove which keyboard API Steam reads.

The correction experiment is off when these options are omitted:

```ini
client.steamOverlayInput = True
server.steamOverlayInput = True
```

It requires the existing windowed presenter configuration (`server.presenterWindow=True`, `server.presenterInput=True`) and existing client input hooks. Use the matching client and x64 Host update together. Preserve the current working ReShade settings, DXVK backend and retention policy. The packaged `STEAM-INPUT-DIAGNOSTICS.conf` now includes both correction and diagnostic flags; merge it rather than replacing the complete configuration.

The state-forwarding/capture experiment remains available only for isolated development tests. It forwards actual callback state to the Host-owned presenter, with independent Steam/ReShade capture sources. It is **not recommended for users**. The packaged diagnostic snippet explicitly disables both correction flags.

Commit `33e8cb2` also changed Host keyboard state before dispatch and moved focus to the child. The subsequent hardware test failed and reported gameplay input becoming unreliable. Those keyboard-state writes and focus changes have been removed. Current diagnostics only read keyboard state; the original v1.1 keyboard-hook behavior is restored. Disabling the two experiment flags preserves the normal ReShade input path.

The current observer additionally records `sdk_flags` and `sdk_callback` after `SteamAPI_RegisterCallback`. `registered` now reflects the SDK-written registration bit rather than merely successful export lookup/calling. The registration export returns void: reaching it did not itself prove acceptance. Missing activation events must be investigated independently of the Host capture path.

The initial registration-field test exposed a 32-bit layout mismatch in the standalone callback declaration (64-bit passed). The observer now inherits from a separate legacy callback base containing only the three virtual functions, byte flags and integer callback ID. Its size is asserted as 12 bytes on x86 and 16 on x64; SDK-manager writes to that base are tested independently from the derived C++ function objects. Events include `base_bytes` and `diagnostics_revision=2`. This repairs the ABI declaration and the reliability of registration-field observation; it does not prove that the earlier missing activation events were caused by that layout mismatch.

To restore normal input, set **both** `steamOverlayInput` options to `False` and restart. Keep diagnostic flags enabled for the short reproduction below. The normal release configuration and main branch remain unchanged.

### Confirmed failed correction run: 22:47–22:48

The client logged `state_forwarding=1`, and the Host logged numerous `keyboard-state updated=1` events. This confirms execution of the correction build, unlike the earlier run. The tester reported ReShade working, Steam visible but unusable, and gameplay movement/Esc/clicks responding unreliably. No client `steam-active` or `steam-forward` event was recorded despite the visible Steam page. Host `steam-capture active=0` appears only during teardown; there is no activation transition. ReShade capture toggled at `22:48:00.233` / `22:48:00.597`.

The keyboard-state modification failed to make Steam respond and introduced a reported input regression. The logs do not show the Steam focus/capture branch executing, so it cannot explain this run through a Steam activation transition. A Host-owned child can share an input queue with its cross-process parent; writing keyboard state at forwarded-message dispatch can affect input processing in that shared queue. This is a plausible explanation for the regression, not a completed A/B causal proof. The failed writes and the unvalidated focus changes are rolled back rather than expanded. Native capture-union tests did not validate real gameplay input or proprietary Steam hooks.

Current acceptance for the rollback build: ordinary movement/clicks/Esc work, ReShade Home works, and a game-requested Steam opening is recorded with its real callback/registration diagnostics. Continued Steam input failure is expected to remain unresolved. Do not use this rollback as evidence that Steam input has been fixed.

### Corrected passive registration run: 23:02–23:03

Both experiment flags were `False`. Client PID 14176 logged `registered=1 sdk_flags=1 sdk_callback=331 base_bytes=12 diagnostics_revision=2` at `23:02:13.823`, confirming the SDK-written legacy base fields in the actual x86 game. At `23:03:04.515`, it received `steam-active active=1`. The tested reproduction is the game's built-in **Join Server** action opening a rendered Steam Overlay, not Shift+Tab opening it. Both logs ended with successful shutdown cleanup.

All 57 sampled Host states remained `enabled=0 capture=0`; mouse-message counts remained zero. This is expected with capture disabled and is not evidence that the corrected capture path was attempted and failed. No keyboard-state-write events were present. The logs alone cannot verify subjective recovery of movement/click/Esc responsiveness, or establish that the ABI correction caused activation delivery to resume.

The next isolated development test can reuse the verified `192970a` client/Host pair, enabling both `steamOverlayInput` flags solely for activation-state forwarding and existing presenter capture. The withdrawn keyboard-state writes and focus changes are absent even when these flags are enabled. First check ordinary gameplay input before opening Steam, then open it through Join Server and observe `steam-forward`, `steam-capture`, presenter enabled state, client input suppression and actual Host mouse-message delivery. This tests routing and mouse acceptance; it does not establish a Shift+Tab fix. If the page remains unusable, terminate the run, preserve the logs and disable both experiment flags again.

### Capture routing run: 23:09–23:11

The tester confirmed ordinary movement before opening Steam, an unusable visible overlay afterward, and mouse clicks no longer reaching the game behind it. Client PID 20912 received `steam-active active=1` at `23:10:28.880` and logged `steam-forward active=1 presenter=0x000D08D8` at `23:10:28.893`. Host PID 21252 logged `input-capture active=1` and `steam-capture active=1` at that same time. Subsequent state samples show `enabled=1 capture=1`, cursor hit testing on the Host-owned presenter, and client `capture=1`. Host WndProc mouse observations are nonzero (for example 57 in the `23:10:52.811` interval), while client mouse observations fall to zero. This verifies the activation relay, presenter enablement and gameplay input suppression; it does not verify Steam consuming that input.

Foreground/focus remain on the game HWND `0x002C0C60`, owned by the client. The Host WndProc head remains in `OpenGL32.dll`; its basename alone does not identify the entire Steam/ReShade hook chain or prove which component consumes input. During capture, forwarded Tab dispatches report both queue and asynchronous key state up. The existing low-level hook consumes physical keys while captured; synthetic window messages are not equivalent to OS keyboard state. The failed keyboard-state-write workaround remains removed. Both processes exit with successful cleanup.

The remaining fault is downstream of Bridge mouse routing. Steam may use another window/queue, client-side input hooks, polled keyboard state, or a focus condition; none of those proprietary paths is identified conclusively here. A same-build run with ReShade actually unloaded (not just effects disabled or the panel closed), while retaining presenter/capture options, can isolate overlay hook coexistence before another input-system change. This A/B does not assume ReShade is the cause. Compare Steam activation relay, actual mouse delivery, hook-head module and user interaction results.

### ReShade-off comparison logs: 23:32–23:33

The client again confirms legacy callback registration (`sdk_flags=1 sdk_callback=331 base_bytes=12 diagnostics_revision=2`). It receives `active=1` at `23:33:32.352` and forwards that state at `23:33:32.356`. Host PID 23864 enables capture at that time; later samples show cursor hit testing on presenter `0x00130BA0` and nonzero mouse observations (52 at `23:33:36.757`). Focus remains on the game HWND `0x001B0B98` in client PID 9460. No ReShade API registration event appears in this run. Both processes report successful shutdown.

The hook-head module remains `OpenGL32.dll`, exactly as in the earlier run. A module basename cannot identify whether that is the Windows system DLL or another file with that name; it must not be labelled ReShade without a full module path. Missing API registration alone does not enumerate or prove absence of every ReShade component. The tester's actual interaction result and Host module paths remain necessary to interpret this A/B. The logs establish routing/capture, not Steam accepting the mouse or a proven ReShade conflict.

The tester subsequently confirmed that Steam remained unusable and could not close after disabling ReShade. No ReShade public API registration is present in this run. A ReShade conflict is therefore not an established explanation or the current fix target.

### Opt-in native input experiment

An independent, default-off option `server.steamNativeInput=True` requires both existing `steamOverlayInput` options to be enabled. It changes input only after actual Steam activation: when the game is foreground, the presenter thread gives focus to its own child. Once that child has focus, the existing low-level hook passes physical keyboard events to Windows instead of posting duplicates and blocking them. The client's existing UI-active hooks continue suppressing gameplay. No keyboard state is written, no new input queue is attached, and no global foreground/Steam API hook is introduced. The previous focus window is restored only when focus still belongs to the presenter; background deactivation does not foreground the game. ReShade-only capture retains its existing keyboard path.

This tests the established focus/physical-keyboard-routing gaps, not a proven proprietary Steam focus check. Logs include `native-focus acquired=1/0`, ordinary focus HWND ownership and Tab queue/async observations. `window-hook` now gives the actual hook-head module path when it changes, without identifying Steam's entire hook chain. Native tests check that a background Steam activation does not steal focus and that combined Steam/ReShade capture still restores correctly. Foreground keyboard/mouse acceptance and Steam closure require hardware validation.

Use a matching new client/Host pair. Preserve the ReShade-off baseline for the first native test, open through Join Server, test mouse and keyboard, attempt Shift+Tab close and verify input returns to the game. Closed-state Shift+Tab opening remains a separate acceptance condition, not a promised feature. Disable all three input experiment options and restart to roll back.

### Follow-up run: 22:42–22:43

The tester reported ReShade working and all Steam interactions still failing. Both logs list `client.steamOverlayInput=True` and `server.steamOverlayInput=True`. However, registration still reports `capture_changed=0`, rather than the correction branch's `state_forwarding=1`. The client receives `active=1` at `22:43:11.977`, but no `steam-forward` event follows. Host shortcut dispatches retain `tab_queue=0` and have no `keyboard-state` correction events; no `steam-capture` event is present. ReShade capture toggles at `22:42:39.233` / `22:42:39.699`.

This run establishes continued failure and no observable execution of the correction paths. It does not yet establish failure of those paths after activation. First compare the installed client and Host SHA-256 against artifact `11392840438` (commit `33e8cb2`, run `37419352518`). The verified artifact contains the correction option names and event strings. Older diagnostic binaries accept/log arbitrary configuration keys without implementing the new correction. A version/loading mismatch is consistent with these observations; installed binary hashes and, if necessary, actual loaded module paths are required before asserting its cause or changing the input design again.

Back up the current client/Host binaries. Install the matching diagnostic client and Host together; keep the existing DXVK backend, ReShade and database. Stay in the main menu, preferably outside a live multiplayer session.

1. Enable the two flags above. Start with the exact configuration that reproduces the issue. Note whether `server.presenterWindow` and ReShade are enabled, and which Host runs.
2. Wait roughly 5 seconds. Press Shift+Tab once, then wait 5 seconds.
3. Use L4D2's Join Server / Steam Group action that already opens Steam. Record the approximate open time.
4. Move the mouse, attempt one click, press Shift+Tab once to close, and wait 5 seconds. Report which actions worked. If it remains stuck, terminate the game as before; this diagnostic build does not promise an escape key or a fix.
5. Send `bridge32.log` and `bridge64.log` for x64 Host, or `bridge32.log` and `bridge-host32.log` for x86 Host. Preserve this failed run before restarting; the logs can be overwritten. Include the current presenter/ReShade settings and the times of opening/closing attempts.

For the activation/keyboard-state build, wait five seconds after opening through the game action, then make two deliberate Shift+Tab attempts (hold Shift first, tap Tab, release both; wait two seconds between attempts). Try one mouse click. Do not repeatedly hold Tab, since auto-repeat obscures individual attempts. Keep all existing settings and the renderer backend unchanged. If the overlay cannot close, preserve the logs after terminating. A missing `steam-active` transition despite successful callback registration is itself a useful result; do not compensate by assuming visibility from the hotkey.

One short reproduction is enough for the first diagnosis; no ETL/system-wide profiling is needed. Disable both flags after testing. If a presenter-on/off A/B is necessary, choose that after inspecting this run, rather than changing several input policies at once.

### Native-input hardware result: 23:54–23:57

The tester reports the experiment still ineffective. The paired logs confirm `server.steamNativeInput=True` and both capture-relay flags enabled. Client PID 6944 receives `steam-active active=1` at `23:56:26.479`, forwarding it to presenter `0x003C102A` at `23:56:26.492`. Host PID 11232 reports `native-focus acquired=1` at `23:56:26.501`. Subsequent **both-process GUI thread observations** confirm focus on that Host-owned presenter, while foreground/active remain the game HWND `0x00121184`. This is actual child keyboard focus, not merely a successful queued request.

Tab dispatches at `23:56:27.781` and `23:56:28.269` report Shift and Tab down in all three observations: queue state, asynchronous state and `GetKeyboardState`. Steam still does not accept input or close according to the tester, and no deactivation callback occurs before shutdown. Moving keyboard focus and delivering matching real OS key state are therefore insufficient in this configuration. This does not prove that Steam reads those APIs on that thread, or that its foreground/root-window conditions are satisfied.

The actual Host WndProc head resolves to **`C:\Windows\system32\OpenGL32.dll`**. The client head resolves to the installed `bin\DXVK_D3D9.DLL`, with saved downstream procedure unresolved. These observations do not enumerate either complete hook chain; the Host path must not be labelled a ReShade proxy. Steam modules remain renderer32 + API32 in the client, renderer64 + VulkanLayer64 in the Host, with no Steam API in the Host.

Mouse capture remains on the **game** HWND in many post-activation samples, even when hit testing and keyboard focus are on the presenter. In sampled intervals after native focus acquisition and before `23:57:00`, client diagnostics count 1447 mouse and 2153 raw-input observations, while Host diagnostics count 38 mouse and 69 keyboard observations. Counts are not unique physical events; first intervals can span activation. The route differs from the earlier capture-only run, and keyboard focus does not establish ownership of mouse capture. Both processes report successful shutdown. There is no claim of focus restoration being verified during normal Steam closure, since closure did not occur.

### Focused client suppression diagnostics

The development build now observes immediate callers at the existing client input-neutralization boundaries when `client.steamInputDiagnostics=True`. It retains all current input behavior and records no key identities, typed text, cursor positions or raw-input payloads. No additional configuration is required to enable these observations in the existing short diagnostic reproduction.

`event=input-suppression` identifies API, action (`neutralize` or `stop-hook-chain`), thread, return address and resolved module path. Covered boundaries are cursor polling/recentering, Win32 keyboard polling, raw-input reads, DirectInput state/data and the client Win32 hook-chain cutoffs. Each API/return-address pair logs once per thread, bounded at 128 sites; saturation emits `event=input-suppression-limit`. Module lookup occurs only on first observation of a site, without creating hooks, loading Steam components, altering data or bypassing gameplay suppression. These are **immediate callers**, not reconstructed call stacks or proof of the ultimate consumer behind a wrapper/trampoline. Absence of a Steam caller cannot establish absence of Steam input activity.

Source inspection shows two concrete boundaries requiring attribution: client Win32 input getters neutralize every caller during UI capture, and `client.overrideCustomWinHooks=True` makes the client hooks return before `CallNextHookEx` while captured. The latest run enabled that option. Stopping a hook chain can hide input from downstream hooks as well as the game; the code does not identify which downstream Steam hooks are installed. The new observations distinguish these boundaries from Host focus failure without yet assuming a Steam-specific bypass is safe. Existing GUI samples continue to report actual mouse-capture ownership.

Use the matched diagnostic client/Host pair, preserving the latest test configuration and backend. Open through Join Server once, try mouse movement/click and Shift+Tab, then preserve both logs. The update is diagnostic, **not a claimed fix**. Disable the three Steam experiment flags and restart to return to normal release input behavior. ReShade's public-interface capture remains separate and unchanged. The next functional change must follow evidence about the actual Steam receiver or hook-chain boundary; repeatedly modifying Host key state is not justified by this failed native-input result.

### Latest suppression run: 00:23–00:25; investigation paused

The diagnostic build ran successfully with both capture-relay flags, native input and both diagnostic flags enabled. Client PID 7192 received Steam activation at `00:25:19.541` and forwarded it at `00:25:19.547`. Host PID 22372 acquired presenter focus at `00:25:19.559`; GUI samples independently confirm it. Shift+Tab dispatches at `00:25:23.364` and later report both keys down in queue, asynchronous and keyboard-state observations. Mouse capture nevertheless remains on the game HWND in the sampled active intervals. No Steam deactivation callback is recorded before successful shutdown of both processes.

The new immediate-caller observations identify `left4neko.dll`, `skeeto.dll`, the Bridge client, `tier0.dll`, the game's `bin/libcef.dll` and Windows `textinputframework.dll` at neutralized input APIs. Hook-chain cutoffs identify USER32 as the dispatcher. No observation directly identifies `GameOverlayRenderer.dll` as a neutralized API caller, and no diagnostic saturation event appears. This does **not** rule out Steam behind a wrapper or a stopped downstream hook; the game's libcef cannot be labelled Steam's browser simply from its name. The logs therefore do not establish a Steam-specific bypass or a complete root cause.

At the user's request, investigation is paused rather than expanded into further speculative input changes. Steam Overlay input remains unsupported; these experiments are not a release fix. No further implementation or test build is required for this pause. Preserve the diagnostic evidence and existing ReShade implementation. Disable `client.steamOverlayInput`, `server.steamOverlayInput`, `server.steamNativeInput`, `client.steamInputDiagnostics` and `server.steamInputDiagnostics`, then restart, to deactivate the Steam experiments and logging. This restores the release input policy, not functional Steam Overlay support. Disabling the Steam Overlay for L4D2 in Steam's per-game settings avoids the confirmed visible-but-unusable overlay while support remains unresolved.

## Historical fix proposal and acceptance gate

### Paired hardware reproduction: 21:29–21:30

The tester explicitly confirmed entering a server and opening Steam through the Steam Group UI. The overlay rendered, but mouse clicks continued to operate L4D2 behind it and Shift+Tab did not close it. This run therefore includes a visible-overlay failure, unlike the preceding run whose visible-overlay state was unconfirmed.

- Client PID 3612 loaded `GameOverlayRenderer.dll` and `steam_api.dll`. Host PID 21280 loaded `GameOverlayRenderer64.dll` and `SteamOverlayVulkanLayer64.dll`; neither Steam API DLL was loaded in the Host. Vulkan presentation runs in the Host, but these module observations do not isolate which Steam component draws each UI element.
- Game HWND `0x00260558` belongs to the client; presenter HWND `0x00080FB0` belongs to the Host. All 73 sampled Host states reported `enabled=0 capture=0`. In foreground-game samples, focus and cursor hit testing remained on the game. The counter totals were **0 Host mouse messages**, versus **6642 client mouse observations**. These are diagnostic observation counts, not unique physical events or counts specifically confined to the overlay-open interval.
- Host counters recorded 90 keyboard messages and 9 Shift+Tab observations. For example, `21:30:20.781` reported `keyboard=8 shift_tab=2`, and `21:30:22.785` reported `keyboard=10 shift_tab=2`, while capture remained off. The shortcut reaches the presenter WndProc; enqueue failure is not the explanation for this reproduction. This does not prove that Steam's input hook accepted it.
- The client legacy Remix channel remained unready (`remix_channel_ready=0 presenter_delivery=0`). This route does not supply mouse input to the vanilla-DXVK presenter. Client counts can include both message-pump and WndProc observations; twice the Host shortcut count does not establish duplicate injection.
- Both logs ended with successful shutdown cleanup. No crash was reported in this reproduction.

The established mouse-routing gap is that opening Steam never enables the Host presenter or activates the existing client input-suppression path. The keyboard shortcut is a separate unresolved acceptance problem: posted legacy messages alone do not reproduce a real input-queue keyboard state, raw input or Steam's focus assumptions. These logs do not identify which of those checks Steam uses.

The next focused integration should observe Steam's actual activation/deactivation in the game, then connect that state to the existing presenter capture mechanism independently of ReShade. Steam and ReShade active states must be combined so closing one cannot release the other's capture. Hotkey diagnostics should inspect only Shift/Tab state at the dispatch boundary before choosing a keyboard-state or focus correction. Do not implement hotkey-based guesses of overlay visibility, add another activation/rendering path, or describe this diagnosis as a working fix.

Use the paired observations to identify the actual receiver and missing transition. Prefer the existing presenter/message-channel/game-input suppression infrastructure. Do not infer Steam active state by toggling a boolean on Shift+Tab: Steam can open through game requests, hotkeys can fail and focus can change.

Required hardware acceptance: open through the game and Shift+Tab, mouse movement/clicks, keyboard input, repeated Shift+Tab close/open, input returning to the game, Alt+Tab, and ReShade coexistence. Windows native tests can validate the observer and presenter mechanics but cannot claim proprietary Steam Overlay compatibility.

The v1.1 release remains the tested baseline. The investigation does not retroactively mark Steam input as supported, and its findings must not be described as a released fix.

## Attribution

Existing NVIDIA Bridge notices are retained. Newly added diagnostic code is MIT licensed:

The Steam callback declarations are independently written against Valve's public legacy binary interface, referenced from [Steamworks SDK 1.51 steam_api_common.h](https://github.com/ValveSoftware/Proton/blob/proton_9.0/lsteamclient/steamworks_sdk_151/steam_api_common.h) and [isteamfriends.h](https://github.com/ValveSoftware/Proton/blob/proton_9.0/lsteamclient/steamworks_sdk_151/isteamfriends.h). Valve retains ownership of its Steam/Steamworks components and SDK. Those components are not redistributed or relicensed as project MIT code. Native API test doubles verify observer mechanics, not proprietary Steam compatibility.

`All newly added implementation code in this fork was generated by OpenAI Codex from prompts and specifications provided by yeyunyyds.`

## Resumed: Client hook-chain correction

The user requested resuming from the documented failed state, with Steam as the first target and ReShade excluded from the first hardware reproduction. The prior failed keyboard-state and focus results remain the evidence baseline. This does not undo those rollbacks or assume ReShade caused the failure.

Source inspection establishes that WH_CALLWNDPROC, WH_GETMESSAGE and WH_KEYBOARD immediately return 0 whenever the combined Bridge UI capture is active. Although 0 is often a non-blocking hook result, omitting CallNextHookEx prevents lower hook procedures from observing the event. Existing diagnostics confirmed these cutoffs with USER32 as immediate dispatcher; they did not identify the lower hook owners or prove that this is the sole Steam failure.

The isolated correction adds `client.steamHookChainPassThrough=False` by default. It requires `client.steamOverlayInput=True`, the real legacy Steam activation callback, and current active state. In that state only, each of the three hooks calls CallNextHookEx exactly once with its original handle/code/arguments and returns its downstream result unchanged. No HWND/MSG payload is rewritten. Negative hook codes always delegate. Inactive Steam, unknown state, disabled experiment and ReShade-only capture preserve the prior cutoff policy. Game WndProc/message-pump suppression, Win32 polling neutralization, raw input and DirectInput suppression remain unchanged. No process injection, activation API, emulated key state, new focus change, cursor-capture change or input thread is added.

This deliberately allows every lower hook, including possible game hooks, to observe events. WndProc/polling suppression still exists, but a custom hook can act directly; native tests cannot guarantee no input leaks in every MOD. It also does not release the game's mouse capture, bypass client polling for Steam callers, or supply Steam's proprietary cross-process foreground checks. These unresolved boundaries are recorded instead of claiming this candidate fixes all input.

The matched experiment is based on the tested IPC six-gear development branch, in independent branch `codex/steam-overlay-hook-chain`. Normal defaults remain C1 and all Steam experiments off. Merge [the dedicated snippet](../config/STEAM-HOOK-CHAIN-EXPERIMENT.conf) into the existing configuration, preserving Host, DXVK, IPC batch and retention settings. Remove ReShade binaries from the first test process using your normal backup/disable method; closing its panel is not unloading it. The snippet retains the existing native-focus test setting for comparison; this change introduces no new focus behavior.

Test the same build with only `client.steamHookChainPassThrough=False` versus `True`. Start in the menu, verify normal movement/Esc/clicks, open through the game's Steam Group/Join Server entry, try mouse movement/click and Shift+Tab closure, then verify game input returns and try Shift+Tab opening from closed state. Preserve full Client/Host logs. A True run should contain real `steam-active active=1`, state relay/capture, and `event=input-suppression action=pass-hook-chain` instead of cutoffs while active. These observations prove execution, not proprietary Steam input acceptance. Missing hook events can indicate the hooks were not installed or not invoked. If behavior still fails, retain the actual route evidence rather than retrying Host keyboard-state writes.

Rollback by restoring the matched three old binaries and configuration, or disable the new switch plus client/server steamOverlayInput and server.steamNativeInput, then restart. Steam diagnostics can be kept for a brief reproduction and disabled afterward. Acceptance still requires opening/closing, mouse/keyboard, no gameplay leakage or stuck keys, repeated cycles, Alt+Tab and later optional ReShade coexistence. Until that test, status is AWAITING GAME VALIDATION, not a release fix.

### Resumed experiment build and delivery

[Windows run 38129815967](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38129815967) passed. Compiled source: `bf315fd6b0316db3485bcb59fdb948fa2a54a1e7`. Build ID: `ldb-ipc-c32-dev+ca4679629ca93d4a`.

- Production-method hook fixture: 72 hook-chain combinations per execution, 3 experiment modes on x86 and x64 (432 decisions), preserving downstream arguments/results and gameplay keyboard-poll suppression. Local extracted production methods also passed all 3 modes.
- Actual cross-process Presenter regression in both architecture directions, including callback ABI/capture union, route/resize and teardown, passed. Fake ReShade/Steam API modules validate mechanics, not proprietary Steam behavior.
- Retained six-gear IPC: 252 process pairs, 30 fail-closed scenarios, both architecture policy tests, 8 prior regression suites plus Presenter, and 39 Python checks passed.
- Actual MSVC 14.29 Release Client/x86 Host/x64 Host builds passed. Architecture, matching embedded IDs, binary hashes, x86 Host LAA, build receipts and ZIP checksum verified. Public fork download rechecked against the local ZIP; naming is Steam-specific but the embedded IPC development identity prefix is retained.

[Download matched Steam hook-chain experiment ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/bec1cdd5a8b2899d902b144aa01c9c50ca40fb0d/l4d2-bridge-steam-hook-chain-dev-ca4679629ca93d4a.zip). SHA-256: `3d049374eee5a24792be3e5f0614bfd54549acf4f97595a4b35addb2e809726e`. Install the ZIP's entire `bin/` set over the backed-up existing bridge; merge `config/STEAM-HOOK-CHAIN-EXPERIMENT.conf` into the existing config, preserving backend and IPC settings. Do not replace the complete config. Check both logs for build suffix `ca4679629ca93d4a` before testing. Toggle only the new flag for the same-build control run and restart between runs. Restoring the previous matched binaries/config is the rollback.

Evidence: [native pass excerpts](steam-input-evidence/38129815967-native-pass.log), [complete IPC repetitions](steam-input-evidence/38129815967-ipc-raw.json), [fault cases](steam-input-evidence/38129815967-ipc-faults.json), [upload/checksum receipt](steam-input-evidence/hook-chain-upload-receipt.json).

main remains `de74cd7d5dd546bfd4f03ab451d0565f1fff0107`; the IPC development branch remains `00007e8762385704afdacabea6e48444d798d27a`. No formal version, tag, release or earlier ZIP was overwritten. Status: **AWAITING GAME VALIDATION** for Steam; the source defect in downstream hook delegation is corrected, while Steam opening/closing/input compatibility is still unproven.

### Hardware feedback: hook-chain candidate ineffective

The user reports the resumed candidate is ineffective. This establishes that the offered experiment did not resolve the reported Steam problem; it must not be presented as working Steam support. No paired logs or effective configuration accompany this feedback, so actual build identity, activation delivery, pass-hook-chain execution, foreground/focus and mouse capture in this run are not yet verified. The report does not prove either that hook-chain delegation never executed or that no relevant hook lies downstream.

Keep the experimental branch unmerged. Revert the new flag and client/server Steam capture plus native-input flags, or restore the previous matched IPC bridge/config. Preserve this failed run's Client log, corresponding Host log and actual bridge.conf before restarting. The next investigation must first distinguish missing activation/relay from executed-but-insufficient delegation, then inspect actual input receiver and mouse capture. Previous failed keyboard-state/focus experiments remain excluded as default next steps. No new speculative input change or binary is offered solely from this brief feedback. Native test results remain mechanics evidence, not game acceptance.

### Paired hardware logs: hook delegation confirmed; mouse capture remains on game

The newly supplied logs verify both binaries are `ca4679629ca93d4a`, x86 Client and x64 Host, same session, C1 on Device/Module. Effective startup configuration enables all earlier Steam flags and the new chain passthrough. The separately uploaded bridge.conf instead disables message/custom hooks and omits the experiment flags; it is not the configuration represented by this run. Use effective startup logs for this investigation. The logs also show ReShade API registration, so this is not a ReShade-unloaded run; prior documented unloaded tests already failed and ReShade remains outside the new correction target.

Steam callback 331 registers successfully. At log time 14:06:31.078 the Client receives active=1, relays it at .085, and Host capture enables. Client WH_CALLWNDPROC and WH_GETMESSAGE log pass-hook-chain at .090/.099. No WH_KEYBOARD pass event is recorded; its execution is not established merely because that path exists. Host native focus acquisition at .099 and subsequent both-process GUI samples confirm focus on the Host-owned presenter. Fourteen Host tab-dispatch observations exist across the whole run; post-activation observations show Shift/Tab down in queue, async and keyboard-state data, but the user reports Steam remained unresponsive and no Client deactivation callback appears before shutdown.

For the thirteen samples at ticks 427031 through 439031, both processes report mouse capture on the game window, while foreground root is game and keyboard focus is the Host child. Host mouse observation total is 0; Client totals are 804 mouse and 1,079 raw-input observations, with Host 65 keyboard and 12 Shift+Tab observations. These counters are not unique physical events and the first interval crosses activation. They establish the sampled route, not which proprietary Steam receiver is expected. After Alt+Tab, capture clears and Host mouse observations become nonzero; Steam still did not operate according to the overall feedback. Cursor clipping remains desktop-sized, so no clip correction is justified by this run. Both processes shut down successfully; no IPC timeout/fault is observed.

The failed candidate therefore executed at the observed hook boundaries and was insufficient. The next isolated experiment is [game-thread mouse-capture handoff](STEAM-MOUSE-CAPTURE-EXPERIMENT.md). It is independent from the failed chain-only toggle and introduces no keyboard-state, focus, foreground or ClipCursor change. Keyboard acceptance and the identity of Steam's actual input receiver remain unresolved. User raw logs/config are kept in the local attachment workspace; they are not published to the repository.

The mouse-capture candidate has now been compiled and delivered from the separate `codex/steam-overlay-mouse-capture` branch; see its [verified build, immutable ZIP and test evidence](STEAM-MOUSE-CAPTURE-EXPERIMENT.md#verified-build-and-delivery). Detailed CI log review also found repeated MSVC environment initialization leaving some earlier fixture architectures unverified despite requested x86 names. The native suites are now isolated and Steam fixtures directly check PE architecture; final run 38176402172 supplies verified x86/x64 coverage. Prior successful fixture executions establish their tested mechanics, but earlier naming-based architecture claims are superseded. The new default-off mouse candidate remains **AWAITING GAME VALIDATION** and is not demonstrated working Steam support.

Subsequent [Client-only feedback](STEAM-MOUSE-CAPTURE-EXPERIMENT.md#subsequent-client-only-hardware-feedback) reports continued failure on the new binary, but its handoff switch is absent/default False and the earlier chain switch remains True. All 89 sampled states have no mouse capture, including 18 after Steam activation. This further shows sampled game mouse capture is insufficient as a sole explanation of Steam failure. The same-session Host log is needed to verify actual message delivery and shortcut/input acceptance; mouse hit testing and focus alone do not establish them.

### Host counterpart supplied: delivery confirmed, Steam acceptance unresolved

The supplied Host log matches the Client's `edc0a4d32de8ff07` build, C8, game/presenter ownership and activation/relay times. The 18 sampled intervals after Steam activation record 933 Host mouse observations, 103 keyboard observations and 18 Shift+Tab observations. Nineteen Tab dispatches include 18 with both keys down in queue/async/keyboard-state data and one with Shift up; these are observations, including repeat, not distinct attempts. All 89 Host and Client GUI samples show no mouse capture; 15 post-activation samples have Host focus. Successful shutdown includes a Bridge capture clear, without a Client Steam deactivation callback. These facts establish normal window-message delivery and matching shortcut state, while the user still reports Steam unusable.

Source audit confirms configureOverlayPresenter already returns the Host child as non-null backend focus and assigns it as hDeviceWindow. The earlier CreateDevice `focus` log printed the original game argument; it must not be used to infer backend focus routing failure. The diagnostic label now distinguishes game_focus from backend_focus without changing routing.

Four Client polling getters still stop before the stored original API chain while UI-active, even though the earlier WH_* policy delegates. Immediate caller logs naming game/MOD modules do not prove no Steam hook is downstream. The next [query API chain experiment](STEAM-POLL-CHAIN-EXPERIMENT.md) delegates these originals into scratch storage and continues returning neutralized/virtual values to the game. It does not change setters, raw input, DirectInput, Host focus, backend or IPC. This tests a concrete remaining suppression boundary; it does not identify Steam's actual receiver or establish the unique cause. Mouse-handoff retesting is not a prerequisite. Raw user attachments remain private and are not committed.

### Query candidate hardware failure and renewed capture obstruction

The next paired logs verify new build `62934c05672b373c`, enabled polling/WH chain flags and poll-chain-neutralize observations for all four getters. The user reports no improvement. This is an executed-but-ineffective candidate, not a missing-toggle explanation. The runtime config disables mouse handoff; its separately uploaded short configuration does not match effective startup settings.

During 34 post-activation samples, game capture remains present with Host focus: Host mouse observations are 0, versus 99 keyboard and 9 complete shortcut observations; Client mouse observations are 1,745. After switching away, 6 capture-free sample intervals provide 224 Host mouse observations, but no Host keyboard observations and no restored Host focus in the four foreground-game samples. Thus this run does not establish all mouse/keyboard/polling boundaries working together. Source/backend focus routing is independently confirmed by the corrected device log. No Steam deactivation callback occurs before clean shutdown.

The bounded next comparison [combines the existing mechanisms](STEAM-POLL-CHAIN-EXPERIMENT.md#next-controlled-comparison-existing-binary-one-option-change) by changing only mouse handoff False -> True on the same binary, keeping C8 and other effective settings. This does not establish capture as the unique cause or promise a keyboard fix. If the combined boundary conditions execute and Steam still fails, obtain Steam's own renderer/UI or input-hook association evidence before additional behavioral patches. No new bridge binary is compiled solely for this feedback.
