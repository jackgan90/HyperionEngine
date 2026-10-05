## 1. Configured dependency evidence

- [x] 1.1 Collect executed CMake target/source/link declarations with visibility, locations and freshness checks; integrate supported configure entry points.
- [x] 1.2 Implement per-target direct/public checks, production cycles and CPU domain constraints while preserving test/private/SDK boundaries.

## 2. Regression and contracts

- [x] 2.1 Add adversarial checker fixtures and compile consumers linking only the owning public target; verify and correct exposed declarations individually.
- [x] 2.2 Document graph coverage and the current architecture ownership overview.

## 3. Validation and acceptance preparation

- [x] 3.1 Configure, compile and test affected Debug/Release and build selections; record concrete results.
- [x] 3.2 Independently audit all unreviewed work with quality-audit, verify/fix findings and re-review before the Change 2 human gate.
