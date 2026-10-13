# GPLALL MSAA loop hint modification

This is an explicitly modified GPLALL/DXVK shader, prepared by NPCodex on 2026-10-13. Original authorship and the original zlib/libpng license in `LICENSE` are retained. The GPLALL project name does not replace the actual license text.

Upstream source: <https://github.com/Digger1955/dxvk-gplall/blob/6a0ea561f9add008899680e6c313aa21c151e03e/src/dxvk/shaders/dxvk_blit_frag_2d_ms.frag>, tag `DXVK-GPLALL-2.6.8-2`. `original.frag` is the original source. `modified.frag` and `shader.patch` mark the dated change: only the second MSAA linear/nearest sample loop switches from `[[unroll]]` to `[[dont_unroll]]`. The average-resolve loop remains unchanged. No MSAA samples, texel fetches, alpha weighting, threshold, fallback, coordinates or source arithmetic are removed or reordered.

`config/backend.json` pins the original release archive and both original DLL hashes, every source/shader artifact, the unique embedded shader offset, and both modified DLL hashes. `scripts/backend_recipe.py` verifies the complete source transformation and that the two SPIR-V files differ only at the second `OpLoopMerge` control word. It replaces that shader in a separate copy of each verified original DLL. All other DLL bytes remain unchanged; this is not a complete backend rebuild.

The following CPU-only commands reproduce both SPIR-V files using `glslangValidator` 16.5.0 (generator version 11), available in the already verified portable RenderDoc 1.46 `plugins/spirv` directory. No compiler download, driver installation or GPU run is required. Run from this directory and write output outside it if preserving a clean checkout:

```powershell
glslangValidator --target-env vulkan1.3 -V -S frag -o original-recompiled.spv original.frag
glslangValidator --target-env vulkan1.3 -V -S frag -o modified-recompiled.spv modified.frag
Get-FileHash -Algorithm SHA256 original-recompiled.spv, modified-recompiled.spv
```

Expected SHA256:

- Original: `e879efae72d147585afa2814ad7e5a4e04d0543d1a39f37b7e2bab212cb17a37`.
- Modified: `4a99ba7d328be8a1bed42dd761a08a6edabfda788cb523414a036a728c9dce18`.

The original compilation is byte-identical to the shader extracted from the pinned backend. The modified compilation is byte-identical to the checked-in replacement; byte offset 4712 (word 1178) changes from 1 to 2. Altering a compiler hint can change driver scheduling and floating-point rounding despite unchanged source arithmetic. The captured-frame comparison found 26 intermediate half-float components differing by one ULP, while the final presented frame was identical. The user explicitly accepted that disclosed difference for this version; the historical strict comparison failure remains recorded. This recipe alone makes no FPS, universal image-equivalence or long-term stability claim.

Player packages retain the existing two backend DLL paths and merge this modification notice and original licenses into `THIRD-PARTY-NOTICES.txt`; source, SPIR-V, recipes and diagnostic receipts are build/repository materials, not additional player-package files.
