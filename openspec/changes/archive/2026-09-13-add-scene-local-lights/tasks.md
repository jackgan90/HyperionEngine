## 1. Scene data and persistence

- [x] 1.1 Add typed point/spot nodes, validation, hierarchy-aware edits and Scene APIs.
- [x] 1.2 Extend reflected/source persistence, migrations, importer revisions and save/reload tests.

## 2. Publication and visibility

- [x] 2.1 Publish immutable local-light state with exact scene tokens and retained frame ownership.
- [x] 2.2 Reuse spatial index/visibility contracts for independent incremental light culling and statistics.

## 3. Deferred lighting

- [x] 3.1 Extract common direct BRDF and add finite distance/cone attenuation plus shader contracts.
- [x] 3.2 Implement shared conservative closed volume meshes, material preparation and graph accumulation.
- [x] 3.3 Integrate Deferred-only routing, inactive Forward reporting and existing CSM/debug behavior.

## 4. Viewer and Sponza

- [x] 4.1 Add point/spot creation/editing, influence visualization and diagnostics to SceneViewer.
- [x] 4.2 Tune persistent Sponza point lights toward the reference screenshot and capture on/off comparisons.

## 5. Validation and delivery

- [x] 5.1 Add/run scene publication/culling and real GPU volume/attenuation/additivity/routing regressions.
- [x] 5.2 Validate queued lifetimes, camera/light motion, resize, pipeline changes and bounded resources.
- [x] 5.3 Run Debug/Release builds, relevant CTest, shader/style/naming/boundary checks and strict OpenSpec validation.
- [x] 5.4 Document implementation, Sponza visual evidence and measured limits; finish tasks without Git commit.
