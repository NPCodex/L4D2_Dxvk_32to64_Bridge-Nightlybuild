# DXVK-GPLALL bridge variant

This variant retains the project's x86 client, x64 Host, protocol and texture shadow cache. The separately loaded D3D9 backend is built from the pinned DXVK-GPLALL 2.6.8-2 source with the reviewed CLIP, WORKERS and SPEC repairs plus the MSAA `DontUnroll` shader change. The generic SSE2 build avoids requiring an AMD/Intel-specific CPU build. Do not use the release's Native Linux libraries or its x32 DLL in the Host.

Release: https://github.com/Digger1955/dxvk-gplall/releases/tag/DXVK-GPLALL-2.6.8-2

The GPLALL source repository, commit, candidate patch hashes and original release provenance are pinned in `config/backend.json`. Run `./scripts/build_bridge.ps1` to fetch that exact source, apply all four reviewed backend changes and build the x86/x64 D3D9 DLLs with the selected MSVC toolset. `BACKEND-SOURCE.json` is required during packaging. An explicit `-DxvkDll` remains available for other backends; their identity is not claimed to be GPLALL.

For an existing compatible bridge installation, close the game and Host, back up `bin/.l4d2bridge/d3d9vk_x64.dll`, then copy the backend-update package's `bin` contents into the game's `bin`. This historical backend-only package contains no client or Host. Keep the Bridge client (currently `bin/d3d9.dll`, optionally `bin/dxvk_d3d9.dll` with `-vulkan`) and `bridge.conf`.

The package does not install the release's large example `dxvk.conf` or force async, frame pacing, vendor hiding, LOD bias or a display-specific FPS limit. Existing game configuration still applies; move it aside for a baseline test if needed. Confirm the new Host log reports DXVK-GPLALL and x86_64, then test campaign rendering, normal Mods, repeated map changes, alt-tab and clean shutdown. Compare warmed-up frame times with 2.6.1 under identical settings. No performance increase or game validation is claimed until measured.

The historic Intel Arc B580 / DXVK 2.6.1 validation remains evidence for that original combination only. This variant requires a fresh game test. Keep the backend license and source/release attribution when distributing.
