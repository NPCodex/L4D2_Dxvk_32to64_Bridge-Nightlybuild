# IPC C2/C4/C8/C16 experimental update

Default remains **C1**. Supported batch sizes are **1, 2, 4, 8, 16, 32**. The user reports previous C1/C32 game performance and stability passed; the new intermediate configurations still require game comparison. Their optimal FPS/latency tradeoff has not been established.

Edit `bin/.l4d2bridge/bridge.conf` and restart the game and both Hosts:

```ini
client.ipcBatchSize = 4
```

Other unsupported values fall back to 1. `LDB_IPC_BATCH` is not used. Both Client channels log the actual selected batch during startup. Merge only this setting into your existing config; the ZIP's config is a reference. Back up/replace all three bridge binaries together using [installation and rollback steps](IPC-C32-DEVELOPMENT.md).

Batch selection is validated once at initialization; the existing per-command pending-count comparison is retained. Full batches, full message blocks, synchronous waits, external payloads, Present/Reset and non-batchable lifecycle operations retain their existing publication behavior. The existing 1 ms API-entry lazy timeout and Surface complete-layout preflight are unchanged. No thread, mapping, queue format, resource lifetime or backend change is introduced. The normalized source build ID nevertheless changes, so matched binaries are required.

[Windows run 38127232367](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/actions/runs/38127232367) passed 252 cross-process pairs (42 per gear, each covering x86 Host and x64 Host, Device and Module, 7 workloads, 3 alternating repetitions), 30 fail-closed cases (5 per gear), both architecture policy tests, 8 existing native regressions and 39 Python checks. Policy tests cover exact batch boundaries, partial-tail semantic flushes, default/invalid values and explicit deadline flush. Cross-process tests cover lazy API-entry flush, RPC, threaded order, retained pointers, Surface upload wrapping and full-ring layout. Native fixtures inject batch configuration into isolated channels; they do not replace game/GPU validation.

Raw results: [38127232367-raw.json](ipc-c32-evidence/38127232367-raw.json), [38127232367-faults.json](ipc-c32-evidence/38127232367-faults.json). All repetitions are preserved; no universal best gear or FPS improvement is claimed. The small Surface control workload forces publication in every gear and should not be used to rank small scalar batch performance.

Compiled source: `d59493c5c7cdc8ca6a7853f7bf745428df09ffe6`. Actual Windows MSVC 14.29 Release compilation produced the x86 Client, x64 Host and x86 Host. Binary hashes, embedded IDs, architecture, x86 Host LAA and ZIP SHA-256 were verified; the uploaded immutable ZIP was downloaded again and compared.

[Download matched update ZIP](https://raw.githubusercontent.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge/eb3b2abcbfe66d1490a607bf1de1943dc8cbd723/l4d2-bridge-ipc-c32-dev-ec19f9c2780fa8c1.zip). Build: `ldb-ipc-c32-dev+ec19f9c2780fa8c1`. ZIP SHA-256: `4ebcc0f6a2bbd1797f6e7b58cedd34676854b456dd0173755e5b23a71eb4bf3e`.

main, VERSION, formal tags/releases and the earlier pre-release are unchanged. This update is appended to the fork download branch, preserving existing ZIPs. Status for the new gears: **AWAITING GAME VALIDATION**.
