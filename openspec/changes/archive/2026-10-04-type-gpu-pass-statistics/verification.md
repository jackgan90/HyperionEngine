# GPU pass statistics verification

## Baseline and scope

- Baseline: `afdb45e` on `main`; clean worktree before this change. The earlier material execution and import-rule work is already committed and remains outside this change.
- Existing Debug `render_controls` and `render_graph`: 2/2 passed, 1.84 s, after rebuilding both targets. The checked-in diagnostic JSON and 138-column CSV fixture are the compatibility oracle and are not regenerated.
- Raw logs: `out/maintainability/GpuPassStatistics/` (ignored output).

## Contract review

- Renderer owns timing categories and category-local instances. RHI owns only opaque Domain/Value words. Graph graphics splitting copies the tag to every batch; compute adaptation and both compile modes preserve it.
- D3D12 uses the already-retained immutable `FPassCommands` for collection. No new native metadata cache, registry, allocation, fence, wait, log or profiling enablement path is introduced.
- Built-in producers explicitly tag CSM, legacy/HDR Forward, Deferred base/compatibility/lighting/local lights, transparent, tonemap, sky, HZB and contact masks. Debug, legacy display, GUI and graph exports remain unclassified.
- Target equality includes timing metadata; native PSO/draw plans still depend only on rendering inputs. The per-submission command keeps its own tag.
- CSV projection uses named fields from one typed aggregation. Category totals, cascade bounds, unknown/default/foreign tags and exact existing CSV output are covered independently of display names.
- No reflected fields, asset formats, CLI flags, automation operation IDs or diagnostic wire schemas change. This introduces no new user-facing operation requiring an adapter.

## Verification results

- Debug and Release builds succeeded for `graph_tests`, `render_control_tests`, `compute_rhi_tests`, `shadow_render_tests`, `hierarchical_depth_tests`, `deferred_render_tests`, `sky_render_tests`, `contact_shadow_tests`, `d3d12_frame_failure_tests`, `rhi_contract_tests` and `hyperion_editor`. MSVC 14.50.35717, Ninja, BUILD_TESTING=ON, Tracy=OFF in both; RenderDoc=ON in Debug and OFF in Release. No compiler warning/error matches in the selected build logs.
- Final full formatting/path check: 1121 owned source files passed; module boundaries: 1082 source files / 39 modules passed. Semantic naming/declaration checks passed for all 23 changed translation units, followed by passing checks of the final Compute assertion and two GPU fixture edits.
- Strict OpenSpec validation: 110/110 passed; final diff and active-change checks also pass.
- Initial Debug regression: 9/10 passed. The new Compute test incorrectly equated acquisition frame identity with successful submission count despite a prior cancelled frame in its existing fixture. Corrected only that assertion to check nonzero identity/owner and correspondence with the latest completed timing. Production code was unchanged. Test-only enum aliases rejected by the naming rule were also replaced with full type names.
- Debug full targeted run: 10/10 passed, 79.25 s (`DebugFinalTests.log`). Release's first run passed 9/10 in 31.47 s; the new sky assertion could read an empty latest timing because screenshot readback precedes the final submission fence. Sky and Deferred test fixtures now explicitly wait for completion before asserting metadata. This changes tests only; production adds no wait.
- Final affected-suite reruns: Debug 2/2 passed in 67.11 s and Release 2/2 passed in 19.67 s (`DebugTimingFixtureTests.log`, `ReleaseTimingFixtureTests.log`). Thus all ten selected suites have passing results on their final sources in both configurations; this does not claim a single new Release 10/10 invocation after the fixture-only edit.
- Native validation used NVIDIA GeForce RTX 5080 with the D3D12 debug layer enabled. Existing test assertions verify zero validation errors; this is not a claim that the test runs emit no warnings or intentional failure-injection diagnostics.
- `VerifiedSource.json` records the 31 modified/new source/build files; all hashes match the validated source. The original diagnostic fixtures are unchanged. HEAD remains `afdb45e05dd3f99890bcc4d964d7a0baf0ef56fe`, with no staged files, archive, commit or push.

The full targeted command for each build directory is:

```powershell
ctest --test-dir out/build/debug -R '^(render_controls|render_graph|compute_rhi|cascaded_shadow_rendering|hierarchical_depth|deferred_rendering|sky_rendering|contact_shadows|d3d12_frame_failure_recovery|rhi_backend_contracts)$' --output-on-failure
ctest --test-dir out/build/release -R '^(render_controls|render_graph|compute_rhi|cascaded_shadow_rendering|hierarchical_depth|deferred_rendering|sky_rendering|contact_shadows|d3d12_frame_failure_recovery|rhi_backend_contracts)$' --output-on-failure
```

After the final test-only synchronization change, both affected targets were rebuilt and `sky_rendering|deferred_rendering` rerun in both configurations; other production/test sources remain unchanged from their passing runs.

## Coverage and manual review

- Existing JSON/CSV oracles remain unchanged. `ViewDiagnosticsTests` attaches explicit fixture categories, checks all 138 columns and existing bytes, then renames every pass (including misleading reserved prefixes on unclassified passes) and checks byte-identical output.
- `RenderPassTimingTests` covers default/foreign/unknown tags, invalid typed categories, maximum instance round-trip, out-of-range cascades, split batches, immediate/deferred preparation, copying/consuming graph compilation, and timing-aware target equality.
- Real GPU checks cover arbitrary opaque tags on individual and 25-pass batched RHI recording, tagged shadow cascades, every HZB mip, and the Forward/Deferred/lighting/sky/contact/local-light producer paths exercised by the existing rendering suites. An independent test-only label oracle checks built-in producer assignments; production aggregation has no label parsing.
- The existing D3D12 gate test now mutates caller tags after submitting blocked GPU work and verifies the retained original tags after fence release, across capture epoch boundaries and ordinary completion. Existing cancellation/recovery, capacity/drop, invalid submission and exactly-once checks remain enabled.
- All changed production C++ files are below 500 physical lines (largest: `RHITypes.h`, 422). Modified/new functions are below 100 lines, including graph preparation, declaration, aggregation and CSV writing. The large backend failure-test and renderer integration-test files are actual test targets; additions remain in focused test functions. No unrelated production file splitting was necessary.
- Semantic review checked typed identity versus display text, one aggregation owner, named totals, explicit category/instance encoding, and metadata lifetime/cache invalidation. RHI/native files have no Renderer enum dependency; only test code retains label-based migration assertions.

## Limits

This is a structural refactor, not a measured performance improvement. Native GPU verification targets D3D12 on the available Windows device; no Vulkan/Metal backend or full-suite result is implied. OpenSpec archive and Git commit are not part of this implementation turn.

## Independent quality audit

- Two reviewers started without inherited conversation context and independently inspected the frozen 40-file workspace against `afdb45e05dd3f99890bcc4d964d7a0baf0ef56fe`. Their scopes covered metadata ownership/propagation/native completion and producer completeness/aggregation/compatibility respectively. Both returned no confirmed defects or unresolved defect hypotheses. The main agent independently verified the relevant source paths; no production or test repair was required.
- Main-agent verification included retained per-logical-pass commands, split/compute and copy/consume compilation, metadata-aware target equality, copy-on-write snapshots, fence/capture/cancellation behavior, producer assignments, CSV projection and the unchanged diagnostic DTO. All 40 file hashes matched the frozen snapshot throughout source review; only this verification record and the audit task were updated afterward.
- Fresh coordinated audit runs passed `render_controls`, `render_graph`, `compute_rhi` and `d3d12_frame_failure_recovery`: Debug 4/4 in 2.37 s and Release 4/4 in 2.05 s. The main agent executed these tests serially; reviewers performed independent static reviews and did not separately launch builds or GPU tests. Source files remain identical to the previously built and validated implementation.
- Optional coverage improvement recorded by the statistics reviewer: the cache-specific regression directly checks target inequality, but does not execute a complete retained-session sequence that changes only Timing while keeping an older graph alive. The main agent verified full-target equality, copy-on-write, current-target assignment and declaration from the current snapshot. This is not a confirmed defect or a required repair.
- Audit report, reviewer reports, snapshots and raw test logs are in `out/maintainability/AuditGpuPassStatistics/`. The audit does not claim a new full-suite run, live GUI/automation run, actual device removal test, other native backends, or a performance result. The change remains active and uncommitted.
