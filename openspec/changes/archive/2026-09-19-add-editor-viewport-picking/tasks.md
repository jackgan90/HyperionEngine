## 1. Spatial primitives

- [x] 1.1 Add bounded ray intersection helpers and extract reusable bounds BVH behind the existing Renderer adapter.
- [x] 1.2 Add compact immutable triangle acceleration with nearest traversal and affine transform coverage.

## 2. Scene queries and preparation

- [x] 2.1 Add Main Scene ray queries, independent incremental transaction invalidation, filtering, material policy and availability.
- [x] 2.2 Enable optional Worker geometry preparation through scene-instance loading with existing cancellation and shutdown ownership.

## 3. Editor integration

- [x] 3.1 Share camera projection construction and expose clipped viewport ray generation.
- [x] 3.2 Integrate press/release selection arbitration, persistent clear selection and preview/lifetime handling.

## 4. Validation and documentation

- [x] 4.1 Add deterministic geometry, incremental update, readiness, view-ray and input regression coverage, including candidate-work comparisons.
- [x] 4.2 Run relevant builds, spatial/editor regressions, style/naming/boundary checks and strict OpenSpec validation; document results and limitations.

## 5. Reported interaction regression

- [x] 5.1 Reproduce Outliner light selection followed by a held viewport click on real Sponza geometry; fix gesture ownership and cover selection/gizmo transitions without authoring edits.

## 6. Independent quality audit

- [x] 6.1 Fix conservative nearest-hit traversal limits and scheduled material-pass exclusions, add regressions, and complete independent re-review.
