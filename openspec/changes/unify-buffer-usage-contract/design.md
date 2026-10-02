## Context

At baseline `4da30568bb09e4f8ec60be596df7f822f77724b1`, `ERHIBufferUsage` declares Vertex=1, Index=2, Constant=4, StructuredRead=8, RawRead=16, StructuredWrite=32 and RawWrite=64. Public descriptors and immutable resource information carry `std::uint32_t` usage values. `BufferUsage` currently only converts one enum value.

RenderGraph uses 96 to identify writable buffers and 120 to restrict imports to shader resources. D3D12 uses 7 to reject storage/geometry-or-constant combinations and 127 to reject unknown usage bits. These sets have different meanings. Renderer also repeats material-source-to-RHI usage construction in compute declarations, graphics buffer reads and material GPU allocation. Each repetition currently agrees, but adding a bit can split those contracts.

The scope is M02 only. No application composition, plugin lifecycle or user-facing capability changes are needed. Current AGENTS.md and CodingStyle.md govern naming, including lowercase `b` prefixes for boolean variables.

## Goals / Non-Goals

**Goals:**

- Give RHI one adjacent, enum-derived definition of known usage membership and shader read/write categories with small constexpr composition/query helpers.
- Make each layer's supported subset and validation responsibilities explicit.
- Preserve all present acceptance/rejection behavior, including intentional differences between Graph state declaration and native resource access validation.
- Derive graph imports and native allocation from one Renderer material-source usage helper.
- Verify compatibility using independent fixed expectations and real native resource/compute regression paths.

**Non-Goals:**

- No new buffer usages, graph resource states, native backend or API-wide migration to a wrapper bitset.
- No global registry, enum reflection framework or macro-generation system for seven existing values.
- No changes to shader mapping, structured layout, material persistence, cache identity, allocation policy, constant publication or fence retirement.
- No attempt to make Graph validation as deep as D3D12 binding/access validation.

## Decisions

### RHI owns membership and basic queries

Keep `ERHIBufferUsage`, its numeric values, the existing `BufferUsage(ERHIBufferUsage)` spelling and all `uint32_t` fields. Add named constexpr usage sets composed from enum members and small type-aware composition/queries, including unknown-bit detection and shader-read/shader-write membership. Existing call sites can continue using `BufferUsage`; focused migrated code uses the shared definitions.

The known set is assembled from explicit enum members/categories rather than a numeric maximum or the ordinal position of the last enum. Category membership remains an explicit semantic decision next to the enum. Graph's allowed set is separately composed from its four supported enum values so a future RHI extension does not silently expand graph support.

This is preferred over a common final `ValidateBuffer` routine because RHI does not own Graph support or D3D12 heap/alignment constraints. A wrapper-type migration would broaden source compatibility work without improving this checkpoint's guarantee.

### Preserve layered acceptance

| Layer | Preserved acceptance and rejection |
| --- | --- |
| RHI foundation | Known bits and typed combination/query operations; creation/import callers reject zero and unknown bits |
| RenderGraph import | Nonempty subset of StructuredRead, RawRead, StructuredWrite, RawWrite; existing size/source/identity/alias rules |
| RenderGraph state | ShaderRead remains allowed for any valid graph import, including write-only usages; ShaderWrite requires at least one write bit |
| D3D12 create | Known nonempty usage, valid size/data; storage usages exclude Vertex/Index/Constant and require four-byte alignment; Constant is exclusive and requires 256-byte alignment |
| D3D12 access | ShaderRead access requires a read bit; ShaderWrite access requires a write bit and compute; existing range, view, ownership and simultaneous-access rules |

The graph's existing write-only ShaderRead behavior is exercised by `ComputeGraphTests.cpp` and remains compatible. A state declaration does not imply a buffer can be bound/read without the native access checks. `D3D12ResourceAccess.cpp` and `D3D12Draws.cpp` consume the shared read/write sets while retaining their separate checks. `D3D12PassAttachments.cpp` buffer transitions and `D3D12ResourceReadback.cpp` diagnostic readback also use the shared write set: both remain storage-only, and diagnostic readback continues to allow a write-only storage buffer independently of shader read-access requirements.

Numeric masks disappear from production usage validation. Fixed numbers remain appropriate in independent compatibility fixtures: the test oracle must not call the production helper whose correctness it is judging.

### Renderer owns material-source translation

Introduce a focused private Renderer helper that describes buffer usage for the current `FMaterialReadBufferSource`: read-only sources carry StructuredRead and RawRead; storage sources add StructuredWrite and RawWrite. `ComputePass`, `RenderPassDeclaration` and `MaterialGpuSources` use it for graph declarations and allocation. Materials keeps its current RHI-independent source interface.

The helper returns a value only. It does not cache, allocate, capture lifetimes, replace source identity or change resource generation. Source ownership, graph deferred resolution, RHI 0 preparation and GPU fence retention remain in their existing owners.

### Independent compatibility evidence

First add a focused regression matrix covering all values 0 through 127 and each unknown bit 7 through 31. Use fixed historical expectations for creation and Graph import rather than deriving expected results from production masks. Use a dynamic fake buffer for graph checks and the existing D3D12 device fixture for native creation/access checks.

At an aligned fixed size, Graph accepts the 15 nonempty subsets of its four shader bits. D3D12 acceptance additionally permits existing geometry/read combinations and the exclusive Constant case, while rejecting storage mixed with geometry/constant. Test size and initial-data restrictions separately so a coincidental failure cannot masquerade as correct usage rejection.

Add explicit graph cases for write-only ShaderRead, read-only ShaderWrite rejection, unsupported states, graphics writes, undefined contents and repeated-write barriers. Verify non-storage buffers remain rejected by native buffer transitions and diagnostic readback while storage inputs preserve their bytes. Preserve the existing compute GPU readback, graphics consumption and immutable constant-page tests. Verify readonly and storage material sources resolve to the same usage declared by compute and graphics graph imports.

Build existing targets before CTest in Debug and Release: `graph_tests`, `rhi_contract_tests`, `d3d12_device_tests`, `compute_rhi_tests`, `compute_renderer_tests`. Run `render_graph`, `rhi_backend_contracts`, `d3d12_device_ownership`, `compute_rhi` and `compute_rendering`, plus the normal style, naming, module-boundary, diff and OpenSpec checks. If implementation selects another existing target for a new focused fixture, list and build that target in the validation record.

## Risks / Trade-offs

- [Graph and backend sets are accidentally unified] → Freeze independent layer matrices before replacing masks and keep write-only state compatibility explicit.
- [A future enum addition enters Graph unintentionally] → Keep Graph's supported four-value subset explicit instead of equating it with all known RHI usages.
- [Material import/allocation metadata diverges] → Use one private helper in all three consumers and retain physical-description matching before preparation.
- [A GPU failure is counted as a usage rejection] → Separate pure/query, fake Graph and native tests; only count the expected validation exception for invalid usage, and require valid native cases and readback to execute.
- [Helper adoption changes resource lifetime or adds hot-path cost] → Use constexpr/value-only helpers; do not move allocation/ownership or add runtime logging and registries.
- [Windows build output permissions or desktop availability prevent validation] → Record the actual limitation and use approved normal-user execution; do not change ACLs, weaken tests or claim non-D3D12 native coverage.

## Migration Plan

1. Add and run the compatibility fixtures on the baseline.
2. Add RHI definitions and migrate the focused consumers and Renderer source helper in the same change.
3. Run affected Debug/Release builds and tests, complete independent review, and update `docs/RenderGraph.md` and `docs/ComputePipelines.md` with the layered contract.
4. Archive and commit only under the orchestrator's delivery authorization. No persisted format migration is required. Rollback is a source revert because enum values, public storage and resource behavior remain stable.

## Open Questions

None that block implementation. Final helper names and whether a small helper resides in an existing or new private header are local implementation choices; the ownership and compatibility requirements above constrain both.
