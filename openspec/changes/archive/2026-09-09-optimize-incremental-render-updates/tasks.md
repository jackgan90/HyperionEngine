## 1. Baseline and observability

- [x] 1.1 Preserve reference runtimes and binary hashes; retain the investigation's workload/settings and raw measurements.
- [x] 1.2 Add reusable counters and benchmark reporting for shared-only updates, retained scene preparation and independent instance packing/reuse.

## 2. Shared material and draw updates

- [x] 2.1 Introduce immutable shared parameter updates with persistent local evaluation and conservative fallback for mixed/override/resource transitions.
- [x] 2.2 Make batch compatibility, instance consumers and draw constant preparation observe effective shared/local values without per-item full result publication.
- [x] 2.3 Cover old frames, multiple views, generic/mixed providers and resource/default transitions; measure this stage and synchronize design decisions.

## 3. Scene and instance preparation

- [x] 3.1 Reuse stable scene item preparation across camera visibility changes and retain bounded state across independent families.
- [x] 3.2 Reuse packed instance records and independent compatible blocks across updates/views while preserving immutable GPU lifetime and budgets.
- [x] 3.3 Add visibility, object-only update, block reuse, custom collection/order and retirement regression coverage.

## 4. Measured completion

- [x] 4.1 Profile the remaining pipeline; optimize confirmed residual costs and record decisions about native submission/receipt work.
- [x] 4.2 Run final Debug/Release static, small/large motion, fixed-visibility and visible-UI A/B measurements and pixel/coverage checks.
- [x] 4.3 Complete appropriate builds, CTest, style/naming/boundary checks and independent review; fix confirmed in-scope findings and verify again as needed.
- [x] 4.4 Synchronize proposal/design/specs/tasks with delivered code, record complete evidence and limitations, validate OpenSpec and leave changes uncommitted for the initial review.
