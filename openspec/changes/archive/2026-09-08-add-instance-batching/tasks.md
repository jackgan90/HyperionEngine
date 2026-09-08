## 1. Shader and RHI contracts

- [x] 1.1 Add HYP_ENABLE_INSTANCE 0/1 permutations, authored instance-array declarations, reflected record mappings and generic typed packing with invalid-contract tests.
- [x] 1.2 Add RHI instance counts/capabilities and compact constant-extent validation with native boundary tests.
- [x] 1.3 Migrate Model, Triangle and GUI shaders/consumers to macro-selected system instance IDs while preserving ordinary rendering.

## 2. Render batch planning and caching

- [x] 2.1 Implement the generic coordinator/strategy interface, complete compatibility and ordering barriers with exclusive coverage tests.
- [x] 2.2 Implement stable item identification, compact chunk data reuse, budgets and visibility/value invalidation tests.
- [x] 2.3 Integrate Render plans into session snapshots and RHI materialization, including GPU slice reuse and whole-group failure publication.

## 3. Viewer and validation

- [x] 3.1 Enable default Scene Viewer instancing, runtime/CLI comparison controls and meaningful batch/cache/timing diagnostics.
- [x] 3.2 Add real GPU correctness tests for custom typed instance data, numeric overrides, resource/state splits, capacity, multiview, failures and retained-frame lifetime.
- [x] 3.3 Update scene integration checks to verify equal images/coverage and reduced actual draws, including moving-camera cache behavior.
- [x] 3.4 Complete Debug/Release builds, appropriate full regression suites, style/naming/boundary checks and strict OpenSpec validation.

## 4. Performance and delivery

- [x] 4.1 Run repeated serial warmed static/moving A/B measurements and collect CPU/GPU scope evidence with cache/upload counters.
- [x] 4.2 Document interfaces, measured results, limits and reproduction commands; reconcile all task evidence and final workspace diff.
