# Performance Gate Report

- Candidate: v1.4.4 (84aff8bc678052f00bac7a962023e2dd59a466e0)
- Baseline: v1.3.0 package d41eb95affa86af9589c7f9f40c6279a7f003a5fbba47d5f1e962d456580d5e6
- Candidate package SHA256: ca26dd588da4a563c156bbeb0e9283cde8d4b8908a2901fc9997886103a0d348
- Evidence: six valid 60-second captures, three runs per variant and scene, fixed package/configuration, no CSV loss or malformed rows.

## Result

The candidate fails the release performance gate and is not eligible for merge or publication. Median changes versus v1.3.0:

| Scene | Average FPS | 1% low | 0.1% low | p99 frame time | Max frame time |
|---|---:|---:|---:|---:|---:|
| Flashlight off | -1.969% | -0.778% | -6.856% | +1.206% | +9.465% |
| Flashlight on | -2.586% | -0.719% | -5.763% | +3.932% | +2.014% |

The regression is material in average FPS and tail-frame behavior. It cannot be waived by labeling the build optional, experimental, or test-only.

## Scope decision

The candidate contains multiple functional groups (upload/retention optimizations, IPC close/retry fixes, menu/language changes, keyboard protection, and related plumbing). The current evidence does not isolate a no-regression subset. Until an independently rebuilt subset passes the same gate, the official baseline remains v1.3.0. Do not claim full 45-item adoption.

The failed candidate may be retained for diagnosis only. Do not start the game automatically and do not modify the user's original installation.
