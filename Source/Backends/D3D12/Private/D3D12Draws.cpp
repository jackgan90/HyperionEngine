#include "D3D12Draws.h"
#include "D3D12Bindings.h"
#include "D3D12GraphicsState.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
// Packet ownership keeps these objects alive for the entire validation/recording traversal.
template<typename TResource, typename TInterface>
const TResource& ResolveNative(const std::shared_ptr<TInterface>& InPayload, const FD3D12DeviceState& InState,
                               const TResource*& OutCached)
{
	if (!OutCached || InPayload.get() != OutCached)
	{
		OutCached = &NativeResource<TResource>(InPayload, &InState);
	}
	return *OutCached;
}

struct FDrawResources
{
	const FD3D12Pipeline* Pipeline{};
	const FD3D12Buffer* Vertices{};
	const FD3D12Buffer* Indices{};
};

std::pair<std::uint32_t, std::uint32_t> IndexBounds(const FDrawPacket& InDraw, const FD3D12Buffer& InIndices)
{
	HYP_PERF_SCOPE_C(Detail, ValidateIndexBounds);
	std::lock_guard Lock(InIndices.IndexRangesMutex);
	for (const auto& Range : InIndices.IndexRanges)
	{
		if (Range[0] == InDraw.FirstIndex && Range[1] == InDraw.IndexCount)
		{
			return {Range[2], Range[3]};
		}
	}
	std::uint32_t Minimum = UINT_MAX;
	std::uint32_t Maximum{};
	for (std::uint32_t Index = 0; Index < InDraw.IndexCount; ++Index)
	{
		Minimum = std::min(Minimum, InIndices.IndexData[InDraw.FirstIndex + Index]);
		Maximum = std::max(Maximum, InIndices.IndexData[InDraw.FirstIndex + Index]);
	}
	if (InIndices.IndexRanges.size() < 32)
	{
		InIndices.IndexRanges.push_back({InDraw.FirstIndex, InDraw.IndexCount, Minimum, Maximum});
	}
	return {Minimum, Maximum};
}

void ValidateGeometry(const FDrawPacket& InDraw, const FD3D12Buffer& InVertices, const FD3D12Buffer& InIndices)
{
	if (!InDraw.InstanceCount || !InDraw.VertexStride || InVertices.Size > UINT_MAX || InIndices.Size > UINT_MAX ||
	    (InVertices.Usage & BufferUsage(ERHIBufferUsage::Vertex)) == 0 ||
	    (InIndices.Usage & BufferUsage(ERHIBufferUsage::Index)) == 0 ||
	    (std::uint64_t(InDraw.FirstIndex) + InDraw.IndexCount) * 4 > InIndices.Size)
	{
		throw std::invalid_argument("Invalid draw packet");
	}
	if (InDraw.IndexCount)
	{
		const auto [Minimum, Maximum] = IndexBounds(InDraw, InIndices);
		const std::int64_t First = static_cast<std::int64_t>(Minimum) + InDraw.VertexOffset;
		const std::int64_t Last = static_cast<std::int64_t>(Maximum) + InDraw.VertexOffset;
		if (First < 0 || static_cast<std::uint64_t>(Last) >= InVertices.Size / InDraw.VertexStride)
		{
			throw std::invalid_argument("Draw index references a vertex outside the geometry buffer");
		}
	}
}

void ValidateDepthBindings(const FD3D12DeviceState& InState, const FPassCommands& InCommands, const FDrawPacket& InDraw)
{
	if (InDraw.Bindings)
	{
		const auto& Set = NativeResource<FD3D12BindingSet>(InDraw.Bindings.Payload, &InState);
		for (const auto& Entry : Set.Description.Entries)
		{
			for (const auto& Value : Entry.Values)
			{
				if (const auto* Texture = std::get_if<FTexture>(&Value);
				    Texture && NativeResource<FD3D12Texture>(Texture->Payload, &InState).DepthViews)
				{
					if (*Texture == InCommands.GetDepthTexture() ||
					    std::find(InCommands.SampledTextures.begin(), InCommands.SampledTextures.end(), *Texture) ==
					        InCommands.SampledTextures.end())
					{
						throw std::invalid_argument("A sampled depth texture must be declared and cannot be writable");
					}
				}
			}
		}
	}
}

struct FDrawState
{
	ID3D12PipelineState* Pipeline{};
	D3D_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	D3D12_VERTEX_BUFFER_VIEW Vertices{};
	D3D12_INDEX_BUFFER_VIEW Indices{};
	FGraphicsDynamicState Dynamic;
	FRect Scissor;
	bool bDynamic{};
	std::uint64_t PipelineBinds{};
	std::uint64_t GeometryBinds{};
	std::uint64_t DynamicBinds{};
};

void BindDrawState(ID3D12GraphicsCommandList& InList, const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline,
                   FDrawState& InState)
{
	if (InState.Pipeline != InPipeline.Pipeline.Get())
	{
		InList.SetPipelineState(InPipeline.Pipeline.Get());
		InState.Pipeline = InPipeline.Pipeline.Get();
		++InState.PipelineBinds;
	}
	const auto Topology = NativeTopology(InPipeline.Topology);
	if (InState.Topology != Topology)
	{
		InList.IASetPrimitiveTopology(Topology);
		InState.Topology = Topology;
		++InState.GeometryBinds;
	}
	if (!InState.bDynamic || InState.Dynamic.StencilReference != InDraw.DynamicState.StencilReference)
	{
		InList.OMSetStencilRef(InDraw.DynamicState.StencilReference);
		++InState.DynamicBinds;
	}
	if (!InState.bDynamic || InState.Dynamic.BlendConstants != InDraw.DynamicState.BlendConstants)
	{
		InList.OMSetBlendFactor(InDraw.DynamicState.BlendConstants.data());
		++InState.DynamicBinds;
	}
	if (!InState.bDynamic || InState.Scissor.Left != InDraw.Scissor.Left || InState.Scissor.Top != InDraw.Scissor.Top ||
	    InState.Scissor.Right != InDraw.Scissor.Right || InState.Scissor.Bottom != InDraw.Scissor.Bottom)
	{
		const D3D12_RECT Rect{InDraw.Scissor.Left, InDraw.Scissor.Top, InDraw.Scissor.Right, InDraw.Scissor.Bottom};
		InList.RSSetScissorRects(1, &Rect);
		++InState.DynamicBinds;
	}
	InState.Dynamic = InDraw.DynamicState;
	InState.Scissor = InDraw.Scissor;
	InState.bDynamic = true;
}

void BindGeometry(ID3D12GraphicsCommandList& InList, const FDrawPacket& InDraw, const FDrawResources& InResources,
                  FDrawState& InState)
{
	const D3D12_VERTEX_BUFFER_VIEW Vertices{InResources.Vertices->Resource->GetGPUVirtualAddress(),
	                                        static_cast<UINT>(InResources.Vertices->Size), InDraw.VertexStride};
	if (InState.Vertices.BufferLocation != Vertices.BufferLocation ||
	    InState.Vertices.SizeInBytes != Vertices.SizeInBytes ||
	    InState.Vertices.StrideInBytes != Vertices.StrideInBytes)
	{
		InList.IASetVertexBuffers(0, 1, &Vertices);
		InState.Vertices = Vertices;
		++InState.GeometryBinds;
	}
	const D3D12_INDEX_BUFFER_VIEW Indices{InResources.Indices->Resource->GetGPUVirtualAddress(),
	                                      static_cast<UINT>(InResources.Indices->Size), DXGI_FORMAT_R32_UINT};
	if (InState.Indices.BufferLocation != Indices.BufferLocation ||
	    InState.Indices.SizeInBytes != Indices.SizeInBytes || InState.Indices.Format != Indices.Format)
	{
		InList.IASetIndexBuffer(&Indices);
		InState.Indices = Indices;
		++InState.GeometryBinds;
	}
}
} // namespace

void ValidateDraws(const FD3D12DeviceState& InState, const FPassCommands& InCommands)
{
	HYP_PERF_SCOPE_C(Rhi, ValidateDraws);
	FDrawResources Resources;
	const auto CompletedFence = InState.Fence->GetCompletedValue();
	for (const auto& Draw : InCommands.GetDraws())
	{
		const auto& Pipeline = ResolveNative(Draw.Pipeline.Payload, InState, Resources.Pipeline);
		const auto& Vertices = ResolveNative(Draw.Vertices.Payload, InState, Resources.Vertices);
		const auto& Indices = ResolveNative(Draw.Indices.Payload, InState, Resources.Indices);
		ValidateGeometry(Draw, Vertices, Indices);
		const ERHIDepthFormat Depth =
		    (InCommands.HasDepth() || InCommands.HasStencil()) ? InCommands.GetDepthFormat() : ERHIDepthFormat::None;
		if (Draw.VertexStride != Pipeline.VertexStride || Pipeline.Target.bSrgb != InCommands.IsSrgb() ||
		    Pipeline.Target.ColorCount != (InCommands.HasColor() ? 1U : 0U) || Pipeline.Target.Depth != Depth ||
		    (Pipeline.GraphicsState.bDepthTest && !InCommands.HasDepth()) ||
		    (Pipeline.GraphicsState.bStencil && !InCommands.HasStencil()))
		{
			throw std::invalid_argument("Draw geometry or target is incompatible with pipeline");
		}
		ValidateGraphicsDynamicState(Draw.DynamicState);
		ValidateGraphicsBindings(Draw, Pipeline, InState, CompletedFence);
		ValidateDepthBindings(InState, InCommands, Draw);
	}
}

void RecordDraws(ID3D12GraphicsCommandList& InList, const FPassCommands& InCommands, FD3D12DeviceState& InState)
{
	HYP_PERF_SCOPE_C(Rhi, RecordNativeDraws);
	FDrawResources Resources;
	FDrawState State;
	FD3D12GraphicsBindingState Bindings;
	for (const auto& Draw : InCommands.GetDraws())
	{
		const auto& Pipeline = ResolveNative(Draw.Pipeline.Payload, InState, Resources.Pipeline);
		ResolveNative(Draw.Vertices.Payload, InState, Resources.Vertices);
		ResolveNative(Draw.Indices.Payload, InState, Resources.Indices);
		BindDrawState(InList, Draw, Pipeline, State);
		BindGeometry(InList, Draw, Resources, State);
		RecordGraphicsBindings(InList, Draw, Pipeline, InState, Bindings);
		InList.DrawIndexedInstanced(Draw.IndexCount, Draw.InstanceCount, Draw.FirstIndex, Draw.VertexOffset, 0);
	}
	InState.GraphicsRootBinds += Bindings.RootBinds;
	InState.GraphicsHeapBinds += Bindings.HeapBinds;
	InState.GraphicsConstantBinds += Bindings.ConstantBinds;
	InState.GraphicsTableBinds += Bindings.TableBinds;
	InState.GraphicsPipelineBinds += State.PipelineBinds;
	InState.GraphicsGeometryBinds += State.GeometryBinds;
	InState.GraphicsDynamicBinds += State.DynamicBinds;
	HYP_PERF_PLOT(Rhi, GraphicsRootBinds, double(Bindings.RootBinds));
	HYP_PERF_PLOT(Rhi, GraphicsHeapBinds, double(Bindings.HeapBinds));
	HYP_PERF_PLOT(Rhi, GraphicsConstantBinds, double(Bindings.ConstantBinds));
	HYP_PERF_PLOT(Rhi, GraphicsTableBinds, double(Bindings.TableBinds));
	HYP_PERF_PLOT(Rhi, GraphicsPipelineBinds, double(State.PipelineBinds));
	HYP_PERF_PLOT(Rhi, GraphicsGeometryBinds, double(State.GeometryBinds));
	HYP_PERF_PLOT(Rhi, GraphicsDynamicBinds, double(State.DynamicBinds));
}
} // namespace Hyperion
