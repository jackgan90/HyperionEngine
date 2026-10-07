# Quality audit

Date: 2026-10-07. Repository: `F:\HyperionEngine`. Baseline: `a843904345d8c1384201f0259e40ff256bed4dd1`.

## Scope and version

The independent reviewer was created without inherited conversation history and reviewed all related tracked modifications and new files in the uncommitted working tree. There were no staged changes or known unrelated existing edits. The initial snapshot is recorded in `out/SceneLifecycleAudit/InitialSnapshot.sha256`; both reviewer and primary agent rechecked every recorded file with zero mismatches before recording this audit. Reviewed source files were not modified during the review.

The review covered scene new/close, Save/Discard/Cancel and dismissal, untitled persistence, save failure and late completion, stale documents/handles, history/selection/transient state, independent asset documents and content roots, closed-scene GUI/rendering, native modality, typed automation parity, provider absence, shutdown draining, production/test boundaries and documentation.

## Findings and primary-agent verification

Confirmed defects: **none**. No source repair was warranted.

One suspected issue was investigated: a synchronous save failure hides the scene decision without explicitly closing the current ImGui popup. The independent multi-frame probe showed that a popup no longer submitted is automatically cleared on the following frame, with focus restored to Main. The primary agent separately checked the raw probe output and the `NewFrame` → `FocusTopMostWindowUnderOne` → `FocusWindow` → `ClosePopupsOverWindow` call chain. Persistent modal blocking is therefore not supported by the evidence, and the suspicion was rejected. The original document is not retired on that failure. Uppercase save-extension validation predates this change.

The primary agent also independently read the lifecycle admission/commit/retirement, shared save polling, transition cancellation, closed-instance and interaction paths; checked the existing Debug/Release runtime reports and raw test logs; and inspected the closed viewport capture. These checks did not identify a further confirmed defect.

## Verification and limits

- Independent Debug CPU tests `automation_scene`, `editor_interaction_policies` and `editor_document_transitions`: **3/3 passed**, recorded in `out/SceneLifecycleAudit/Reviewer/FocusedCpuTests.log`.
- Independent multi-frame ImGui probe: popup stack cleared and focus recovered, recorded in `out/SceneLifecycleAudit/Reviewer/ModalProbe.log` with its source beside it.
- `git diff --check` passed; reviewed snapshot hashes matched.
- Existing final Debug runtime selection passed 3/3 and Release lifecycle selection passed 2/2. Original logs, lifecycle reports and the closed-scene capture were inspected; GUI reports record zero GPU validation errors.
- Full builds and GPU suites were not repeated during this audit because no source changes were made. Device loss and resource exhaustion were not injected. The synchronous GUI save-failure path was checked statically; its disputed ImGui lifetime behavior was independently executed in the probe.

The independent report is `out/SceneLifecycleAudit/Reviewer/IndependentReview.md`. No unresolved evidence-backed defect remains within the reviewed scope. The OpenSpec change remains active; no archive, spec sync, Git commit or push was performed.

## Subsequent user acceptance correction

After the reviewed snapshot, user acceptance clarified that a closed scene should not produce per-object waiting prompts in Place Object. The primary agent confirmed this presentation issue in `EditorPlacement.cpp` and the earlier closed capture, then applied a bounded correction: hide preparation/tooltips/footer for the typed closed state and clear retired placement feedback. This is a post-review correction; the original independent report applies to its recorded snapshot, rather than this updated source.

Debug/Release Editor builds, existing Debug lifecycle/placement tests (3/3), Release GUI lifecycle (1/1), style, focused naming and visual capture checks passed. No shared placement validation or automation contract was changed. Follow-up evidence is recorded in [verification.md](verification.md).
