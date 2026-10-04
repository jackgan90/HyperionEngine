## 1. Baseline and discrete choice contract

- [x] 1.1 Capture current API/type contract snapshots from a verified baseline build.
- [x] 1.2 Add Renderer-owned culling/outline identity, wire and presentation mappings with strict conversions.

## 2. Service and GUI integration

- [x] 2.1 Route shared viewport validation and Editor state conversion through the mappings without changing schemas or error precedence.
- [x] 2.2 Replace combo-index assumptions with identity lookup and preserve the popup's existing controls and service call.

## 3. Regression and delivery

- [x] 3.1 Add fixed-value, reordered/relabeled, invalid-input and external-shape regressions; verify actual GUI and automation choices preserve unrelated state.
- [x] 3.2 Build affected targets, compare baseline contracts, pass focused CPU/GUI/automation, style, boundary and OpenSpec checks, and update developer guidance.
- [x] 3.3 Complete independent quality audit, independently verify and repair findings, obtain targeted re-review, and stop for user diff acceptance.
