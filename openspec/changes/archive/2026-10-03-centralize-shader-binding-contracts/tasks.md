## 1. Baseline

- [x] 1.1 Record existing material, RHI and compute test behavior and confirm contract owners and compatibility boundaries.

## 2. Implementation

- [x] 2.1 Introduce typed shader stages and explicit RHI visibility conversions; migrate producers, merge and stage accounting.
- [x] 2.2 Move common resource translation and pipeline-binding validation into RHI; retain native validation entry points and restrictions.
- [x] 2.3 Add independent stage/resource mapping and layout-rejection tests and document ownership.

## 3. Validation

- [x] 3.1 Build Debug/Release and run affected public-contract, shader/material, graphics and compute tests.
- [x] 3.2 Run format, naming, paths, dependency and OpenSpec validation; review compatibility and scope, record results without archiving or committing.

## Verification

See [verification.md](verification.md). Implementation and verification are complete. Archive and commit were separately authorized on 2026-10-03.
