## 1. Shared domain and Automation

- [x] 1.1 Implement selected-root KeepWorld batch preparation and atomic commit with unchanged-state rejection/no-op behavior.
- [x] 1.2 Reflect and register scene.nodes.reparent while preserving the single-node contract; cover discovery, transforms, history and failures.

## 2. Editor gestures

- [x] 2.1 Add engine-owned GUI gesture/target helpers and selection-preserving Outliner drag sources, including search results.
- [x] 2.2 Restrict reparent sources to Outliner, preserve synchronized viewport selection and Gizmo/picking/navigation behavior, and retain transient snapshot cancellation.
- [x] 2.3 Add parent/root targets, feedback, hover expansion, scrolling and deferred shared commit; remove Details Hierarchy.

## 3. Verification and documentation

- [x] 3.1 Add real Editor input acceptance for Outliner-only sources, viewport selection and rejected viewport initiation, multi-selection, internal hierarchy, cancellation/rejection, history and save/reopen.
- [x] 3.2 Update Editor and Automation documentation for the new interaction and operation.
- [x] 3.3 Complete affected builds, CPU/GUI/Editor/Automation regressions, style/naming/boundaries and strict OpenSpec validation; record evidence without archive or commit.
