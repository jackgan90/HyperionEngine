## 1. Freeze compatibility evidence

- [x] 1.1 Record the implementation baseline, inspect current buffer usage consumers and confirm clean ownership of shared CMake/build paths with the orchestrator.
- [x] 1.2 Add independent fixed-expectation tests for all existing seven-bit combinations, zero and every unknown high bit across RHI membership, Graph import/state and D3D12 creation; verify them against baseline behavior before replacing production masks.
- [x] 1.3 Add explicit regression cases for Graph write-only ShaderRead compatibility, read-only ShaderWrite rejection, native read-bit requirements, storage/constant alignment and initial-data size; record baseline results separately from unrelated device failures.

## 2. Establish shared definitions and migrate consumers

- [x] 2.1 Add focused enum-derived RHI known/read/write sets and constexpr typed composition/query helpers, preserving enum values, BufferUsage compatibility and public uint32_t usage storage.
- [x] 2.2 Express Graph's explicit supported four-value subset and state checks using the foundation; preserve source/identity/alias, initialization, full-overwrite and barrier behavior.
- [x] 2.3 Migrate D3D12 creation, resource-access, storage-binding, transition and readback checks to shared membership queries while retaining native allocation, alignment, view, device and compute restrictions.
- [x] 2.4 Add one private Renderer material-source usage helper and consume it from compute imports, graphics buffer-read imports and native material GPU allocation without moving ownership or changing source/cache identity.

## 3. Verify integration and document the contract

- [x] 3.1 Verify readonly/storage material sources produce matching graph/native descriptions through compute and graphics paths; retain expected GPU output, source immutability, constant-page retention and recovery behavior.
- [x] 3.2 Build graph_tests, rhi_contract_tests, d3d12_device_tests, compute_rhi_tests and compute_renderer_tests in Debug and Release, including any additional focused fixture target selected during implementation.
- [x] 3.3 Run render_graph, rhi_backend_contracts, d3d12_device_ownership, compute_rhi and compute_rendering in both configurations; record commands, results and native coverage limits.
- [x] 3.4 Update docs/RenderGraph.md and docs/ComputePipelines.md to explain RHI membership, graph support and native validation differences, including write-only state compatibility.
- [x] 3.5 Run appropriate format/path/naming checks, CheckBoundaries.py, git diff --check and strict OpenSpec validation on the final implementation; record any environmental limitations accurately.

## 4. Independent review and handoff

- [x] 4.1 Obtain the orchestrator's independent review of final source/specs and verify each finding directly; address confirmed M02 issues and repeat only affected validation.
- [x] 4.2 Provide the final implementation/validation snapshot and remaining limitations to the orchestrator for authorized ledger update, archival and commit; do not push.
