# Implementation and validation

Implemented and audited on 2026-09-14; all 21 tasks are complete. Archived on 2026-09-14 after independent review and the compute constant-page lifetime repair.

## Delivered behavior

- Compute has an independent shader/PSO/dispatch path. Renderer clients choose source, entry and compile options, publish named constants and sampled/storage resources, and schedule work through `FComputePassDesc` / `AddComputePass`. Published values are frozen. Graphics and compute share source caches, reflected layouts, resource bindings and constant packing/allocation.
- D3D12 supports R32F mip storage, structured/raw storage buffers, SRV/UAV views, compute bindings, per-mip transitions and UAV ordering on the existing direct queue. Float/byte readback is diagnostic only. Portable DXIL/SPIR-V/MSL compilation and reflection include compute groups and writable resources; runtime execution remains D3D12.
- Logical graph passes are recorded in bounded contiguous native batches. More than 16 passes retain independent timestamps, state and ownership. Invalid zero recording capacity is rejected before acquiring a frame. Failed recording/submission continues to use existing cleanup and fences.
- HZB is an independent requested producer. Compatible consumers share a product; no current consumer means no dispatch or active product. Products carry graph/source/view identities, camera/revision, viewport, dimensions and reduction metadata. Consumers validate these before using a product. Standard nearest=min and reversed nearest=max; farthest uses the opposite reduction. All NPOT edges are covered.
- Deferred orders BasePass, requested HZB, fullscreen R8 contact mask, then lighting. Only directional direct visibility uses `min(Contact, CSM)`, for ordinary and clustered Deferred. Forward and later compatibility/transparent receivers keep their existing behavior. CSM and contact switches are independent, subject to the light's shadow eligibility.
- DebugUI provides the live switch, ray length/thickness/bias/budget, mask/HZB preview and activation statistics. HZB preview is a separate consumer. Shipped defaults leave contact disabled.

Usage and limitations are documented in [Compute pipelines](../../../../docs/ComputePipelines.md) and [Contact shadows](../../../../docs/ContactShadows.md). The latter includes Unreal and Unity HDRP references for pipeline placement and Forward trade-offs.

## Automated validation

| Coverage | Result |
| --- | --- |
| Debug full CTest suite | 71 tests exercised. Initial run passed 70; the old recording-capacity expectation was updated for batching and passed on rerun. |
| Final Debug affected suite | 8/8: graph, Renderer compute, HZB, contact, Deferred, RHI contracts, compute RHI and frame failure recovery. |
| Release full CTest suite | 70/70 passed. |
| Final Release compute extensions | 2/2 passed after adding separate parameter/resource/shader replacement coverage. |
| Tracy profile build and execution | Renderer/Deferred and compute RHI built; 2/2 selected tests passed. No connected Tracy capture is claimed. |
| Style and naming | 535 owned source paths/formats, 330 translation units passed; the final two modified test units passed a further semantic naming check. |
| Module boundaries | 502 source files / 29 modules passed. |
| OpenSpec and diff | Strict change validation and `git diff --check` passed. |

The Debug count includes its additional memory-oriented coverage. No image thresholds or validation settings were weakened.

Specific checks include:

- Cold/cached compute compilation, defines, thread-group reflection, writable textures/structured/raw buffers across all three shader formats.
- Exact GPU arithmetic into a 19x7 mip of a 39x15 texture plus structured/raw buffers; changed constants; bad groups, mip ranges, overlapping SRV/UAV declarations, missing accesses and overlapping logical timing indices.
- Twenty-five dependent logical passes recorded in two native lists with 25 GPU timing entries, followed by precise readback.
- Renderer frames that independently change constants, texture/buffer identities and shader entry, then verify stable cache reuse and accumulation into a previous graph's explicitly initialized product. A graphics pass consumes both the generated texture and buffer. Modifying CPU parameters after publication does not change the submitted values.
- Graph undefined reads, partial writes, empty writes, buffer hazards, mip dependencies and cycles; shared lifecycle/failure tests cover retained resources and cancellation.
- HZB GPU versus CPU comparison at every float mip, nearest/farthest and both conventions: 1x1, 1x17, 17x1, 17x9, 32x32 and 255x129. Tests cover normalized viewport depth, compatible sharing, no-consumer retirement and rejection of another graph, view, camera, viewport or revision.
- An analytic contact fixture produces a known occluder shadow, no plane self-shadow, neutral uncovered pixels and bit-identical Standard/Reversed masks. Deferred checks cover CSM/contact combinations, ordinary/clustered lighting, neutral-mask baseline identity, Forward bypass, sub-viewports, resize, shared preview demand and disabled resource retirement.

Logs under the ignored build output directory: `out/compute-debug-tests.log`, `out/contact-debug-delivery-tests.log`, `out/contact-release-tests.log`, `out/contact-release-delivery-tests.log`, `out/contact-profile-tests.log`, `out/contact-final-naming.log`, `out/contact-delivery-naming.log` and `out/contact-diff-check.log`.

## Sponza acceptance

Used the current `/Game/Scenes/Sponza.hasset` and settings derived from `experiments/Scene.json`, with the existing scene camera/light, clustered lighting and exposure 2.5. Every accepted run completed with 1/1 models ready, zero model failures and zero D3D12 validation errors. Static renders had 79 main draws; the moving trajectory had 79–81. Shutdown completed normally, including Debug lifetime checks.

Twelve bounded runs passed:

- Debug and Release, each in Standard and Reversed Z: actual GUI checkbox off/on/off/on, followed by active resize to 960x540, minimize and restore. Each used 2,200 application ticks and rendered 2,198 frames because minimized ticks were skipped. Captures also passed model and GUI verification.
- Debug and Release, each in Standard and Reversed Z: 2,200 frames with a moving camera, 1,500 warmup frames and 700 measured frames. The synthetic orbit used true cursor coordinates with `--benchmark-camera-step 3`.
- Release Reversed-Z static contact on/off, and Debug/Release Reversed-Z moving contact off for comparison. Each used the same warmup/sample counts.

At 1440x728, contact requests one 11-mip product with 5,590,600 HZB payload bytes. The R8 mask adds 1,048,320 bytes: 6,638,920 bytes (6.33 MiB) total logical payload. After resizing, the GUI checks the 960x540 product's 10 dispatches and 2,764,220 bytes. With contact and preview off, all sampled frames reported zero HZB consumers, dispatches, bytes and HZB/contact GPU time.

The static final on/off images differ at 817 pixels. RGB differences range from -59 to 0 in 8-bit code values, with mean absolute difference 0.003844. Contact only adds local darkening in this scene; much of its mask overlaps existing CSM visibility. The mask was also inspected directly. Standard/Reversed final shading is not asserted bit-identical because the full renderer includes CSM/rasterization differences; the isolated contact fixture is bit-identical.

An early 300-frame probe ended before Sponza was ready and was excluded from acceptance. The subsequent ready captures and the final matrix provide the evidence above.

Example of the reusable GUI exercise with the shipped scene configuration:

```powershell
out/build/debug/bin/hyperion_viewer.exe --config experiments/Scene.json --hidden --no-vsync --frames 2200 --exercise-contact-shadows --exercise-window --capture out/contact-gui.png --verify-model --verify-ui
```

For measured runs, use a settings copy with the desired `contact_shadows` / startup `reversed_z`, `--no-ui --benchmark out/contact.csv --benchmark-warmup 1500`, and optionally `--benchmark-camera-step 3`. The temporary matrix driver is `out/ContactSponzaAcceptance.ps1`; aggregate logs are `out/contact-final-acceptance.log` and `out/contact-final-moving-baseline.log`.

## Measured cost and allocation behavior

NVIDIA GeForce RTX 5080, D3D12 debug layer enabled, VSync off, hidden window, no GUI for benchmarks, 1440x728. These are single-run observations from the normal fenced GPU timestamps, with 700 valid samples per case; no profiling-only queue waits are added.

| Case | HZB GPU mean ms | Mask GPU mean ms | All logical GPU passes mean ms | CPU frame interval mean ms |
| --- | ---: | ---: | ---: | ---: |
| Release static, on | 0.048981 | 0.185775 | 0.512868 | 5.220 |
| Release static, off | 0 | 0 | 0.281809 | 5.287 |
| Release moving Reversed, on | 0.049004 | 0.169309 | 0.486755 | 6.861 |
| Release moving Reversed, off | 0 | 0 | 0.276707 | 6.791 |
| Release moving Standard, on | 0.049263 | 0.170757 | 0.474614 | 6.913 |
| Debug moving Reversed, on | 0.049502 | 0.169598 | 0.486712 | 36.093 |
| Debug moving Reversed, off | 0 | 0 | 0.277783 | 32.642 |
| Debug moving Standard, on | 0.048964 | 0.170527 | 0.489276 | 34.993 |

Release static HZB/mask p95 were 0.049344 / 0.193187 ms. Static on frames kept PSOs at 10, cumulative descriptor allocations at 300 and live GPU allocation bytes at 533,880,832 throughout sampling. Static off kept descriptors at 268 and live allocation bytes at 526,606,336. CPU differences in these short runs should not be treated as a general performance claim.

Moving clustered lighting legitimately changes cluster-buffer bindings. Its cumulative descriptor allocation counter grows with contact off as well: Release off 285–6,161, on 320–7,020. Adding contact also changes the Deferred lighting descriptor table. This counter is not live descriptor usage. Release live GPU allocations fluctuated within 526,606,336–527,065,088 off and 533,880,832–534,339,584 on; PSO counts remained fixed. The same 458,752-byte live-allocation range in the two moving runs, stable static counts and clean shutdown support bounded lifetime behavior, rather than interpreting cumulative allocations as a leak.

Every benchmark row was checked for nonempty draws, zero failed items, expected activation/dispatch/bytes and valid GPU times. Machine-readable statistics are in `out/contact-final-report.json`; the analysis script is `out/ContactAcceptanceReport.py`.

Screenshots: [contact on](../../../../out/contact-final-release-reversed-static-on.png), [contact off](../../../../out/contact-final-release-reversed-static-off.png), [visibility mask](../../../../out/contact-sponza-mask.png), [GUI after resize](../../../../out/contact-final-debug-reversed-gui.png).

## Quality audit follow-up

The independent review of the 140-file working snapshot above baseline `3d434ec7cc26b3415ff8ffd85fa1dba16995da6a` found one in-scope P1 issue, `CHZB-001`. Direct RHI compute commands with a sole constant-page handle nested in an immutable shared command shell could reset that page after recording. The reviewer recorded `Add=10`, re-published `1234`, and read `1234` from the GPU. The Renderer caches normally retain extra handles; the probe establishes a general compute lifetime defect rather than a demonstrated Sponza artifact.

The main agent independently confirmed the reset/record/submission call chain. Compute now registers weak command owners before constant-slice validation, using the existing graphics page registration mechanism and mutex. Graphics keeps its shared-draw ownership contract. There is no new fence wait or public API change.

The added compute regression failed before the repair and passed afterward. It checks multiple dispatch pages, `RecordOwned`, a compute pass inside a batch, cancellation, retained submissions after caller references are released, immutable GPU output, and legal reset after all command owners expire. Fresh post-fix validation passed Debug 10/10 affected tests and Release 3/3 compute/graphics ownership and frame-failure tests, source formatting, naming for the five affected translation units, and dependency boundaries. These targeted checks do not replace the earlier full-suite/Sponza evidence with a claim of a fresh full matrix or profile run.

Review snapshots, the independent numeric probe, before/after regression logs and review reports are retained under `out/quality-audit-compute-hzb`. No Git commit was made.

## Remaining limits

There is no async-compute queue, indirect dispatch, counter UAV, temporal contact history, Forward depth prepass or later-receiver contact support. Screen-space depth cannot supply offscreen/hidden occluders, and minimum visibility cannot remove CSM acne. Defaults use short deterministic rays with bias/thickness and edge/end fades. These limits match the accepted first version; no required implementation work remains.
