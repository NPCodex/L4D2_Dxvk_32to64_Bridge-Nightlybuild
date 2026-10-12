# C32 Surface upload wrap regression

Status: **AWAITING GAME VALIDATION**. The original `7e0ea4bbf6f7c4ea` experimental build regressed on the user's third-party map. The new ZIP fixes a reproduced Data Ring wrap defect and requires retesting that map; synthetic correctness does not establish the exact gameplay root cause or stability.

## Evidence

The same-session logs show C32 selected on both channels, a Client `IPC_DATA_FAULT` for command 349 (`IDirect3DSurface9_UnlockRect`) at 18:57:09.781, followed by Host command-loop termination at 18:57:09.786. At the failure, `reserved=1597445599`, `published=consumed=1597445590`, `capacity=25156544`, `request_words=0`, and no wait had started. The nine-word unpublished prefix matches the generic Surface layout: UID, RECT length and four DWORDs, flags, format, pitch. A previous session had the same nine-word difference and failure command. These are observations, not proof of the failed blob's dimensions: that size was discarded by the old failed-reservation diagnostic, and bounded resource snapshots do not recover it.

The production planner rejects a contiguous blob when payload plus skipped tail exceeds ring capacity, even if payload plus actual command fields fit at the head. In the scaled native repro, a 32,768-DWORD ring at position 17,011 receives a 20,001-DWORD blob reservation. Its footprint is 35,757, so the old planner rejects it before waiting. After retiring the unused tail independently, the nine-word prefix plus the same blob occupies 20,010 DWORDs and fits. The new test deliberately exercises this case with actual production serialization in separate processes.

## Change and ownership

For an otherwise fitting blob, the Client can relocate a bounded unpublished prefix (up to 256 DWORDs), publish an internal padding record, and restart encoding at the ring head. The Host consumes this internal record in the transport wait loop, never dispatching it as a D3D9 operation. It advances the Data Ring cursor and publishes pin-aware consumed credit before releasing message-block credit. Client allocation still waits for actual consumed progress; the published padding is not permission to overwrite retained data. Generic blob and complete known-packet allocation use the same recovery.

Existing command mutex, ordering, UID, pin ownership, backend calls and mapping sizes remain. Cursor advance is overflow checked; oversized payloads and prefixes outside the bounded migration remain rejected. No upload pool, helper thread, map-specific exception suppression, timeout extension or GPU-completion assumption is added. Invalid generic reservations now report requested words rather than an empty invalid plan. Modified protocol source changes both matched build identity and shared-channel hash; the old binaries cannot be mixed with the new ones.

## Validation and download

[Windows run 38103808315](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38103808315): 48/48 cross-process pairs passed, with three alternating C1/C32 repetitions for x86→x86 and x86→x64, on Device and Module channels. Twelve pairs run the new upload-wrap scenario, totaling 768 Surface uploads plus positioning uploads. Every byte and relocated RECT/scalar field is checked; internal padding must not reach dispatch, and UID order is preserved. The other 36 pairs retain scalar, mixed/pinned-upload, RPC and multi-thread coverage. Both standalone policy architectures, failure tests, eight existing native regressions and 39 Python contracts passed. The real Release x86 Client, x64 Host and x86 Host compiled and passed architecture, build-ID, /O2+NDEBUG and x86 Host LAA checks.

Raw test output: [38103808315-raw.json](ipc-c32-evidence/38103808315-raw.json). This is correctness validation, not proof of improved gameplay performance; previous C32 performance limitations remain.

Compiled source: `3607ceda2ec1dac2376864609984c2b8c44cd516`. Build: `ldb-ipc-c32-dev+87bac59e7c0b43b0`.

[Download matched experimental update ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/d49e59b5bdd6c36078160f1b3276602da81148b8/l4d2-bridge-ipc-c32-dev-87bac59e7c0b43b0.zip)

ZIP SHA-256: `1322ee966d84abb3c4764b9d3779f43d5e8a1dbc62fe7710b411bb65c79a3041`.

Exit both processes and back up/replace all three bridge binaries together using [the install/rollback instructions](IPC-C32-DEVELOPMENT.md). Retest the same failing map with default C32 first; preserve complete logs if it still fails. Oversized uploads can still fail with a meaningful requested size. The old ZIP is retained for reproducibility, not recommended for use. No main, PR6, VERSION, tag or release modification.

## Surface follow-up

The Surface-specific prefix migration path is superseded by [complete-layout preflight](IPC-C32-SURFACE-PREFLIGHT.md). The earlier artifact and evidence remain preserved; use the matched follow-up ZIP for testing this correction.
