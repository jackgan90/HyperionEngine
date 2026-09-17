## 1. Scene and rendering contracts

- [x] 1.1 Add reflected validated initial-view metadata, schema compatibility, settings/snapshot persistence, and focused regression tests.
- [x] 1.2 Add shared browsing initialization and strict camera preview resolution; test camera-free views and disabled/missing targets.

## 2. Editor and SceneViewer behavior

- [x] 2.1 Implement independent startup and explicit initial-view authoring with undo/redo/save semantics.
- [x] 2.2 Implement view-source UI, preview/return, apply view to camera, optional camera creation, and unavailable-preview feedback.
- [x] 2.3 Move SceneViewer navigation to an independent value view and verify saved scene data remains unchanged.

## 3. Migration and verification

- [x] 3.1 Stage guarded Sponza/source metadata migration and prove other scene objects and shared dependencies are preserved.
- [x] 3.2 Verify Debug/Release builds, focused runtime tests and actual editor save/reopen/preview interactions.
- [x] 3.3 Update documentation and complete formatting, naming, boundary and OpenSpec checks.
- [x] 3.4 Publish verified Sponza assets to F:/HyperionAssets after explicit authorization; verify the published graph, unchanged dependencies, and cache hit.
