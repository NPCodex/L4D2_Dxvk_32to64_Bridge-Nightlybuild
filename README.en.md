# L4D2 Bridge Nightly

> **This build includes DXVK (GPLALL), L4N and the Bridge tools in one package. Extract and copy to install; no separate component downloads are needed. Do not mix it with other similar projects or bundles.**
>
> **Remove the `-vulkan` launch option and back up/move away `d3d9.dll` from the game root; keep this package's `bin/d3d9.dll`.** L4N / Left4Neko is the work of **Starfelll (@Starfelll)**, with thanks to its original author.

[简体中文](README.md) | **English**

## Download and installation — start here

**v1.5.4 adopts the IPC immediate-publication deadline change and the GPLALL MSAA shader loop-hint change.** GPU intermediate outputs have disclosed 1-half-ULP differences and fail the original byte-exact gate, although the captured final frame is identical. Replay does not demonstrate a speedup, and IPC comparisons do not establish a stable gain. This combination has no completed gameplay-performance or all-scene image-quality validation. See [implementation and evidence](docs/GPU-IPC-20261013.md). Defaults remain x64, C8 and mapped shadow off. **If 1% low drops, frames drop or frame rates become unstable, return to [v1.1.10](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases/tag/v1.1.10-nightly-20261009-3a75b814-r44c3dfa4ec5c-b38052959126.1).** Exit the game and all Hosts, restore the Client and both Hosts together, and restore that version's two GPLALL backends to undo this GPU change.

**[Download the full package from Releases](https://github.com/NPCodex/L4D2_Dxvk_32to64_Bridge-Nightlybuild/releases)**. Each release has one `l4d2-bridge-*.zip` and its `.sha256` checksum. It includes **L4N 2.51.0, DXVK-GPLALL, Bridge, `dxvk.conf`, L4N's `left4dead2/neko/config.vdf`**, and the repaired `bin/studiorender.dll`. No patch tool or Python installation is needed. The layout follows the maintainer's supplied bundle; `config_template.vdf` is omitted. See [bundle provenance](docs/L4N-BUNDLE.md). The player ZIP contains runtime components, active configuration, L4N's original VDF templates and QC/VMT examples, a short `README.txt`, and one combined `THIRD-PARTY-NOTICES.txt`. Developer documentation, JSON manifests, diagnostic scripts, the SDK and offline Mod authoring tools remain in the repository.

1. **Exit the game and Bridge Host, then back up existing files.** Save the original `left4dead2.exe`, L4N files, client, Host, `dxvk.conf`, `left4dead2/neko/config.vdf`, Bridge configuration and `bin/studiorender.dll`. Extract the ZIP into a temporary directory. If ThinFlex is already installed, keep the initial original-DLL backup. If another similar project is installed, uninstall it or restore its original files before installing this package.
2. **Preserve configuration before upgrading:** remove any configuration you want to retain (`dxvk.conf`, `left4dead2/neko/config.vdf`, `bin/.l4d2bridge/bridge.conf`) from the temporary extraction directory before copying. If you customized your backend, also remove the packaged `bin/.l4d2bridge/d3d9vk_x64.dll` and `d3d9vk_x86.dll` there. Skip this step on first installation. Back up personal changes to L4N shaders and other files with matching paths, as those will also be replaced.
3. **Check the installed engine DLL:** run `Get-FileHash 'your-game-directory\bin\studiorender.dll' -Algorithm SHA256` in PowerShell and compare with the table below. The supported original can be replaced. If the repair is already installed, retain it and the original backup. **For any other hash, remove `bin/studiorender.dll` from the temporary extraction directory and update Bridge only.**
4. **Copy the prepared files:** remove `-vulkan` and back up/move away any game-root `d3d9.dll`. Merge the prepared files into the game root beside `left4dead2.exe`, installing L4N, DXVK and the matching Client/Hosts together. Replacing the matching `studiorender.dll` applies ThinFlex directly. **Keep `bin/d3d9.dll`**, the Bridge client; the matching x86/x64 Hosts, backends and configuration stay in `bin/.l4d2bridge`. Follow this layout instead of the ordinary DXVK instructions in the original L4N readme.

| Installed `bin/studiorender.dll` | SHA-256 |
|---|---|
| Supported original | `3f5f5b0f539e8ad22bcfc4381be41571257c0c29e8061057682f9b8525ca7b85` |
| Packaged repair | `03964dedcf8b7f4ebde24cd3d0738873d37c075a7a9b313dad001bb937f9d1b6` |

The user has reported a successful ThinFlex retest, limited to this exact version. Recheck after game updates: copying files does not automatically check the installed version. The repository [engine manifest](runtime/engine/studiorender.manifest.json) records the changes; the packaged `THIRD-PARTY-NOTICES.txt` preserves attribution. The modified DLL's original digital signature is invalid. See the [installation and restoration guide](docs/THINFLEX-TEST-README.txt).

To use `-vulkan`, rename the client to `dxvk_d3d9.dll` within the game `bin`. When switching to the default loading path, back up the old `bin/dxvk_d3d9.dll` and remove `-vulkan`. Both paths share `bin/.l4d2bridge`.

The full package includes both x64/x86 Hosts and matching GPLALL backends. **x64 remains the default.** To switch, set `client.testX86Server=False` for x64 or `True` for x86, retain `forceX64Server=True`, and restart the entire game.

L4N and the Bridge settings menu plugin are installed together. The plugin is already at `bin/neko/plugins/L4D2BridgePlugin.dll` and becomes available in the L4N menu when the game starts. Installing it does not change configuration or run GC; those actions require menu selections. To disable the menu, exit the game and move or delete that DLL; Bridge itself will still work. See the [L4N guide](docs/L4N-BRIDGE-CONTROLS.md). Original L4N documentation and author attribution are included in the combined `THIRD-PARTY-NOTICES.txt`; see also the [Starfelll notice](licenses/L4N-NOTICE.txt).

Fresh installs use upstream's learned-aggressive retention policy and disable routine memory/crash/data diagnostics by default. Preserve your installed configuration when upgrading. See [configuration settings and costs](docs/CONFIGURATION.md).

Roll back the Bridge client and Host together. Restore your original `studiorender.dll` backup to undo ThinFlex. To uninstall, remove the installed files and restore your backups.

See the [NVIDIA source update](docs/NVIDIA-UPSTREAM-UPDATES.md) and the earlier [v1.5.0 integration and validation scope](docs/UPSTREAM-45-INTEGRATION.md). The [historical v1.2.3 comparison](docs/HOT-COMMAND-PACKETS.md) records FPS with this machine's GPLALL + x64 Host configuration; the [earlier IPC comparison](docs/UPSTREAM-1.2.2-INTEGRATION.md) documents the tradeoff when adopting v1.2.2. The [flashlight investigation](docs/FLASHLIGHT-PERFORMANCE-2026-10-10.md) and [earlier paired runs](docs/PERFORMANCE-2026-10-10.md) concern previous versions; see also the [game validation guide](docs/GAME-VALIDATION.md).

Based on [NVIDIA dxvk-remix Bridge](https://github.com/NVIDIAGameWorks/dxvk-remix), this repository retains the patches from the [original L4D2 project](https://github.com/yeyunyyds/L4D2_Dxvk_32to64_Bridge) and automatically builds an x86 client and both x86/x64 Hosts for 32-bit Left 4 Dead 2.

## Differences from upstream

- Builds only Bridge; the RTX Remix renderer is neither built nor distributed.
- Applies L4D2 patches for Host and backend loading paths, surface and buffer shadow-memory management, diagnostic logging, and game-specific configuration. See [patches/l4d2-bridge.patch](patches/l4d2-bridge.patch).
- Uses **DXVK-GPLALL 2.6.8-2 x64** as the default backend. Its version and download checksum are pinned separately in [config/backend.json](config/backend.json); updating Bridge does not automatically update the backend. This package additionally applies the pinned MSAA loop-hint transformation; see the [validation scope](docs/GPU-IPC-20261013.md).
- Adds upstream monitoring, automated builds and tests, Nightly releases, and minimal runtime packaging.
- Fixes volume-texture byte pitches and upload offsets to prevent corrupted color-correction lookup tables. See [fix provenance and validation](docs/VOLUME-TEXTURE-COLOR-FIX.md).
- Fully integrates original-project **1.3.0 / `f6a059b`**, including IPC v2, Steam input/window/workshop adaptation, PageBlock retention/reclaim/recovery, manual GC, Reset state handling, diagnostic separation, both Hosts and L4N settings. Source, presets, tests and the optional DXVK memory experiment are retained; the experimental backend does not replace default GPLALL. See [integration tracking](docs/ORIGINAL-PROJECT-UPDATES.md).
- Extends creation-failure cleanup to volume/cube textures, vertex/index buffers and standalone surfaces, releasing client wrappers and clearing outputs. Response timeouts retain ordered server cleanup.
- Hardens buffer-lock bounds and volume temporary ownership, and optimizes contiguous uploads and queue reads. See [validation methodology](docs/RUNTIME-RELIABILITY.md).

`bin/d3d9.dll` is the 32-bit Bridge client. `.l4d2bridge/d3d9vk_x64.dll` and `d3d9vk_x86.dll` serve the matching Host architectures.

## Automatic and manual builds

The workflow checks the NVIDIA build source default branch (currently `main`) every hour at minute 23. Codex reviews new commits hourly; future production updates are reported for the maintainer's decision. Actions builds only the approved commit in `config/nvidia-upstream.json`, skipping an already published source/recipe pair. GitHub scheduling may be delayed.

Under **Actions → Build latest upstream Bridge → Run workflow**:

- Leave `upstream_commit` empty to observe HEAD and build the approved commit, or enter a full 40-character SHA for an explicitly authorized override.
- Enable `force_rebuild` to create a new independent build without replacing existing assets. This option is disabled by default.
- Enable `validation_only` to build, test and run A/B benchmarks, saving artifacts and skipping publication.
- Enable `thinflex_test` to label a full upstream integration + ThinFlex test release (Pre-release, not Latest). Every build includes the fixed ThinFlex engine DLL in one full package; check the installed game version before copying it.

Deduplication checks both upstream SHA and a fingerprint of build/package/test inputs. Versions use `nightly-YYYYMMDD-upstreamSHA-rRecipeDigest-bRunID.Attempt`. Dates use upstream commit UTC time. Rebuilds and reruns have separate versions, preserving old assets. Release notes record full identities. JSON manifests remain build-validation inputs and are excluded from the player ZIP. Patch, compile or test failures prevent publication.

When reusing a local source checkout, the build script verifies the complete patch and index, rejecting additional source changes while preserving the checkout. Rerunning a failed publish job searches paginated release listings for the unpublished draft, verifies existing assets and uploads only missing files. Conflicting assets stop publication; existing files are never overwritten.

## License

- Project-specific additions and modifications: **MIT**, see [LICENSE](LICENSE). The original copyright notice for `yeyunyyds` is retained.
- NVIDIA Bridge: **MIT**, see [licenses/Bridge-MIT.txt](licenses/Bridge-MIT.txt).
- L4N / Left4Neko 2.51.0: original author **Starfelll (@Starfelll)**. Original files and documentation retain their attribution and are outside this project's root MIT license; see [L4N-NOTICE](licenses/L4N-NOTICE.txt).
- DXVK / DXVK-GPLALL: distributed with the **zlib/libpng** license; see [licenses/DXVK-LICENSE.txt](licenses/DXVK-LICENSE.txt) and [licenses/DXVK-GPLALL-LICENSE.txt](licenses/DXVK-GPLALL-LICENSE.txt).
- Dependencies included in Bridge, such as Detours and Tracy, retain their respective licenses. See [licenses/Bridge-third-party.txt](licenses/Bridge-third-party.txt).
- The modified `studiorender.dll` comes from the maintainer's matching local game file, with the limited ThinFlex repair applied at the user's request. The original engine belongs to Valve and is outside the root MIT license; see [engine attribution](licenses/Valve-engine-NOTICE.txt).

The root MIT license does not replace third-party licenses. Release packages retain copyright and license notices. See [THIRD_PARTY.md](THIRD_PARTY.md) for full attribution. The Left 4 Dead 2 game itself is outside the scope of this project's license.
