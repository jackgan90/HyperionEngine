## 1. Baseline and named status contract

- [x] 1.1 Verify current status/material meanings and capture passing SceneBridge/SceneInstance baseline builds and tests.
- [x] 1.2 Introduce the named bridge status snapshot and migrate the model-status cache without changing counters or equality inputs.

## 2. Material dependency records

- [x] 2.1 Replace the mixed version stream and editable dependency pairs with named records, preserving freeze, prepare, commit and removal behavior.
- [x] 2.2 Add behavioral regressions for status components, idle reuse, material revisions and section/dependency changes; document the contract.

## 3. Validation and acceptance

- [x] 3.1 Build affected consumers and pass focused Renderer/SceneInstance regressions, style, naming, boundary and strict OpenSpec checks.
- [x] 3.2 Complete independent quality audit, verify and minimally repair findings with targeted re-review as needed, then stop for user diff acceptance.
