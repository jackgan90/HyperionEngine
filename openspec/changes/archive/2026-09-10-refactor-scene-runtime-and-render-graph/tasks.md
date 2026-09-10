## 1. Baseline and contracts

- [x] 1.1 Record current Debug build/CTest and preserve baseline Viewer executables and deterministic scene/model captures plus warmed static/moving shadow measurements.
- [x] 1.2 Document runtime ownership, explicit graph/RHI contracts and unchanged user-visible acceptance requirements in proposal, design and delta specs.

## 2. Runtime scene extraction

- [x] 2.1 Implement FSceneInstance lifecycle, asynchronous loading, structured status and model operations using existing FScene/FSceneRenderBridge.
- [x] 2.2 Move loader registration and instance bookkeeping out of SceneViewer; retain camera, controls, animation and GUI behavior.
- [x] 2.3 Add independent-runtime lifecycle, loading-time editing, failure and close regression coverage; validate existing scene/model tests.

## 3. Explicit attachments and graph compiler

- [x] 3.1 Introduce explicit RHI attachment/load/store/transition contracts and migrate D3D12 binding, validation, caching and resource retention.
- [x] 3.2 Implement graph-owned resources/imports/exports, graphics attachment declarations and deterministic resource-hazard dependencies.
- [x] 3.3 Implement content validity, regional clear, discard, compatibility and conflicting access validation with positive and negative tests.
- [x] 3.4 Migrate all owned direct-RHI and graph clients/tests; remove the legacy target-flag authoring path.

## 4. Pipeline and preparation separation

- [x] 4.1 Separate view data from pass target/read declarations and derive material target signatures from attachments.
- [x] 4.2 Declare shadow, forward, depth-preview and GUI passes before deferred draw preparation; preserve segmented draw order and one frame coordination boundary.
- [x] 4.3 Preserve retained draw/batch caches, independent shadow culling, immutable ownership and GPU-safe failure recovery; run targeted regressions.

## 5. Delivery validation

- [x] 5.1 Complete Debug/Release builds and full CTest including current SceneViewer/ModelViewer acceptance, shadow GPU tests and frame recovery.
- [x] 5.2 Compare deterministic before/after viewer rendering and warmed static/moving CSM performance; investigate material regressions without weakening checks.
- [x] 5.3 Run formatting, semantic naming, module boundaries, OpenSpec strict validation and git diff checks.
- [x] 5.4 Update architecture and verification documentation with final implementation, evidence and any remaining limitations; verify all tasks are complete.
