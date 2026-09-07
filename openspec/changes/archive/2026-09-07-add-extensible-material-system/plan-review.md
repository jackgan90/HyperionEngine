# Material system plan review

## Scope and review identity

- User-authorized scope: design and implementation plan for the material system, including triangle/model-viewer/scene-viewer migration and directly required RHI/DebugUI adaptation. This turn does not implement, archive, stage or commit.
- Baseline: `1847be673f72f9877e62fb2d80556e0f8a10ccc9` in `F:/HyperionEngine`.
- Independent reviewer: Gauss, agent `01a07aa7-d5c4-7521-869b-d26aefe88d81`, created with `fork_context: false`. This is the available tool's equivalent of a reviewer without inherited conversation context. The reviewer received a self-contained requirements/scope/version brief and did not create another reviewer or edit files.
- Round 1 input: proposal, design, tasks, nine capability specs and `.openspec.yaml`; 13 files frozen by `out/MaterialPlanReview/Round1Manifest.json`. SHA256 matched before and after review. Raw reviewer report is retained in `out/MaterialPlanReview/ReviewerRound1.md`.
- The main agent independently checked the cited current code and plan conditions before modifying the artifacts. The table below records those decisions; reviewer suggestions were not treated as automatically correct.

## Round 1 findings and main-agent disposition

Plan line numbers in the evidence column refer to the frozen Round 1 version, not the revised files.

| ID | Priority / evidence | Main-agent assessment | Revision and verification path |
| --- | --- | --- | --- |
| R1 | P1; design:119-121; shared-render-resources:19. Current RenderResourceLifetime.cpp:37 publishes only Ready descriptions; SceneVisibility.cpp:26 filters them before RenderSession.cpp:25 creates frame bindings. SceneRenderTests.cpp:36 waits for Ready before Build. | Confirmed phase-contract gap. A frame-dependent Ready requirement could prevent first collection. In scope. | D8 now separates CPU interface, static ResourcesReady and group-scoped DrawPreparation, defines LastDrawResult and recovery; first-frame/section failure/stale diagnostic scenarios and task 9.8 added. |
| R2 | P1; design:29-35,71-73. ShaderCompiler.h:37 returns artifacts; RenderResourceService.cpp:68 prepares on Worker without a CPU schema handoff. | Confirmed. Independent Materials cannot discover names or validate Main writes without an explicit prepared-interface handoff. In scope. | D4 defines async PrepareMaterialDefinition, Materials-owned prepared schema, Main acceptance/migration, InterfaceNotReady and variant merging; new spec requirement and task 2.6. |
| R3 | P1; design:87-95. RenderPrimitive.h:54 allows multiple outputs; SceneCollection.cpp:63 assigns them one primitive handle. | Confirmed. Primitive/frame identity alone does not identify distinct emitted Object/Draw payloads. In scope. | D6 defines LocalItemId/ordinal, occurrence scope and content-checked keys, plus group identity; multi-item/reordered-output scenarios and tasks 5.7/9.2. |
| R4 | P2; design:55-56; D3D12Uploads.cpp:61 creates only RGBA8 sampled textures. | Confirmed supported-matrix contradiction. Independently checked Microsoft's format support table: RGBA8 UNORM/SRGB do not support comparison filtering. In scope with a small bounded correction. | D3 reports comparison sampling Unsupported, with explicit rejection requirements/tasks. No sampled-depth, shadow or offscreen implementation added. |
| R5 | P2; design:21,29-31,115-117. Existing CPU images are in Assets/Scene, and mip/sampler descriptions in RHITypes.h:17. | Confirmed missing author-to-Renderer resource input protocol. In scope. | D1 defines immutable owned CPU texture/read-buffer source/view and sampler values, validation and source lifetime; independent input scenario and task 1.6/6.1. |
| R6 | P2; design:101-105,117. D3D12Resources.h:19 currently owns individual texture slots; swapchain recording directly selects them. | Confirmed. Existing slots cannot supply an implicit correct general binding-set cache. In scope. | D7 now specifies complete ordered layout/resource/view/sampler cache keys, dynamic-CBV exclusion, ownership, allocation/copy counters and gated old-set lifetime; new requirement/tasks 6.5/9.4. |
| R7 | P2; material-parameter-binding:62-67 versus design M02/M04 and GPU tasks. ShaderTests checks reflection/cache, not actual GPU packing. | Confirmed acceptance gap. Reflection success is not evidence that arbitrary bytes reach GPU correctly. In scope. | D11 and task 9.7 require nested padded structs/arrays, bool/int/uint and both-major non-square matrices in a real GPU fixture with an independent CPU numerical oracle. |

R4 external evidence: [Microsoft Direct3D feature-level 12.0 format support, RGBA8 section](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/hardware-support-for-direct3d-12-0-formats#dxgi_format_r8g8b8a8_unormfcs-28). No external source was used to infer a repository implementation that was not inspected.

## Additional main-agent checks

- CPU schema and resource input gaps were also found independently (MA-01/02), and closed by the same R2/R5 revisions.
- MA-03: added one Render invocation per BuildViews family, explicit unique graph names and frozen frame/provider input in ViewerFrame. Existing independent Build calls collected anew and generated repeated Scene 0 names; they could not establish the promised family boundary implicitly.
- MA-04: Opaque and Masked retain one opaque ordering bucket and their existing stable relative order, including equal-depth cases. Baseline SceneVisibility.cpp:64 only separated blended from non-blended draws.
- Clarified Manual/Semantic source independently from override permissions, and specified FModel update preparation versus publishing so one shared material edit can enter one existing Render batch.
- No confirmed finding requires large development outside the material scope. Full shadows, MSAA/resolve, arbitrary RenderGraph, persistent asset format changes and Tasks/submission redesign remain excluded. A future mandatory dependency on such work still triggers the user's scope decision gate.

## Round 2 targeted independent review

- Input: the 13 revised core artifacts frozen by `out/MaterialPlanReview/Round2Manifest.json`; this audit log is excluded from that manifest so final results can be recorded without changing the reviewed plan. The original independent reviewer checked R1-R7, the actual revised requirements/tasks and directly affected baseline call paths. Full report: `out/MaterialPlanReview/ReviewerRound2.md`.
- Reviewer disposition: R1-R7 are all closed at the plan level. No confirmed new material-scope design executability gap was found in the revisions. The reviewer explicitly distinguished an implementable plan from completed implementation or successful runtime verification.
- R1 closure covers static readiness before any frame, group-scoped draw preparation, contextual errors and recovery. R2 covers Worker-to-Main prepared-schema delivery and versioned migration. R3 covers multiple emitted items, occurrence identity and effective-content cache checks.
- R4 closes the capability contradiction by rejecting comparison sampling in the first tier. R5 supplies independent owned CPU resource inputs. R6 supplies binding-set keys, ownership and descriptor reuse counters. R7 supplies actual GPU packing acceptance with an independent numerical oracle.
- Direct cross-checks found the BuildViews boundary compatible with the existing Main-to-Render submission path, the Scene bridge batch change bounded by existing admission/generation contracts, and group staging compatible with existing scene sorting. No Tasks/transaction/RenderGraph redesign is required by the reviewed plan.
- The main agent verified the fixed input hashes and baseline again, retained the original evidence and checked the spec/task structure. No core artifact was changed during re-review. No new finding requires further revision or a scope decision; optional unrelated work was not added.

## Validation and final status

- Round 1 change strict validation passed; all-spec strict validation passed 26/26.
- Structural cross-check: 9 capability specs, 32 initial requirements, 69 initial scenarios, 50 initial implementation tasks; all original scenarios in MODIFIED requirements retained. The first custom checker run contained a DOTALL regex error; the checker was corrected and passed, with no artifact change needed.
- Final revised change strict validation passes; all-spec strict validation passes 26/26. OpenSpec reports all four planning artifact types complete, which means the proposal package is ready, not that implementation is complete.
- Final structural check: 14 files including this log, 9 capability specs, 35 requirements, 83 scenarios and 56 unique unchecked implementation tasks. Proposal capabilities match spec directories, all original scenarios in MODIFIED requirements are retained, M01-M09 have implementation task references, and artifact whitespace/final-newline checks pass.
- All 13 Round 2 core SHA256 hashes still match and HEAD remains `1847be673f72f9877e62fb2d80556e0f8a10ccc9`. The only Git-visible worktree addition is this change directory. No source, shader, test implementation or existing spec was modified; no build/GPU test, archive, staging or commit was performed.
- Final audit status: independent initial review, main-agent verification and bounded plan revisions, followed by independent targeted re-review completed. R1-R7 are closed; no confirmed unresolved material-scope execution blocker or pending scope decision remains. Implementation risks and required runtime acceptance remain explicitly documented in design/tasks.
