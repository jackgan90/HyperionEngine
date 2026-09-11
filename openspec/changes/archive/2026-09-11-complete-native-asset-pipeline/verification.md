# Implementation verification

Verified on 2026-09-11 in F:/HyperionEngine. The change is implemented and left uncommitted.

## Delivered behavior

- Reflection now supplies checked type registration, persistent/required/aliased fields, recursive dependency visitation, optional/map/unsigned/enum support, explicit adjacent migrations and transactional reads.
- HYPA v2 uses fixed little-endian tags and indexed bulk ranges. HAST native envelopes check integrity, identity, type/schema, revision and reflected dependency metadata. Legacy HYPA reading remains available; writes use the current format.
- Assets loads registered native types with typed and untyped APIs, portable references/catalogs, shared dependency loading, bounded completed retention, isolated cancellation and ordered save/load revisions.
- AssetImport owns glTF/GLB and scene JSON conversion. AssetTool imports, validates, inspects, creates catalogs and upgrades legacy records. Source fingerprints/settings/importer versions drive incremental checks. Immutable dependency generations precede atomic root publication, with exclusive local publication leases.
- Viewer no longer links AssetImport. Native build content preserves Showcase's 79 instances and Shadows' 10 instances. Current scene snapshots persist instance identity, matrix, visibility, simple material overrides and camera, including path rebasing for Save As.

## Environment and completed checks

Windows x64; Ninja; Visual Studio 18 Build Tools, MSVC 19.50 (compiler path 14.50.35717); Python 3.14.2; LLVM 22.1.1. Both Debug and Release were built. RenderDoc was enabled, Tracy/profiling compiled out. GPU acceptance used D3D12 with normal validation settings; no validation or lifetime checks were disabled.

| Check | Result | Evidence under out/NativeAssetChecks |
| --- | --- | --- |
| Full Debug build and CTest | 59/59, 277.61 s | DebugFinalVerification.log, DebugFullLastTest.log |
| Full Release build and hyperion_check | 59/59, 149.03 s | ReleaseFinalBuild.log, ReleaseFinalVerification.log, ReleaseFullLastTest.log |
| Final AssetTool rebuild + native management/publication/fixture selection, Debug | 4/4, 3.31 s | DebugDeliveryBuild.log, DebugDeliveryTests.log |
| Same final selection, Release | 4/4, 2.84 s | ReleaseDeliveryBuild.log, ReleaseDeliveryTests.log |
| Formatting and source paths | 397 files passed | CheckStyle.py |
| Semantic C++ naming and declarations | 244 translation units passed | NamingFinal.log |
| Module boundaries | 378 sources / 27 modules passed | CheckBoundaries.py |
| OpenSpec all strict | 45/45 passed | openspec validate --all --strict |
| Whitespace and final candidate consistency | passed, no staged changes | git diff --check, DeliveryCandidates.json |

The only C++ change after the complete suites was explicit final stdout flushing in AssetTool, followed by formatting, both builds, the targeted tests above and fresh measurements. The final CMake test dependencies/work-directory changes were included in Release hyperion_check and the subsequent Debug configure/targeted run.

## Coverage and observations

- Archive/record tests cover required/default/alias conflicts, nested diagnostic paths, failed transactional reads, explicit migrations, invalid type registration, enums, full unsigned values, fixed scalar wire bytes, owner-backed bulk lifetime, malformed ranges and legacy decoding.
- Native management tests use an additional registered fixture type. They check checked casts, payload corruption, forged type/dependency metadata with recomputed outer hash, hash equivalence, size limits, relative/catalog references, relocated graphs, one-Worker cycles, sharing, retry, cancellation, cache retention and save/load ordering.
- Publication tests cover generic recursive dependencies, conversion deduplication, external binary and real PNG changes, preserved output IDs, changed revisions, unchanged-import skips, source/child-write failures preserving the previous graph, one-Worker cycles, ordered publications, exclusive filesystem leases, model-to-scene conversion and legacy native upgrade.
- Importer tests retain glTF geometry/accessor/sparse/stride/normalized/topology/image/hierarchy validation. Runtime model and scene GPU tests use a filesystem that rejects all non-.hasset reads and assert zero attempted source reads.
- SceneViewer editing tests cover hide/move/duplicate/remove, non-TRS affine matrix persistence, stable IDs, simple material overrides, unsupported-state rejection, camera persistence, Save As references and real GPU readback after reload. In both Debug and Release, 1,228,800 compared components had maximum difference 0.
- Scene acceptance covers 514 instances across no/linear/BVH culling with identical rendered images, GUI, 514/515 ready after removing one native dependency, and rejected invalid reimport without changing the published root.
- Existing full-suite rendering checks include moving camera, instancing, materials, deferred/forward rendering, depth conventions, cascaded shadows, frame ownership/pipeline, offline startup and RenderDoc lifecycle.
- A Unicode source/output path import and native validation probe passed under out/NativeAssetChecks/Unicode.

Additional direct Debug Viewer runs used 180 frames, hidden window, no UI and --verify-model:
1. Showcase: 79/79 ready, zero failures and zero D3D12 validation errors; saved through --save-scene.
2. Shadows: 10/10 ready, zero failures and zero D3D12 validation errors.
3. Reloaded saved Showcase: 79/79 ready and zero validation errors; PNG bytes exactly matched the first run.

Logs, captures and the result summary are in out/NativeAssetChecks/SampleAcceptance. Showcase's generated capture was also visually inspected.

## CPU loading measurements

Run tools/MeasureAssets.py with the corresponding Debug/Release hyperion_asset_tool.exe, assets/Models/Showcase.gltf and a separate output directory. Each mode uses 2 warmups and 7 measured fresh processes. The OS file cache is warmed, not flushed. The model has 4 primitives and 4 node instances; source readiness and native graph validity are required, and source fingerprints are rechecked afterwards.

The final measurement summaries record executable SHA-256 and all individual samples. No compiler, test suite or GPU acceptance run was active during these samples.

| Build / mode | Median elapsed ms | Reads | Bytes read | Median process peak resident bytes |
| --- | ---: | ---: | ---: | ---: |
| Debug source conversion | 21.6792 | 3 | 73,932 | 13,774,848 |
| Debug native loading | 9.6257 | 1 | 202,462 | 12,386,304 |
| Release source conversion | 4.1706 | 3 | 73,932 | 9,302,016 |
| Release native loading | 2.1136 | 1 | 202,462 | 8,851,456 |

Raw logs and summaries: out/NativeAssetChecks/DebugMeasurements and ReleaseMeasurements. Native loading was faster in this small sample and used one read, while reading about 2.74 times as many bytes because decoded pixel/numeric data is stored without compression. These are CPU conversion/loading measurements, including validation and tool command work, not GPU-upload or frame-time benchmarks. Peak residency describes the complete short process, not cache ownership or a memory ceiling. Timing varies across runs and is not a general large-asset performance guarantee.

During initial Debug measurement, some successful tool exits produced empty captured output (8/100 in a bounded probe). Explicit flushing of the final output eliminated this reproduction (0/100); both final measurement runs also completed without missing counters. Probe results are ToolOutputProbe.json and ToolFlushProbe.json. The evidence establishes the output fix in this environment; it does not attribute the underlying shutdown-buffering behavior to a particular third-party component.

## Remaining documented boundaries

- Legacy v1 compatibility is exercised with bounded generated legacy records; there is no external archive corpus certification.
- Tests cover configured limits and corrupt data but do not claim exhaustive fuzzing, OS/process termination at every publication boundary or arbitrary custom validator allocation control.
- Cache weights are conservative stored-size estimates. Default file/decode budgets and graph limits are documented in docs/NativeAssets.md; they are not a process-wide hard memory limit.
- Old immutable generations remain until explicitly cleaned by a content owner. There is no automatic garbage collection, streaming/compression, hot reload, independent texture/material assets or GPU binary serialization.
- Path normalization is lexical; case/symlink aliases are not collapsed. Cross-volume Save As can require absolute references. Directory portability requires moving the referenced content together.
- Scene save rejects source-less attachments and generic runtime material selections without a persistent representation. Existing simple overrides are persisted.

Usage and contracts are documented in docs/NativeAssets.md, docs/AssetPipeline.md and the updated scene/build/source-layout documentation.

## Follow-up quality audit

The user requested independent review and fixes after this implementation verification. The audit baseline is commit 4a86215ff57fb62f0df2d9c379c291668a3182e4 plus the 126-file uncommitted snapshot in out/NativeAssetAudit/ReviewSnapshot.json. The independent report is out/NativeAssetAudit/IndependentReview.md. Four P2 findings and one P3 finding were confirmed: source reference constraints during native import, cross-volume source identity, draining admitted dependency graphs, conflicting callable bindings, and native envelope size budgeting. Targeted regression tests exercise each failure. Re-review identified and closed RR-01: current and legacy revision hashing now preserve caller-supplied archive limits. All six confirmed findings are closed by independent re-review. See [audit.md](audit.md) for the frozen snapshots, per-finding evidence, full Debug/Release 59/59 results before RR-01, and final builds plus nine affected regression cases in each configuration after RR-01. The measurements above describe the pre-audit binaries whose hashes are retained in their original summaries.
