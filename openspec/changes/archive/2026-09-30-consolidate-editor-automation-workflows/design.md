## Context

The solo review covered recent Editor/Automation work through `ab52614a389a70bec8bfa868c98510b73fa20401`. Shared SceneEditing and AssetEditing documents already own history and persistence; this change completes shared orchestration and reduces caller coupling without replacing those foundations. Repository AGENTS.md, CodingStyle.md, PluginSystem.md, Automation.md and VisualStudio.md govern implementation. In particular boolean names use lowercase `b`; the older uppercase-boolean sentence in openspec/config.yaml does not override these current rules.

## Goals / Non-Goals

**Goals:** address all ten reviewed problem groups; preserve established transaction semantics; make input ownership and resource lifetimes explicit; remove unnecessary scene traversal; reduce mixed-responsibility Editor state and private adapter repetition; keep supported contracts discoverable and current.

**Non-Goals:** new rendering features, arbitrary file-size limits, per-panel plugins, an application-owned feature lifecycle, generic frameworks without two real consumers, changes to light priority/clipboard semantics, protocol renumbering, implicit save, new import formats, archive, commit or push. Do not modify historical OpenSpec archives or unrelated work.

## Decisions

### 1. Share shortcut policy, retain command distinctions

`EditorDeletion.cpp` currently lacks the focus, popup and current-frame text ownership guards used by selection/framing; history routing runs before GUI frame construction. Introduce an engine-owned Editor-private interaction snapshot and explicit command admission policy. Capture focus/window/text/popup/modal/navigation/placement/gizmo/hierarchy state at the point where it is current, including input transitions in the same batch. Route commands through it and the shared document idle checks. Keep any intentional inspector edit completion and command-specific panel eligibility explicit. A single blanket Boolean guard is rejected because history and selection need different semantics. Moving code without fixing the Delete admission path is insufficient.

### 2. Put asynchronous asset editing alongside asset documents

GUI texture editing records an encoding generation and polls it; automation independently dispatches and commits. Reference edits likewise duplicate load/validate/commit and busy ownership. Add a focused Runtime/AssetEditing workflow abstraction for admission, an owned preparation snapshot, task completion, document identity/generation revalidation, shared commit/history, and busy cleanup. Main alone mutates documents; worker tasks operate on owned CPU data. Use existing task/asset wrappers, explicit dependencies and lifecycle ownership. GUI and standalone/attached automation documents use the same workflow while retaining their existing session identity and workspace lifetime adapters. The common workflow must not capture transient GUI/transport objects or introduce global services. Quiesce/drain must join before state destruction. Failure or stale completion cannot commit, save, advance history or leak busy state. Merely sharing the CPU conversion helper leaves the reviewed duplication unresolved.

### 3. Separate cheap validation from scene queries

`SceneAuthoring.cpp` validates through `ListSceneNodes(..., 0, 1)`, which still enumerates all handles. Component preparation layers settings and describe queries over that. Introduce direct document/revision/live-handle validation with consistent errors. For batches, validate common state once and each member atomically before commit. Keep result-building queries independent. Use an instrumented target to prove no `Nodes()` enumeration during single-node validation and no K repeated enumerations for K component edits; avoid timing thresholds.

Share ordered root filtering between `SceneDocument.cpp` and `SceneStructure.cpp`. Validate handles according to each caller's established contract, then stop ancestor traversal as soon as a selected ancestor covers the node. Preserve order, primary, stale/duplicate errors, atomicity, child transforms and resource sharing. A deep chain with all nodes selected must not traverse every ancestor for every node. Do not silently remove validation while optimizing.

### 4. Give Editor state cohesive owners

`FEditorPlugin` combines viewport/render state, document replacement/discard flow and acceptance-test machinery. Extract an acceptance driver/state boundary with named phase routing and explicit test input-window targets, replacing step ranges embedded in production input routing. Gate acceptance implementation on BUILD_TESTING while preserving supported test flags in test builds and producing a controlled diagnostic when unavailable. Test-disabled Editor must link without acceptance sources.

Extract viewport state/behavior and pending document transition/close state into focused private owners, keeping lifecycle and service registration in the Editor plugin. Owners must have narrow dependencies and real behavior, not become bags of references to every plugin member. Split `AdvanceFrame`, `DrawDiscardDialog`, `Render` and `RunEditorApplication` where responsibilities differ. Reuse existing owners when appropriate. Declarative operation tables need not be mechanically split to meet a line count. Preserve renderer snapshots, provider lifetime, fences and optional-provider behavior.

### 5. Make import UI consume semantic contracts

Build selectable import types and allowed settings from capabilities using stable type IDs; indices are UI positions only. Keep Auto and currently supported Model/Texture/Sky inputs and extension restrictions. Do not broaden native/source format admission accidentally. Add reflected typed texture dimensions and pixel byte size to draft inspection metadata; retain existing human-readable Details for compatibility but do not parse it in GUI. Name preview pagination limits and use the same limit in request and navigation. Validate capability reordering, reflected schema, and GUI/automation inspection parity.

### 6. Name numeric semantics and reuse private adapters

Add engine-owned input button/modifier definitions and helpers; preserve existing numeric values and the SDL adapter boundary. Replace relevant Editor bit masks and button literals with these names. Name per-session request capacity, retained results, peer limits, handshake size, dispatch budgets and connection timeout in their owning modules, sharing a constant only where it represents the same invariant. Keep wire values/defaults unchanged. Use existing math conversion facilities or focused named conversion helpers for radians/degrees.

Consolidate repeated typed scene operation registration and `FSceneEditError` conversion inside the Automation plugin. Keep every operation's owner, effects, completion mode, availability, examples and schema explicit and stable; do not put scene branches in Runtime transports or homogenize different metadata by accident.

### 7. Remove proven stale material

Verify references before removing private `FNodeHistory`, handle-hash/history-remap wrappers and tests that only preserve unused adapters; move meaningful assertions to the actual shared authority. Correct active Editor/light/shadow docs and `scene.settings.get`, render-settings and unsupported asset error descriptions. Scene settings now carry camera/initial-view state; skies use priority and copied lights preserve authored priority, so docs must not promise unchanged source light selection. Preserve persisted compatibility paths and shipped operation IDs.

## Risks / Trade-offs

- Input timing changes → test focus/text/popup transitions and same-batch commands through the real routing path, including Delete, clipboard, framing, selection and history.
- Async lifetime regressions → deterministic stale/failure/busy/close/drain tests and GUI/automation parity, with work joined before destruction.
- Large structural refactor → implement in independently reviewable stages, run focused tests per stage and integration checks once combined; orchestrator reviews diffs and returns all repairs to the implementer.
- Metadata or schema drift → compare discovery/describe contracts and invoke representative operations through catalog/transport; keep additive DTO changes reflected.
- Conditional acceptance linkage → configure/build test-enabled and test-disabled variants; exercise optional provider absence and shutdown paths.
- Long build or unavailable GPU environment → report exact verified coverage and genuine blockers, preserve runnable tests and do not claim live acceptance without execution.

## Migration Plan

No persisted data migration or user action is expected. Existing operation IDs, serialized fields and numeric input values remain compatible. Land internally in the task order, with shared primitives preceding caller changes. Rollback is a normal source change; no repository history operation is authorized. Keep the active change and all implementation uncommitted and unarchived.

## Open Questions

No user decision blocks implementation. The implementer must confirm current ownership and exact build/test entry points before choosing concrete private type names. If evidence invalidates a review finding or reveals an unavoidable contract change, send the evidence to the orchestrator before expanding scope.
