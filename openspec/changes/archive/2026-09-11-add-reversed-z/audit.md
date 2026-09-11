# Reversed-Z quality audit

## Reviewed version and scope

- Baseline: `197d997f5b4884ed80c2d7e7ecc6313a4de27d29`.
- Initial working-tree snapshot: 70 changed/new files, `out/ReversedZAuditSnapshot.json`, SHA256 `0dcfd2923669cc21c9b61810c37cd23acd4e30b4ad8806b50adba9d32505e57b`.
- Fixed snapshot: 75 changed/new files, `out/ReversedZAuditFixedSnapshot.json`, SHA256 `cfb63465407740ebfcffc50323d457635e0c90e74433604180a1fc5e444ae622`.
- No unrelated pre-existing changes were included. Source files stayed frozen during each review. This report and the final verification addendum were written after review.
- The independent reviewer received a self-contained brief without implementation conversation history. Scope included the startup configuration, Math/Materials/Renderer/D3D12 paths, all application integrations, caches, CSM, tests and OpenSpec documents. The user requested auditing and fixing this development without additional special focus.

## RZ-01 — P2 — Depth remapping incorrectly changed geometric facing

**Confirmed and fixed.** The original Triangle update multiplied `ClipDepthTransform(Reversed)` into its clip-space primitive World. The matrix leaves screen X/Y/W unchanged but has determinant -1. Ordinary and instanced material preparation consequently treated it as a geometric mirror and flipped FrontCounterClockwise. Cull=None hid the default application's impact; enabled face culling or distinct front/back stencil behavior would select the wrong face.

Independent probe linked against the original libraries: screen area remained 2.1, while World determinant changed +1 to -1 and frontCCW changed 1 to 0. The main agent reproduced the output and traced the two draw paths into material state conversion.

The minimal fix preserves geometric World and defines bClipSpace input as canonical Standard clip depth. Renderer applies PrimitiveClipTransform for WVP, culling and transparent sort depth. Triangle now supplies only geometric scale. The view material scope tracks DepthConvention so unchanged matrices/revisions cannot retain stale WVP provider values. Raw shader outputs bypassing the engine semantic remain the caller's responsibility.

New ClipSpace GPU coverage exercises ordinary and verified instance batches; Back/Front/None culling and SV_IsFrontFace; positive/negative geometric scale and OrientationSign; near/far overlap, transparency and out-of-range clipping; and same-session convention changes with unchanged matrices and primitive revisions. This checks the facing predicate used by front/back stencil selection; it does not add a new GPU stencil attachment test.

## Independent re-review

The original reviewer checked the fixed snapshot and closed RZ-01, with no newly confirmed defect in its direct impact. A second probe linked to the fixed Renderer preserves area and frontCCW across conventions for both positive and negative X scale; depth alone changes .25 to .75. The main agent also executed this probe with the Debug runtime dependencies.

Both snapshot file sets/hashes matched during independent review. The reviewer independently ran CPU depth/shadow tests and diff checks, read the new GPU tests/results, and did not independently rerun GPU or full CTest suites. Full GPU/build validation is performed by the main agent below. This is a bounded audit, not a claim of absence of all possible defects.

## Validation

- Targeted Debug `deferred_rendering`, including the new clip-space cases: passed, 7.61 seconds (`out/ReversedZAuditTargeted.log`).
- Format/path checks: 360 source files passed. Boundaries: 341 source files, 25 modules passed. OpenSpec strict validation and `git diff --check` passed.
- Full post-fix Debug CTest: **56/56 passed**, 170.19 seconds (`out/ReversedZAuditFullDebug.log`).
- Full post-fix Release build/CTest: **56/56 passed**, 109.04 seconds (`out/ReversedZAuditBuildRelease.log`, `out/ReversedZAuditFullRelease.log`).
- Semantic naming/local-declaration checks: **222 translation units passed** (`out/ReversedZAuditNaming.log`). Final OpenSpec strict and whitespace checks passed; no confirmed finding remains open.
