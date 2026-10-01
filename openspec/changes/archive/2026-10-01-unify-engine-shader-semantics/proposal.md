## Why

Engine material inputs are identified by repeated string literals, and builtin shader resource mappings are separately enumerated by material factories. This makes typos and contract drift difficult to detect and requires unrelated materials to carry feature declarations.

The implemented owner-local declarations still expose visitor conditionals, repeat numeric fields as semantics and uniform members, and require hand-written offsets and block sizes. The accepted refinement makes each field readable at its declaration site and derives the GPU layout from its type and order.

## What Changes

- Introduce a shared macro declaration mechanism for builtin semantic identities, types, scopes, versioned uniform layouts and resource contracts.
- Refine ownership: keep common globals in EngineSemantics.inl and declare rendering-domain uniforms/resources beside their owners using the same macro mechanism, separate domain enums and explicit immutable contract sets.
- Use strongly typed builtin identities in CPU declarations, provider lookup and input publication; retain named custom semantics and stable asset wire names.
- Discover engine resource contracts from reflected shader resource names, validate only encountered resources, and build bindings without per-material builtin mapping lists.
- Share immutable builtin catalogs, and preserve owned immutable extension snapshots.
- Use the same contract validation for graphics, fullscreen and compute; retain target-aware packing, scope caching and fence ownership.
- Add regression coverage and update material documentation. Leave the change active and uncommitted after validation.
- Generate typed shader parameter structures and shader-only declarations from the catalog; migrate builtin pass inputs, resources and instance declarations to enum/structure interfaces without a header-processing tool.
- Preserve semantic IDs and schema handles through runtime publication, snapshots and fullscreen/compute submission rather than converting typed inputs back to names. Retain names only at authored asset, custom shader and reflection boundaries.
- Replace the declaration frontend with readable contract, uniform, field and resource macros. Declare each numeric field once with its shader type, name and scope; hide visitor conditionals and omit routine default policy metadata.
- Infer uniform offsets, matrix/array strides and total size from an explicit supported GPU packing profile. Keep expected layout independent from native reflection and C++ object layout, and preserve exceptional fixed ABIs through explicit compatibility metadata.
- Expose generated shader structures for explicit owner-authored cbuffer declarations. Preserve common flat declarations, stable wire names and existing layouts where compatibility requires them, and verify formatter stability.

## Capabilities

### New Capabilities
- `engine-shader-semantics`: Central common declarations, owner-local typed contracts and demand-driven reflected resource validation.

### Modified Capabilities
- `material-parameter-binding`: Typed semantic identities, shared catalog snapshots and automatically discovered builtin bindings.
- `shader-pipeline`: Structured resource element reflection needed for complete engine contract validation.

## Impact

Runtime/Materials, Runtime/Shaders adapters and cache metadata, Runtime/Renderer parameter preparation and builtin producers, Content/Shaders, material/shader/render tests and docs/Materials.md. Existing asset field names, plugin lifecycle, RHI interfaces, application composition and editor/automation operations remain stable.
