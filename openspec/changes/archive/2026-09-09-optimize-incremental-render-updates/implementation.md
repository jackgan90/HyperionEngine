# Implementation and verification

Implemented on 2026-09-09 against `5895bd7f9409a752b868bc46c34e3e89c9f7fca3`. The complete measured report is [docs/IncrementalRenderUpdates.md](../../../../docs/IncrementalRenderUpdates.md); raw local evidence is [out/incremental-render-updates-20260909](../../../../out/incremental-render-updates-20260909). The initial delivery was retained uncommitted for review. The subsequent user instruction on 2026-09-09 authorizes specification sync, archival and Git commit.

## Delivered behavior

| Requirement | Implementation | Verification |
|---|---|---|
| Shared engine changes preserve local preparation | `FMaterialSharedParameters`, immutable group identity, effective-value access in batch/instance/draw consumers; conservative mixed/default/resource/override fallback | `material_rendering`: moving/multiple views, shared identities, fallback/default and view-resource transitions; both overlay representations in `instance_batching`; final fixed-visibility trace has no ordinary refresh or full evaluation and only 80 B View writes per moving frame |
| Retain independent scene and batch data | Stable opt-in collection metadata, owning item handles with deep snapshot copies, complete culling/ordering; bounded view history across independent families | `CheckIndependentFamilies`, queued and retained frames, custom collection/transparent ordering, receipt changes; 11,200 A/B sample pairs with identical draw/item counts |
| Reuse independent immutable instance storage | Complete canonical record contracts, per-item packed records, ordered immutable block reuse across compatible views/passes, per-block GPU slices; skip unused single-instance binds | Record updates/visibility reuse, compatible view GPU slice reuse, one-object update packs one record/assembles Object block and retains Surface; old-frame pixels, layout/capacity/byte pressure, ownership and failed-Present retirement tests |
| Preserve measurement and rendering integrity | Frozen baseline/candidate runtimes, serial alternating A/B, separate trace, complete source checks and independent review | Debug/Release static/small/large/fixed/visible-UI matrix; 12 exact RGBA image pairs; 4 sustained camera runs; visible PID-owned windows; validation errors 0 |

No native command-stream or shader ABI redesign was introduced. Final native draw recording measured about 0.238–0.250 ms in the diagnostic CSM traces. The remaining material checks, plan publication, receipts and contiguous block assembly are explicitly retained costs.

Public C++ representations changed with their repository callers: `FRenderSceneSnapshot::Items` is `FRenderItemList`, block bytes are shared immutable storage, and batch signatures reference complete immutable structure/value lists. Custom primitive collection remains a `std::vector<FRenderItem>` emission contract. No authored asset, serialized key, existing CLI flag, plugin ID or build-target migration is required.

## Performance outcome

Each version/workload uses 200 warmup + 400 sampled frames, twice in alternating order, with 800 pooled samples. Debug retains `/Od /Ob0 /RTC1`; Tracy is off for the A/B. CSM, D3D12 validation, readiness and source workload are preserved.

| CPU preparation mean, ms | Baseline | Candidate | Reduction |
|---|---:|---:|---:|
| Debug, CSM small motion | 21.177 | 13.599 | 35.8% |
| Debug, CSM large motion | 33.132 | 22.330 | 32.6% |
| Release, CSM small motion | 1.736 | 1.094 | 37.0% |
| Release, CSM large motion | 3.031 | 1.913 | 36.9% |

**The initial Debug targets of 60% small-motion and 50% large-motion reduction were not reached.** They were engineering targets, not replaced by smaller thresholds. Static Release preparation increased by about 0.004–0.037 ms; Release fixed-visibility moving whole-frame mean and some CSM whole-frame tail percentiles regressed. The report includes every workload and the original distributions. The delivered improvement is in moving-camera CPU preparation; there is no claim that every frame metric improved.

Main plus shadow instance uploads fall from 1330.80 to 66.72 B/frame for small motion and 35709.84 to 1793.40 B/frame for large motion. Fixed-visibility motion retains 0 instance upload and 80 B/frame View writes. Immutable CBV blocks still require contiguous assembly when membership changes; per-element GPU updates or a visible-index ABI are not delivered.

## Changes driven by evidence

The [design](design.md#implementation-evidence-and-adjustments) preserves intermediate measurements and rejected assumptions. Shared/local separation plus metadata caching alone gave only a 15.5% provisional alternating reduction; expensive full item movement led to owning handles and a contiguous reuse index. Large-motion planning remained dominant, leading to immutable structural signatures and shared value-list reuse after complete compatibility checks. The final large-motion trace still spends 6.895 ms in planning and 3.452 ms in draw preparation. No unsupported claim of eliminating O(N) work is made.

An asynchronous readiness failure during an intermediate Showcase run was diagnosed and fixed: cached geometry metadata can precede a material program, so packing re-queries an unavailable cached program and rejects a still-missing program before use. The failed run is excluded from performance samples and its debugger evidence retained. Final tests and image comparisons include the corrected source.

The total instance byte budget now covers chunk plus record/block history. The plan-retirement regression uses a larger byte allowance to isolate its unchanged sixteen-item plan budget; a separate 64 KiB pressure test retains aggregate byte enforcement. Independent view retention accounts for one additional View constant block in the object-retirement test without allowing growth over repeated changes.

## Completed validation

- Debug, Release and Profile builds passed. The additional Debug Tracy build is diagnostic only.
- Full Debug CTest: **48/48** passed. Full Release CTest: **42/43** initially passed; `scene_viewer_acceptance` failed before launching Viewer because Python attempted `relpath` from a sandbox C: working-directory alias to F: assets. The identical test and Release binary passed with an explicit F: working directory, validating 514 instances, all culling-mode images, GUI and isolated failure. All 43 Release test cases therefore have passing executions; the first full run is accurately retained as a failure in its log.
- Profile `profiling_*`: **3/3** passed, including actual Tracy capture/reconnect and category/GPU behavior.
- `CheckStyle.py`, `CheckStyle.py --naming --build-dir out/build/debug`, `CheckBoundaries.py` and `git diff --check` passed. Naming covers 187 translation units; dependency boundaries cover 291 sources and 25 modules.
- All 28 A/B pairs have identical per-frame main/shadow item/draw/instance/failure/payload counts across 11,200 paired samples. Exact source-index exclusivity is additionally tested by instance strategy coverage tests; aggregate counters alone are not represented as that proof.
- Debug/Release × Forward/CSM × static/small/large: **12 image pairs with identical RGBA bytes**. Four 1800-frame sustained movement runs pass, including 40 post-warmup cycles each and stable tail native creation counters. Both actual visible Viewer windows are verified by PID, visibility and client size, with screenshots inspected for readiness, enabled validation, 4 × 2048 CSM and instance batching.
- A context-isolated reviewer checked all changed/new candidate sources and semantic/lifetime/cache boundaries, independently checked candidate hashes and diff whitespace, and reported no confirmed introduced functional defects. The reviewer did not independently run GPU tests. Existing material/instance docs and this final evidence were synchronized after the frozen source review.
- A bounded follow-up review cross-checked the final report against raw A/B/trace/delivery data and test logs. It found no numerical mismatch or performance overstatement. Two P3 documentation findings were corrected: temporary parameter composition also occurs on record misses/conservative fallback, and record/block caching predated the later stage 6/7 signature optimization. Both corrections were verified in the final text and require no source change or test rerun.

Evidence files: `DebugCTest.log`, `ReleaseCTest.log`, `ReleaseSceneAcceptanceRetry.log`, `ProfileCTest.log`, `Style.log`, `Naming.log`, `Boundaries.log`, `FinalAggregate.json`, `FinalTraceSummary.json`, `delivery/Summary.json`, `ReviewCandidate.json` and `FinalCandidate.json`. Initial OpenSpec validation is recorded in `OpenSpecValidate.log`. Archive-wide validation is recorded separately in `ArchiveValidate.log`.

## Archive and commit follow-up

On 2026-09-09 the user requested this change be archived and committed. The CLI synchronized all four requirements into `openspec/specs/incremental-render-updates/spec.md` and archived the completed change in this directory. The one-time initial no-commit instruction was removed from the permanent specification after the user authorized this follow-up; renderer requirements and measurements are unchanged. Relative report/evidence links and delivery status were updated.

Before staging, all 54 reviewed source/tool files still match the frozen delivery hashes, so prior builds, regression suites and image/performance validation apply to the same source. The additional validation covers specification sync, all OpenSpec specs, documentation links, exact candidate/staged file sets and whitespace. `ArchiveCommitCandidate.json` records the final file hashes; generated evidence under `out` remains outside the commit.
