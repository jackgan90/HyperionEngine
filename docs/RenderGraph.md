# Render graph

## Authoring

`FRenderGraph` owns resource imports and graphics pass declarations. `FGraphTexture` handles carry graph identity and resource index; consuming compilation resets that identity. Copies preserve declarations. Import frame backbuffer, frame depth, or sampled D32 explicitly, with dimensions, initial state and content validity. Deferred resolution requires stable source identity. Repeated imports must agree; distinct identities resolving to one native texture are rejected. Resolved imports must match the immutable physical base-level size and depth format exposed by `IRHITexture::GetInfo`, before any draw preparation runs.

`FGraphicsPass` declares optional color and depth/stencil attachments, viewport, sampled reads and `After` dependencies. Color view selects linear, sRGB or material draw-batch view. Each aspect has Load/Clear/Discard and Store/Discard actions. A missing attachment never selects an implicit target. Export backbuffer to Present and sampled depth to ShaderRead explicitly.

```cpp
FRenderGraph Graph;
const auto Color = Graph.ImportBackbuffer({640, 480});
Graph.Export(Color, EResourceState::Present);
FGraphicsPass Clear;
Clear.Name = "Clear";
Clear.Color = FGraphColorAttachment{Color, {EAttachmentLoad::Clear}, {.1f, .2f, .3f, 1}};
Graph.Add(std::move(Clear));
```

A pass owns draw batches or one callback returning batches. Attachments and dependencies must exist beforehand. Empty draw lists retain attachment operations. Multiple batches load only on the first and store only on the last; intermediate batches load/store existing contents. Compilation rejects declaration mutation/re-entry on the graph being consumed, including copy/move operations involving an active graph. Moving an idle graph transfers its handles and gives the moved-from graph a fresh identity. Compilation and mutation are not thread-safe.

## Compilation and execution

Declared accesses generate RAW/WAR/WAW edges. Stable topological sorting combines them with `After` without a universal predecessor edge. Before native resolvers or draw callbacks run, the compiler validates handles, names, topology, formats/aspects, dimensions, sampled/write conflicts and content lifetime. Clear initializes a region; discard invalidates it; draws do not prove complete coverage. Region unions can prove whole-resource initialization. Content regions use the same floor/ceil pixel rectangles as D3D12 clear/discard. Unknown frame dimensions cannot prove full initialization from a partial viewport.

Resource state is independent of content validity. The compiler emits explicit transitions and `FPassCommands` attachments/read lists. D3D12 resolves frame targets, checks native/pipeline compatibility, records barriers, binds RTV/DSV and executes clear/discard by aspect. Existing submission fences, shared draw ownership and failed-Present recovery retain native resources safely.

`ExecuteGraph` resolves/prepares on RHI 0 before frame acquisition, then uses the existing frame coordinator and failure cleanup. Direct Compile on Renderer resource callbacks also requires RHI 0. `Compile` materializes draw storage without consuming the original; `CompileAndConsume` preserves shared immutable draw storage.

## Integration and scope

`FRenderView` contains camera/culling/material inputs; `FRenderPassTargets` contains outputs/reads. The forward pipeline declares four shadow producers and a forward consumer. Preview and GUI declare separate passes. Frozen material bindings contribute additional sampled depth reads before compilation. Static prepared views retain their sampled-read declarations in the existing bounded view cache; parameter, target or resource changes invalidate that proof. Moving views read frozen parameter values and validated shared binding metadata without copying parameter tables. Shadow views still query visibility independently; caches compare explicit attachment signatures and retain view-independent work during camera motion. Batch caches derive depth format directly from attachments.

Supported: one backbuffer color attachment, frame D32/D32S8, sampled offscreen D32, graphics draws and one submission queue. Offscreen color, MRT, MSAA/resolve, transient aliasing, compute and pass culling are outside this change. Graph-negative, GPU pixel and lifetime coverage lives in `render_graph`, `material_rendering`, `cascaded_shadow_rendering`, `rhi_backend_contracts` and `d3d12_frame_failure_recovery`.
