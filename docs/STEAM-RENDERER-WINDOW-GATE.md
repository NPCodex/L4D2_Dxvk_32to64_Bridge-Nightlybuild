# Steam renderer sample: process-owned window selection gate

Status: **STATIC EVIDENCE / RUNTIME BRANCH NOT YET CONFIRMED**. Steam remains unfixed. This analysis reads the uploaded DLL without loading/executing it, modifying it or redistributing it. It introduces no bridge binary, runtime hook, default, backend, foreground or IPC change.

## Exact sample

User-supplied GameOverlayRenderer64.dll, 1,576,600 bytes, x64 PE32+. SHA-256: `59057f983ae6f02b7eb2495bf65e0430129dd81dff8d9628c764bd1a07e23fee`. Resource strings include version 10.96.30.42 / source-control ID 10963042 and an additional 1.0.0.1 block. Resource text is not signature verification. The user subsequently confirmed this is the DLL used by LDB64; actual call-site/branch execution still requires the runtime diagnostic. Addresses below are **RVAs for this sample only**; ASLR changes loaded VAs and future Steam versions can change all offsets.

## Verified static finding

An input-window selection helper at RVA `0xA69F0` calls imported GetActiveWindow at `0xA6ACE`. If it returns null, the helper calls GetForegroundWindow at `0xA6ADC`. It then uses GetWindowThreadProcessId to obtain the chosen window's PID and compares it with GetCurrentProcessId. A mismatch reaches the false-return path at `0xA6B28`, before the window-binding state is updated and the input-setup log is emitted.

Independently reconstructed selection logic, omitting timing/rehook bookkeeping:

```text
candidate = GetActiveWindow()
if candidate is null:
    candidate = GetForegroundWindow()
if candidate is null or owner_pid(candidate) != current_process_pid:
    return false
select/update the input window and its thread
```

This is not just a string/import guess. The imported API slots, calls, PID comparison and failure branch were checked in the disassembly. The PE exception table bounds this helper at `0xA69F0..0xA6BA8`. Its input-setup strings accompany the successful branch.

The Vulkan export at `0xC6AE0` calls the shared render/update routine at `0xBFD60`. That routine, whose exception-table range is `0xBFD60..0xC1328`, directly calls this helper at `0xC04FB`. Thus a path from Vulkan presentation to the window-selection gate exists in this exact sample; this does not establish that a particular game run executed it or passed its preceding guards.

Additional routines at `0xA50E0` and `0xA5F20` also obtain the foreground window's PID, compare it with the current PID and post different window notifications depending on the result. Their complete protocol semantics are not reconstructed here. This is a reason to inspect both active-window selection and foreground ownership, rather than assume keyboard focus alone supplies the Steam component's window identity.

Other sample findings:

- Local-thread WH_GETMESSAGE and WH_CALLWNDPROC installation paths exist. One installation sequence uses callbacks at `0xA6310` / `0xA55C0` and a stored thread ID or GetCurrentThreadId fallback. This does not establish that either hook was installed on the actual Host presenter thread.
- Steam uses its own input-event transport; a process-indexed event-stream name is present. Bridge WndProc counters are not proof of events reaching that transport.
- SteamOverlayIsUsingKeyboard and SteamOverlayIsUsingMouse both export the same RVA `0xB0900`, so their results cannot be treated as independent mouse/keyboard diagnostics for this version. Their helper includes timeout-state bookkeeping; these exports were not invoked here.
- VulkanSteamOverlaySetWindowType starts with `ret 0` at `0x3DED0`; it is a no-op in this sample. Calling it is not an evidenced window-association fix.
- A logger at `0xAB180` checks runtime state, then uses a runtime filename buffer and CRT file output. This DLL is not a log and contains no previous session's actual failure events; its file path is not recoverable merely from the zero-initialized buffer in the disk image.

## Relationship to the Bridge hardware evidence

Prior paired logs establish a game-owned foreground/active root in the x86 Client and a Host-owned presentation/focus child in the x64 Host. The actual backend focus and presentation parameters already use that child. The combined experiment supplies initial Host mouse messages, Host focus, complete shortcut state and Client query delegation, yet Steam remains unusable.

The sample adds a concrete possible explanation: a Steam input-selection call in the Host may see the game-owned root through GetActiveWindow, or null then the game-owned root through GetForegroundWindow. The PID comparison would reject that candidate even while the Host child receives Windows messages. Giving the child keyboard focus does not itself change the foreground root's PID.

This is a stronger and narrower hypothesis than an unspecified input loss. It is **not yet a confirmed sole cause**: the exact loaded DLL, Steam call thread, actual GetActiveWindow result, prior guards and ensuing input-hook/transport state are absent from existing Bridge logs. GetGUIThreadInfo snapshots alone do not establish what GetActiveWindow returned on Steam's rendering thread. Both processes also load Steam components; the actual receiving instance still needs association evidence.

## Concrete next diagnostic and compatibility design

First identify the loaded module version/hash and observe **Steam's own** GetActiveWindow/GetForegroundWindow calls without changing their return values. For this exact sample, return-site RVAs `0xA6AD4` and `0xA6AE2` identify the relevant selector. Record caller RVA/thread, returned HWND, its owner PID and Host PID. Confirm whether selection falls into the demonstrated mismatch case; a null active result followed by a Client-owned foreground result would directly support it. If a Host-owned candidate is observed, this gate does not explain that attempt and the next hook/transport stage must be traced.

If rejection is confirmed, evaluate an opt-in compatibility policy scoped to Steam's window queries: while the real game is foreground, expose the already-existing Host-owned presenter as the equivalent window to that Steam input-binding path. Other windows/applications and ordinary Bridge/game queries retain their real results. This avoids assuming that global foreground stealing or more synthetic input is required. Binding must also be evaluated before Overlay activation so shortcut opening is not circularly dependent on an already-active callback. Actual implementation must verify module lifetime, import/dynamic-call coverage, thread ownership, version differences, background behavior and teardown, then undergo native tests and real Steam validation. No private offset write or Steam DLL patch is implemented from this static finding.

## Reproduction

```sh
python scripts/inspect_steam_renderer.py /path/to/GameOverlayRenderer64.dll
objdump -p /path/to/GameOverlayRenderer64.dll
objdump -d -M intel --start-address=0x1800a69f0 --stop-address=0x1800a6ba8 /path/to/GameOverlayRenderer64.dll
objdump -d -M intel --start-address=0x1800c6ae0 --stop-address=0x1800c6bb4 /path/to/GameOverlayRenderer64.dll
```

The script reports file-backed PE metadata and hash-gated sample annotations. It never executes the supplied image. Full proprietary DLL bytes/disassembly and raw user logs stay in the local work/attachment area and are not published. main, prior tested IPC branch, formal version/tags/releases and experimental binaries are unchanged.
