# C32 experimental bridge validation and delivery

**Superseded:** the original build regressed in third-party map loading. See [the wrap-fix investigation and replacement build](IPC-C32-WRAP-FIX.md).

Status: **AWAITING GAME VALIDATION**. Native correctness and the matched Release build passed; production merging remains gated on real L4D2 testing.

Implemented: C32 full-batch/capacity publication, conservative forced semantic boundaries, API-entry lazy age check, inline scalar records, and ordered descriptors retaining PR6 Data Ring ownership. Replies, large payloads and Lock buffers retain the correctness foundation. No independent upload pool or reply slots are integrated.

The final Windows run [38102076636](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38102076636) built the real x86 Client, x64 Host and x86 Host with MSVC 14.29, actual /O2 and NDEBUG verified from compile receipts. Architecture, embedded matching build ID and x86 Host LARGEADDRESSAWARE were checked before packaging. DLL address availability follows the game EXE. No real GPU/game validation was performed.

Correctness: both policy-test architectures, malformed records, protocol mismatch, full-pool bounded timeout, aborted command, missing reply and sticky failure; 36 alternating C1/C32 cross-process pairs covering x86→x86/x64, scalar/mixed uploads, retained pointers, pin-credit release, reverse synchronous replies, resource identifier turnover and four-thread serialization; eight existing native regressions; 39 Python checks. Final Linux policy tests also passed UndefinedBehaviorSanitizer. Simulated turnover is not a real map-load or D3D9 resource lifecycle test.

These timings measure the production serializer/transport extracted into a synthetic fixture, with a periodically slowed Host and small 12,000-command samples. They exclude real DXVK/backend cost and most D3D9 API work. They are not the earlier independent A/B/C benchmark, nor a comparison against release/PR6 binaries. Threaded Client CPU measurements are omitted because the fixture measures the main thread rather than worker CPU; raw records retain them. Windows CI hardware varies. No stable pure-small-command C32 advantage is established; report all measurements, including regressions. Do not infer FPS gains.

### Run 38094727377

36/36 cross-process pairs passed. Three alternating repetitions per configuration.

| Host | Workload | Role | C1 cycles/command median [min,max] | C32 cycles/command median [min,max] | Median change |
|---|---|---|---|---|---|
| x32 | stream | client | 1077.8 [1056.2, 1224.7] | 1163.5 [1087.8, 1244.8] | +8.0% |
| x32 | stream | host | 1118.7 [1045.0, 1134.7] | 1107.8 [1028.4, 1184.6] | -1.0% |
| x32 | mixed | client | 1360.3 [1319.4, 1362.7] | 1257.8 [1253.3, 1300.5] | -7.5% |
| x32 | mixed | host | 2369.4 [2324.9, 2395.1] | 1914.6 [1889.0, 1991.7] | -19.2% |
| x64 | stream | client | 1125.3 [990.5, 1272.5] | 1180.6 [1098.2, 1219.8] | +4.9% |
| x64 | stream | host | 1409.7 [1175.2, 1731.3] | 1312.3 [1231.9, 1339.6] | -6.9% |
| x64 | mixed | client | 1332.8 [1319.0, 1337.5] | 1267.9 [1249.5, 1341.9] | -4.9% |
| x64 | mixed | host | 2105.7 [2076.4, 2201.5] | 2060.1 [2033.0, 2306.6] | -2.2% |

### Run 38102076636

36/36 cross-process pairs passed. Three alternating repetitions per configuration.

| Host | Workload | Role | C1 cycles/command median [min,max] | C32 cycles/command median [min,max] | Median change |
|---|---|---|---|---|---|
| x32 | stream | client | 570.3 [549.4, 599.4] | 613.1 [558.2, 642.8] | +7.5% |
| x32 | stream | host | 583.6 [498.3, 614.9] | 574.6 [532.0, 596.6] | -1.5% |
| x32 | mixed | client | 774.1 [743.5, 1000.7] | 686.3 [672.7, 742.2] | -11.3% |
| x32 | mixed | host | 1366.3 [1350.0, 1575.9] | 1071.0 [1033.5, 1113.0] | -21.6% |
| x64 | stream | client | 528.1 [525.8, 560.3] | 594.4 [562.6, 622.2] | +12.6% |
| x64 | stream | host | 739.1 [524.1, 818.7] | 753.6 [697.1, 759.3] | +2.0% |
| x64 | mixed | client | 773.1 [748.3, 789.5] | 717.5 [713.2, 729.7] | -7.2% |
| x64 | mixed | host | 1278.0 [1247.3, 1308.8] | 1241.6 [1177.3, 1247.8] | -2.8% |

## Download and reproduction

Compiled source: `77b039ff1ca529c7605265b2062d0d5f3a74209d` on `codex/ipc-c32-development`.

Build: `ldb-ipc-c32-dev+7e0ea4bbf6f7c4ea`. ZIP SHA-256: `6195fca7730a727bd6ce2be7b224fe25b9ac3f84d0b934a8ded130fcc24fec7a`.

[Immutable ZIP download](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/f6c1ab3c00f1855be0bfbd6bbd4249176713a5b8/l4d2-bridge-ipc-c32-dev-7e0ea4bbf6f7c4ea.zip). Existing download-branch ZIPs were preserved; main, PR6, VERSION, tags and releases were not changed.

Installation, complete rollback and C1/C32 environment instructions: [IPC-C32-DEVELOPMENT.md](IPC-C32-DEVELOPMENT.md). This is a matched three-file update for an existing installation, not a first-install distribution. Test consecutive map changes in both directions, repeated loads, heavy uploads, idle, alt-tab, Reset/device loss and identical routes/settings before considering merge. The lazy 1 ms check only runs when another API entry occurs.

Reproduce on Windows: prepare_bridge.py → prepare_ipc_c32.py → test_ipc_c32.ps1 → eight regression scripts and Python unittest discovery → build_ipc_c32.ps1. Workflow pins build tools and frozen PR6 fixture generators. Raw data and upload receipt are in `ipc-c32-evidence/`.
