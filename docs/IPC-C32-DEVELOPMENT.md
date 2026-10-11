# IPC C32 development bridge

Experimental Release build, awaiting L4D2 game validation. No formal VERSION, tag or release changes. Keep all three bridge binaries matched.

## Implementation

The PR6 correctness foundation is adapted, not overwritten. Both Device and Module Client→Host channels use ordered shared message blocks (64 × 4 KiB, about 266,624 bytes each). Small scalar-only commands directly encode up to 256 bytes in a leased record, including UID and fixed-width Header metadata. Records publish at the configured batch size or block capacity, whichever comes first. The Host acknowledges the block only after the final command dispatch and parameter access finish. Host→Client replies retain the PR6 reply queues.

Only the explicit asynchronous setter/draw whitelist can defer publication. Other commands force publication, including startup, creation/destruction, Present/PresentEx/SwapChain Present, Reset/ResetEx, EndScene, queries, Lock/Unlock, readback and termination. Before a synchronous reply wait, and before Data Ring backpressure, the Client flushes pending commands.

Blob commands and reserved Lock pointers retain PR6 Data Ring ownership, including read pins. They are descriptors in the same ordered stream and force publication. Scalar prefixes migrate once to that ring when a command requires a blob or exceeds the inline limit; large payload data does not migrate through a message block. No new upload pool, reply slots, API state folding or submission thread is added. This production hybrid is not the exact all-inline independent microbenchmark; its game performance must be measured separately.

API entry checks use the existing `LogFunctionCall` / `LogStaticFunctionCall` hooks. They avoid reading the clock when no batch is pending. A pending batch at least 1 ms old is flushed on the next entry, including local getters. Command construction also checks under the original serialization mutex for entry paths without the hook. **There is no hard 1 ms latency guarantee while the game makes no API calls.** This is an initial experimental threshold, not a measured optimum.

The adapter reads `client.ipcBatchSize` from `bridge.conf` once per Client channel; default 1 (C1). Supported sizes are 1, 2, 4, 8, 16 and 32 (C1/C2/C4/C8/C16/C32); unsupported values fall back to C1. The previous `LDB_IPC_BATCH` environment override is no longer used. C1 uses the same encoding, ownership and matched binaries with immediate publication. It does not restore the old protocol. Hold all other game/backend settings constant when comparing.

Protocol metadata contains a normalized source hash; the existing Host launch build-ID comparison also rejects unmatched binaries before dispatch. Data control uses an experimental version. Message-block full backpressure has a 2 s budget. UID-verified synchronous reply waits are capped at 10 s (or the smaller configured retry budget) and poison the transport on timeout. These are experimental limits, not performance guarantees. Transport waits and full-block backpressure fail closed; no timeout allows continuation of a partially encoded command. Added mappings cost approximately 0.51 MiB of Client virtual address space. Existing legacy command reservations remain allocated in this first adapter; removal is deferred pending evidence.

## Install and rollback

1. Exit L4D2 and both Bridge Hosts. Back up `bin/dxvk_d3d9.dll`, `bin/.l4d2bridge/L4D2Bridge64.exe` and `bin/.l4d2bridge/L4D2Bridge32.exe` together.
2. Verify the ZIP SHA-256, then merge its `bin` directory into your existing game root. The ZIP contains the three bridge binaries, documentation and a reference config under `config/`; only its `bin/` directory is installed directly. Preserve DXVK, ReShade, runtime configuration and retention databases. It is not a first-install package.
3. Confirm Client and Host logs show the same `ldb-ipc-c32-dev+…` build ID. Client startup lines `IPC_C32 … batch=1/2/4/8/16/32` confirm the configuration actually reached both Client channels. Start with your existing game launch options.
4. Set `client.ipcBatchSize = 1` (C1, default) or `32` (C32) in `bin/.l4d2bridge/bridge.conf`, then restart the game and Hosts. Merge this setting into your existing config; the ZIP includes a reference config, not an instruction to overwrite your other settings. Do not mix Client and Host builds.
5. To roll back, exit both processes and restore the entire backed-up three-file set.

## Gameplay validation requested

Test the same route and settings in C1/C32, alternating repeated runs. Record average FPS, 1% / 0.1% lows, frame-time distributions, map load time and CPU usage. Test test-map→MOD-map, reverse, repeated maps and several consecutive changes without exiting; include alt-tab, idle, Present/Reset/device-loss paths and heavy uploads. Report crashes, waits, missing rendering and matching build logs before interpreting performance.

Native simulated resource turnover and retained-pointer tests do not replace real D3D9/GPU gameplay testing. Completion of message blocks means Host CPU consumption, not GPU fence completion. Do not promote this build to a stable release without game validation.
