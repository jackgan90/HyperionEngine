# Compute pipelines

Compute is a first-class shader stage alongside vertex/pixel. The Shaders module compiles `EShaderStage::Compute` to DXIL, SPIR-V and MSL, and reflects constant members, texture/sampler arrays, structured/raw SRVs and UAVs, and `ThreadGroupSize`. Shader-cache reflection versioning separates old artifacts. D3D12 is the implemented runtime backend; SPIR-V/MSL compilation does not imply Vulkan/Metal execution support.

## Renderer authoring

Publish `FComputePassDesc` from the Render domain through `AddComputePass` or `FRenderSession::AppendCompute`. The description owns its shader path/entry/options and numeric parameter values. Parameter names are `ConstantBuffer.Member`; texture, buffer and sampler names match reflection. Texture/buffer arrays use `ArrayIndex`. Unused, missing, duplicate or type-incompatible inputs are rejected. `Extent` is a thread extent, rounded up using the shader's reflected `numthreads`; the shader must guard threads outside the extent.

```cpp
const auto Scope = Session.GetResources().CreateScopeLifetime();
const auto Output = std::make_shared<const FMaterialTextureSource>(
    FMaterialStorageTexture{Width, Height, 1, EMaterialColorFormat::R32Float});
FComputePassDesc Pass;
Pass.Name = "Example/Compute";
Pass.Shader.Source = "Example/Compute.hlsl";
Pass.Shader.Entry = "CSMain";
Pass.Lifetime = Scope;
Pass.Extent = {Width, Height, 1};
Pass.Parameters = {{"Parameters.Width", FMaterialValue::Uint(Width)},
                   {"Parameters.Height", FMaterialValue::Uint(Height)},
                   {"Parameters.Gain", FMaterialValue::Float(Gain)}};
Pass.Textures = {{"Output", Output, 0, 1, EResourceState::ShaderWrite, true, false}};
AddComputePass(Session, Graph, std::move(Pass));
```

```hlsl
cbuffer Parameters : register(b0)
{
    uint Width;
    uint Height;
    float Gain;
};
RWTexture2D<float> Output : register(u0);
[numthreads(8, 8, 1)]
void CSMain(uint3 InId : SV_DispatchThreadID)
{
    if (InId.x < Width && InId.y < Height)
    {
        Output[InId.xy] = Gain;
    }
}
```

`FMaterialReadBufferSource(Size)` creates an engine-owned GPU storage source. `FMaterialBufferView` specifies structured/raw layout and byte range. Compute and graphics use the same source cache, binding layouts/sets, constant packing and page allocator. Graphics declares generated texture reads through `FRenderPassTargets::Reads` and storage buffer reads through `BufferReads`. This creates dependencies before native preparation; simply putting a UAV-produced resource in material parameters is insufficient to describe an undeclared buffer dependency.

Storage starts undefined. `bFullOverwrite` promises that the pass initializes the entire declared mip or physical buffer; arbitrary dispatches do not prove this. Partial or accumulating writes require previously initialized contents. `bInitialized` is an explicit import promise for a product initialized before the current graph, with a retained source and scope; all imports of that resource in one graph must agree. CPU-uploaded sources are initialized automatically and must finish asynchronous upload before compute consumes them. Generated resources return to ShaderRead at graph export.

Published descriptions freeze parameter values and retain immutable sources/scopes. Changing later CPU values, shader selection, resource identity or mip range produces the corresponding next-frame bindings. Native programs, layouts, PSOs and descriptors reuse complete compatible identities; constant byte reuse has a bounded history. Scope retirement and existing submission fences protect queued work.

## RHI and graph contracts

`FComputePipelineDesc` contains the compute artifact and binding layout. `FDispatchPacket` contains pipeline, resource set, constant slices and group counts. Direct RHI callers declare a compute-only `FPassCommands` with texture mip/buffer byte accesses and required transitions. Compute cannot carry graphics attachments, viewport or draw packets. D3D12 validates device ownership, stage/layout, numthreads/group limits, descriptor ranges, resource usage and declared accesses before recording.

`FTextureView` is a contiguous mip SRV or single-mip UAV. Supported storage formats are queried through `FRHICapabilities::StorageTextures`. Structured/raw storage buffers use default heaps and corresponding read/write usage flags. Append/consume/counter UAVs, texture arrays/3D UAVs, indirect dispatch and async compute queues are outside this implementation.

`FRenderGraph::AddCompute` shares graph ordering with graphics. Texture hazards and content validity are per mip. Buffer hazards are conservative per physical buffer even when binding views cover smaller byte ranges. RAW/WAR/WAW dependencies, transitions and UAV barriers are generated from declarations; simultaneous overlapping SRV/UAV states are rejected. Empty or zero-group dispatches cannot fulfill a write promise.

Logical passes are partitioned into ordered contiguous batches within the backend's recording-context limit. A long mip chain does not require one native allocator/list per pass. Per-pass GPU timestamps, optional profiling, resource retention and failure cleanup survive batching. The first version uses the graphics submission queue; dispatch-to-draw dependencies require no per-dispatch wait.

`IRHIDevice::ReadTexture` reads one mip without 8-bit conversion; `ReadBuffer` reads a storage byte range. These blocking diagnostic paths preserve the caller-specified resource state and use the existing fenced immediate queue. Runtime HZB/contact rendering does not call them.

Validation targets: `shaders`, `compute_rhi`, `render_graph`, `compute_rendering`, `hierarchical_depth`, `contact_shadows`, `deferred_rendering`, and `d3d12_frame_failure_recovery`.
