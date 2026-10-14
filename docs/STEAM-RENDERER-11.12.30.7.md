# Steam Renderer 11.12.30.7 compatibility test

Based on upstream main `20ef9df71037125f7a5546f8179bd5b902b3c510` / LDB 1.3.1.
The supplied configuration enables the existing x64 Presenter, input and
Workshop/invitation availability proxy. Shift+Tab works, but Workshop and
invitation entries report unavailable. The newly supplied Renderer DLL has
a different full SHA-256 from the single sample supported by 1.3.1. A Host
loading that new DLL deterministically rejects its availability reader with
`renderer-file-hash-mismatch`. Runtime loaded-file identity still requires logs.

## Reviewed x64 sample

- File version: 11.12.30.7; bytes: 1,679,512; machine: PE32+ / 0x8664.
- SHA-256: `cbde033f76ac070ad8b7d25328e8acfbbabae6e40487f40dfcf6203b6c932c0b`.
- Image size: 0x1d0000; exported IsOverlayEnabled RVA: 0xb8a90.
- Getter shape: `movzx eax, byte ptr [rip + displacement]; ret`.
- Eight-byte getter: `0f b6 05 52 d8 0d 00 c3`; data byte RVA: 0x1962e9.
- The data byte is within .data and the loaded-image bounds. These are static
  observations; the DLL was not loaded/executed, modified or redistributed.

The x86 sample is also version 11.12.30.7, bytes 1,443,480, SHA-256
`cd5059973609f64595cbff4e18970a3fd7b45a2e5ff113b59ee86ba8f462d8a1`.
It is recorded for identification, not accepted by the existing x64-only
availability reader. The x86 Host retains its explicit unsupported boundary.

## Change and cost boundary

Accept both reviewed x64 full-file hashes, keeping the original 10.96.30.42
sample supported. Hash the loaded file once at initial identification and match
the completed digest against the small allowlist. Retain getter-byte decoding,
image bounds, unknown-version refusal, GameUI verification, real Host state,
three-second freshness and process-liveness checks. Do not force availability.

Only `steam_availability_diagnostics.h` and `diagnostic_file_hash.h` change in
production. The legacy single-hash API delegates to the same implementation;
GameUI hashing has the same result and LastError behavior. No new periodic hash,
scan, worker, IPC message, shadow behavior, window/input policy or configuration
is introduced. Existing status polling remains unchanged.

## Validation and installation

Extend the existing x86/x64 native availability test with both getter layouts,
ASLR/bounds/malformed-code refusal, known/unknown digest policy, actual Windows
hashing of a test-owned file, one-pass reads, LastError and disabled-work checks.
Run the complete existing Windows build/regression workflow. CI does not load
the supplied Steam DLL and is not game acceptance or an FPS measurement.

For the matched update package, exit L4D2 and both Hosts, back up and replace
`bin/dxvk_d3d9.dll`, `bin/.l4d2bridge/L4D2Bridge32.exe` and
`bin/.l4d2bridge/L4D2Bridge64.exe` together. Preserve the existing DXVK,
bridge.conf, ReShade, retention DB and L4N plugin. Restore all three backups
to roll back. Test using x64 Host: Shift+Tab, Workshop, invitation opening,
interaction, closing and focus restoration. Return both Bridge logs if either
menu still fails; do not enable full API/memory tracing for this test.

This branch is an experimental build based on 1.3.1, not a new Release/Tag.
main and published assets are not modified.
