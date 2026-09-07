## 1. Scene foundation

- [x] 1.1 Add reusable conservative bounds/frustum math and immutable model spatial metadata.
- [x] 1.2 Implement Main-owned logical scene, stable handles, validated instance edits and acknowledged owned changes.
- [x] 1.3 Add reflected scene manifests with validation and relative asset references.

## 2. Render integration

- [x] 2.1 Implement replaceable spatial/visibility interfaces, BVH rebuild/refit and comparison statistics.
- [x] 2.2 Integrate culling groups and optional primitive bounds before Collect, retaining deterministic item processing.
- [x] 2.3 Implement logical scene bridge, complete initial state and atomic member removal with observed receipts.

## 3. Viewer

- [x] 3.1 Migrate ModelViewer to the logical scene and bridge without changing its interfaces.
- [x] 3.2 Add asynchronous manifest SceneViewer, camera, runtime manipulation and debug/culling controls.
- [x] 3.3 Wire application/configuration, example multi-model content and separate visibility/draw diagnostics.

## 4. Verification and delivery

- [x] 4.1 Add mathematical, logical lifecycle and randomized spatial differential regression tests.
- [x] 4.2 Add render/Viewer acceptance for early collection, multiple outputs, lifecycle, modes and visible interaction.
- [x] 4.3 Run style, semantic naming, boundary, Debug/Release builds and appropriate complete tests; fix failures.
- [x] 4.4 Document usage, architecture, measured evidence and final OpenSpec task status without committing.
