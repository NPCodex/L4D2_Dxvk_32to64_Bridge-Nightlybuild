# Performance Gate Report

Historical decision: after reviewing this report, the maintainer explicitly authorized merging upstream f6a059b with the 45-item candidate and publishing a new version with the v1.1.10 rollback warning. That adoption supersedes the publication block below; these measurements and the failed performance gate remain unchanged. See docs/UPSTREAM-45-INTEGRATION.md for the new scope.

- Candidate: v1.4.4 (84aff8bc678052f00bac7a962023e2dd59a466e0)
- Baseline: v1.3.0 package d41eb95affa86af9589c7f9f40c6279a7f003a5fbba47d5f1e962d456580d5e6
- Candidate package SHA256: ca26dd588da4a563c156bbeb0e9283cde8d4b8908a2901fc9997886103a0d348
- Evidence: six runs with twelve valid 60-second captures, three runs per variant and scene, fixed package/configuration, no reported ETW loss or malformed/inconsistent CSV rows. The known PresentMon process-query privilege warning is retained; exact launched Host PID/application still match.
- Measurement: Host Present submission intervals, not displayed FPS or input latency. Metrics are medians of per-run results; frames are not pooled.
- CI 38236569391 passed for the exact tested commit. Later documentation-only commits and VERSION increments are not separately built or game-tested candidates.

## Result

The candidate fails the release performance gate and is not eligible for merge or publication. Median changes versus v1.3.0:

| Scene | Average FPS | 1% low | 0.1% low | p99 frame time | Max frame time |
|---|---:|---:|---:|---:|---:|
| Flashlight off | -1.969% | -0.778% | -6.856% | +1.206% | +9.465% |
| Flashlight on | -2.586% | -0.719% | -5.763% | +3.932% | +2.014% |

The release block rests on average FPS declining in all three pairs in both scenes (off -1.55% to -2.21%; on -2.36% to -3.00%), with non-overlapping per-variant average-FPS ranges. The 1% low medians are slightly lower, but individual pairs move in both directions. The 0.1% low, p99 and maximum intervals are supporting data, not an independent veto based on 0.1% low. All twelve captures contain zero intervals >=16.667 ms (also zero >=33.333 ms), so these results do not establish increased visible long stutters. The failed candidate cannot be published by relabeling it optional, experimental, or test-only.

Four completed smoke groups (baseline/candidate, x64/x86 Host) each passed four map changes, two Reset operations and clean Client/Host exit. These short tests do not establish visual correctness, interactive-menu compatibility or long-term stability. The 30-minute candidate stability run was not performed after the performance gate failed.

The isolated clone was restored: `clone-after-20261012T123900703390Z/restore.json` records 162 restored snapshot paths and equality of the original E: player-installation inventory before/after. This check covers the recorded package paths, not a hash of the entire player installation. No game/Host/PresentMon processes remained at the monitoring check.

## Scope decision

The candidate contains multiple functional groups (upload/retention optimizations, IPC close/retry fixes, menu/language changes, keyboard protection, and related plumbing). The current evidence does not isolate a no-regression subset. Until an independently rebuilt subset passes the same gate, the official baseline remains v1.3.0. Do not claim full 45-item adoption.

The failed candidate is retained for diagnosis only. Existing authorization for isolated-copy testing remains, but an unchanged heartbeat must not repeat this candidate's game tests, rebuild or publish it. Any new subset needs a distinct source/package identity and fresh acceptance evidence. The user's original installation stays read-only.
