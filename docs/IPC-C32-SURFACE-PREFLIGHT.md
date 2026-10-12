# Surface complete-layout preflight in PR7

Status: **AWAITING GAME VALIDATION**. This adapts the PR6 late Surface fix to PR7; it does not establish all-map stability or game FPS improvement.

## Scope and protocol

Ordinary and ATI Surface uploads declare their complete body before writing the UID, RECT or resource payload. Wire counts exactly match PR6: shared-heap body 9 DWORDs; direct upload body 9 + ceil(payload_bytes/4), plus the Client UID. This includes the RECT length/four coordinates, flags, format, pitch and the upload length or shared-heap ID. The complete interval must fit contiguously. When it does not fit the tail but fits an empty ring, a pin-aware internal padding record retires the tail before the upload is reserved at the head.

The entire interval is checked for capacity and available consumed credit once. Subsequent reservations retain wire/overflow checks, healthy-state checks and a declared-end bound, without repeating shared-space synchronization. Final commit must exactly reach the declared end with the blob closed; underfilled, overrun or oversize layouts poison the channel without publishing the application command. A failed oversize layout writes no UID/RECT/payload. Every newly introduced ownership operation remains under the existing command mutex.

The Surface path no longer depends on moving an already written prefix or the 256-DWORD migration limit. The previous bounded migration fallback remains for unrelated legacy upload paths; those are outside this Surface-only change. No API folding, VB/IB change, upload pool, reply mechanism, mapping-size change, retention policy change, timeout extension or new thread is introduced. Existing C1/C32 selection and default are retained. Matched binaries are required because the normalized protocol source hash/build ID changed.

## Validation

[Windows run 38109547433](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38109547433): 84/84 native process pairs, alternating C1/C32 and generic/preflight order, x86→x86/x64, Device and Module. Tests include normal scalar/mixed/pinned uploads, RPC, multithread order, repeated large Surface wraps, partial RECT descriptors, ordinary/ATI format descriptors, shared-heap IDs, zero/1/3/4-byte lengths and exact full-ring uploads. Host checks every payload byte and UID. Separate abort, missing reply, Surface oversize-before-write, underfill and overrun tests reject submission after fault. Two policy architectures, eight existing native regressions (including ATI layout/copy), and 39 Python checks passed. The descriptor fixture does not call a GPU: actual ATI copy semantics are covered by the retained ATI regression, not by claiming the descriptor mock is a real texture upload.

Actual Release x86 Client, x64 Host and x86 Host built with MSVC 14.29; architecture, matching embedded ID, /O2+NDEBUG compile receipts and x86 Host LAA were verified before packaging. Raw evidence: [38109547433-raw.json](ipc-c32-evidence/38109547433-raw.json).

## Measured small-upload cost

The first preflight run [38109200533](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38109200533) is also retained as [raw evidence](ipc-c32-evidence/38109200533-raw.json), with Client median increases of 2.2–9.8%. A redundant entry health check was subsequently removed: full reservation and each write still check health. Different CI jobs may use different hardware; do not attribute cross-run cycle differences solely to that edit. The table below is the final same-run control comparison.

Same-run synthetic generic versus declared Surface uploads, 64-byte payload and identical fields/validation, 12,000 uploads per channel. Each sample combines Device/Module thread cycles per upload; three repetitions per configuration. The table retains all ranges. It measures complete serialization/transport and scheduling/backpressure effects, not just layout arithmetic, and excludes real backend/GPU calls. No FPS claim or universal overhead bound follows from these samples. C1/C32 both force Surface publication.

| Host | Batch | Role | Generic median [min,max] cycles/upload | Preflight median [min,max] cycles/upload | Median change |
|---|---|---|---|---|---|
| x32 | 1 | client | 1283.6 [1271.8, 1396.6] | 1279.0 [1274.2, 1288.8] | -0.4% |
| x32 | 1 | host | 1333.1 [1331.4, 1447.8] | 1344.7 [1317.3, 1359.1] | +0.9% |
| x32 | 32 | client | 1290.5 [1248.0, 1426.5] | 1285.2 [1253.4, 1537.0] | -0.4% |
| x32 | 32 | host | 1359.7 [1289.7, 1476.3] | 1358.3 [1288.9, 1582.3] | -0.1% |
| x64 | 1 | client | 1299.5 [1260.7, 1479.9] | 1489.0 [1314.4, 1491.5] | +14.6% |
| x64 | 1 | host | 1526.9 [1486.5, 1727.8] | 1706.0 [1530.7, 1716.1] | +11.7% |
| x64 | 32 | client | 1313.2 [1303.3, 1321.9] | 1317.9 [1302.0, 1467.7] | +0.4% |
| x64 | 32 | host | 1569.6 [1536.1, 1640.5] | 1568.4 [1524.1, 1715.4] | -0.1% |

The final Client median changes are -0.4%, -0.4%, +14.6% and +0.4% for x86/C1, x86/C32, x64/C1 and x64/C32 respectively. The x64/C1 Host median also rises 11.7%. This is a material synthetic regression in that configuration; sample ranges overlap and only three repetitions are available, so its cause and persistence remain unresolved. This correctness patch is offered for game validation, not as a demonstrated performance improvement.

## Download, install and retest

Compiled source: `e2b41fd58bd6f118bb07ae550ce64813b5bb748a`. Build: `ldb-ipc-c32-dev+e12fa129cc05a413`.

[Matched experimental update ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/acfebdaca7371e2dd889ca3bd7c64bafa79b5572/l4d2-bridge-ipc-c32-dev-e12fa129cc05a413.zip). SHA-256: `e533b381a7ca205df6c733ac45a930d12361a2fcd7e8fdeb80781db2f439bca5`.

Exit the game and both Hosts; back up and replace the full matched three-file set using [installation/rollback instructions](IPC-C32-DEVELOPMENT.md). Preserve DXVK/configuration/plugins/retention DB. Keep the previously tested C1 setting when comparing this patch's performance; C32 remains selectable. Confirm the new build ID and batch in bridge32.log. Retest the previously failing map and consecutive map transitions, including ATI assets, read-only/shared-heap paths as applicable. Save full logs on failure. The package is an update for an existing installation, not a formal release. main, PR6, VERSION, tags and releases remain untouched.
