## Context

The read-only investigation at `5895bd7f9409a752b868bc46c34e3e89c9f7fca3` is recorded in `out/minimal-update-review-20260909`. Static CSM has 195 main items / 6 draws and 643 shadow items / 24 draws. Small camera motion causes about 713 item evaluation refreshes per frame despite only about 24 provider evaluations and 2 KiB of constant writes. Fixed-visibility Forward updates 235 item results for one 80-byte View block.

The existing system already has reflected layouts, immutable value pages, shared provider overlays, bounded caches, retained view packets and D3D12 state/validation reuse. The implementation must extend those boundaries without weakening extension behavior or recreating a Model-specific material path.

## Goals / Non-Goals

**Goals:**

- Propagate shared engine parameter changes independently of stable local material/object evaluation.
- Retain stable scene preparation and reduce repeated visibility-driven item/batch data construction.
- Reuse independent instance blocks and packed records; preserve unchanged blocks during local updates.
- Keep static paths cheap and support multiple sequential view families without mutual eviction.
- Deliver frozen-binary A/B evidence for Debug and Release with unchanged validation, CSM settings, pixels and source-item coverage. Initial engineering targets are at least 60% less small-motion Debug CSM preparation and 50% less large-motion preparation, with actual results and remaining limits reported.

**Non-Goals:**

- Disabling Debug checks, lowering shadows, reducing rendering frequency, changing material authoring semantics. Git commit was excluded from the initial implementation delivery and authorized separately afterward.
- Unconditionally introducing GPU-driven drawing, a bindless ABI, or native command-list replay. Such changes need evidence beyond the current CPU preparation bottleneck.

## Decisions

### Shared updates and persistent local evaluation

Represent a prepared material as stable local values plus an immutable shared engine update. Prepare compatible shared updates once per pass/override/dependency group. Reuse local evaluation when authoring/object/draw inputs and local provider dependencies remain unchanged. Compose effective values only where needed by actual emitted draws or incompatible/mixed paths. Resource changes, missing values, overrides, shared values appearing in instance records, and custom mixed providers must retain correct fallback behavior.

All consumers (batch compatibility, instance packing and RHI material preparation) must observe the effective composed values; no mutable global current-view state or mutation of retained snapshots is allowed. Cache keys still validate full semantic/layout identity, and hashes only select candidates.

### Stable scene preparation and visibility

Retain view-independent collection/preparation for stable primitives and reuse immutable material/geometry metadata. Culling and transparent ordering remain view-dependent. Changed visibility must not force repeated material lookup and local value preparation for unaffected items. Measure container copying, receipts and retirement after shared update changes; address the measured remaining cost using bounded engine-owned caches.

### Instance records and blocks

Keep the current reflected constant-buffer ABI. Cache packed instance records and independent immutable blocks by complete layout/mapping/effective values. Block reuse is independent of the View identity and unrelated pass bindings where the block contract is compatible. A changed Object block must not force unchanged Surface bytes to be packed/uploaded. Visibility changes can still require assembling a new contiguous block under this ABI, but unchanged records and independent blocks remain reusable. A separate visible-index shader ABI is reserved for follow-up only if measurements show it necessary.

### Draw preparation and native validation

Retain geometry/material resource preparation and refresh constant bindings using effective shared/local identities. Avoid preparing unused single-instance blocks before replacing them with batch bindings. Keep existing native state suppression and ownership checks. Measure the remaining native work after CPU preparation improvements; change native stream representation only when the measured gain justifies the additional contract.

### Measurement and evolving decisions

Preserve original Debug/Release/Profile binaries and dependencies before rebuilding. Use serial, alternating baseline/candidate runs, warmed ready scenes, small and large real input-driven camera motion, fixed visibility, and visible UI as separate workloads. Report frame distributions, preparation stages, provider/item work, packed/uploaded bytes and native creation/binding counters. Use traces for attribution rather than mixing profiling timings into the uninstrumented A/B table. Record ineffective approaches, newly found bottlenecks and resulting design changes here and in implementation evidence before completing affected tasks.

## Risks / Trade-offs

- Shared updates could leak across views or retained frames → immutable update ownership, mixed-provider and old-frame tests, no in-flight slice overwrite.
- A view-independent cache could miss custom collection or ordering changes → require a stable collection contract and preserve conservative fallback and transparent sorting.
- More cache layers could retain resources indefinitely → explicit entry/byte limits, weak ownership/retirement, pressure and removal tests.
- UI/Present scheduling can hide CPU changes → separate stage timing from whole-frame distributions, preserve all A/B samples and report variability.
- Per-block instance reuse still copies a changed contiguous block → report actual copied/uploaded bytes; do not claim per-element GPU updates under an unchanged CBV ABI.

## Migration Plan

No authored asset or CLI migration. Implement and test in the existing checkout, keeping the reference runtime under `out/incremental-render-updates-20260909/baseline-*`. Synchronize this design and tasks as evidence develops. The initial implementation was retained uncommitted for review; the subsequent user instruction authorizes specification sync, archive and Git commit.

## Open Questions

- How much scene collection/receipt and batch regrouping cost remains after eliminating shared-only per-item material publication?
- Does native plan invalidation still contribute enough to justify a new static-template/dynamic-binding RHI representation?

These are measurement decisions within the authorized scope, not approval gates.

## Implementation evidence and adjustments

- Shared-only publication is working: the Debug detail trace (`out/incremental-render-updates-20260909/stage2-trace`) records 712.725 shared item updates/frame, zero ordinary incremental refreshes and 0.01 full evaluations/frame. Provider evaluations remain 23.675/frame. Full local result publication has been removed from this workload.
- This alone does not meet the target. Provisional uninstrumented small-motion preparation measured 21.951 ms in stage 1 and 27.455 ms in stage 2; these are single diagnostic runs, not the final alternating A/B result. The stage 2 detail trace attributes 5.484 ms to material preparation, 4.098 ms to scene snapshot preparation, 3.008 ms to collection, 4.626 ms to draw preparation and 1.846 ms to batch planning. Shared group lookup itself costs 2.441 ms. Additional retained metadata by itself has not demonstrated a speedup.
- Replace repeated shared-mask lookup with retained immutable group identity. Continue profiling collection/culling/container publication and receipts instead of attributing the remaining cost to native command recording (0.408 ms/frame in that trace).
- Instance packing uses canonical reflected record contracts (format, stride, complete member layouts and material name/semantic mappings), a bounded per-item record history, and independent immutable block identities. CPU history has separate bounded record/block halves within the packing budget, in addition to existing whole-chunk history. GPU constant publication is keyed per immutable block so unrelated blocks can survive a chunk update.
- View state is retained for up to 120 inactive frames with at most 64 entries. Publication invalidation still releases prepared snapshots. The object-retirement regression retained 7 constant blocks instead of its previous limit of 6 because the independent second view now retains its one View block. The assertion is updated to include that explicit bounded retention, while still rejecting accumulation across 64 object updates.

- Stage 5 alternating small-motion Debug A/B (two trials, frozen baseline) measured baseline preparation 27.472/26.467 ms and candidate 22.223/23.338 ms: a 15.5% mean reduction, still below target. The matching diagnostic trace shows collection 4.438 ms, snapshot preparation 3.383 ms (including 1.993 ms ordering publication and only 0.858 ms culling), materials 3.619 ms, batch planning 1.856 ms and native recording 0.303 ms. Reusing metadata and moving full `FRenderItem` values still repeatedly moves/destructs their many parameter containers.
- Scene snapshots therefore move ordered owning handles through `FRenderItemList`; copy operations explicitly clone items to preserve earlier snapshots. Custom `IRenderPrimitive::Collect` continues emitting its existing `std::vector<FRenderItem>` values; conversion occurs at collection admission, and view-only visibility/order changes transfer existing item ownership. The snapshot list uses engine-style accessors and indexed/range access rather than promising contiguous item storage. Culling math, transparent sorting and receipt ordering remain unchanged.
- An asynchronous readiness race exposed by the full Showcase runtime was fixed: geometry metadata may exist before its material program becomes ready, so instance packing re-queries a missing cached program and rejects a genuinely unavailable program before dereferencing it. Raw debugger evidence is retained in `out/incremental-render-updates-20260909/Stage4Hang.txt`; the failed A/B is excluded from performance comparisons.
- The configured batch byte limit now covers both whole-chunk history (one half) and record/block packing history (the other half). Canonical layout associations are additionally bounded to 256 entries.

- Stage 6 small-motion alternating Debug A/B measured 26.432/27.140 ms baseline preparation and 18.306/18.132 ms candidate (32.0% reduction). Stage 7 large-motion A/B measured 33.354/33.503 ms baseline and 25.681/26.007 ms candidate (22.7%). These remain below the initial engineering targets. The stage 6 large-motion detail trace attributes 9.512 ms to planning (6.949 ms fresh grouping), 4.051 ms to materials, 3.013 ms to collection and 3.512 ms to draw preparation. Native draw recording is 0.251 ms; a new native command-stream ABI is not justified by this evidence.
- Fresh grouping still copied complete candidate metadata whenever shared numerical values changed. Private cached candidates now update their shared values in place only when uniquely owned; a live planning group forces a copy, preserving the old compatibility proof. Scene reuse lookup also uses one contiguous sorted index instead of allocating one tree node per item on every camera change. Detail scopes separate cached item description, group comparison, instance data and plan publication for continued attribution.
- The plan-retirement test now gives byte history 256 KiB while preserving its sixteen-item plan budget, so it continues isolating expired-plan item accounting after record/block history was added. The separate 64 KiB pressure test still enforces aggregate cached bytes and eviction.

- Stage 8 large-motion preparation measured 23.466 ms in a provisional candidate run. The candidate description now separates immutable structural compatibility from shared values. A bounded weak structural pool canonicalizes only after complete geometry/layout/state/resource equality; hashes remain bucket selectors. Per-view shared value lists are reused only when every non-instance constant member is covered by the same immutable update. Custom strategies continue receiving the complete current signature. This removes repeated structural comparisons and value-list construction during fresh grouping without reusing a visibility plan blindly.

## Final outcome

The frozen final A/B measured Debug CSM small-motion preparation 21.177 → 13.599 ms (35.8%) and large-motion 33.132 → 22.330 ms (32.6%). Release measured 1.736 → 1.094 ms (37.0%) and 3.031 → 1.913 ms (36.9%). The original 60%/50% Debug engineering targets were **not reached**; static Release preparation and several whole-frame metrics regressed. The complete matrix and pooled distributions are in [the performance report](../../../../docs/IncrementalRenderUpdates.md).

Final diagnostic traces still show item-dependent shared validation, collection/ordering, plan publication and draw/receipt preparation. Large-motion planning is 6.895 ms; native draw recording is 0.250 ms. The implementation preserves the existing native/shader ABI and its checks. Continuous instance blocks are still reassembled when membership changes; packed records and unaffected independent blocks are reused. The delivered scope does not include persistent GPU slots or visible-index indirection.

All required behavior and verification are recorded in [implementation.md](implementation.md): 28 A/B workload pairs, 12 exact image pairs, sustained runs and visible UI, builds/tests, independent source review and documented infrastructure retry. Current APIs, bounded histories and statistics are synchronized with `docs/Materials.md` and `docs/InstanceBatching.md`. After the initial uncommitted review, the user authorized archival and Git commit on 2026-09-09. The four requirements are synchronized to the main `incremental-render-updates` specification.
