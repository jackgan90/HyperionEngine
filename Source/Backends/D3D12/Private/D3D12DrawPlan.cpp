#include "D3D12Bindings.h"
#include "D3D12Draws.h"
#include "D3D12GraphicsState.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <bit>

namespace Hyperion
{
struct FD3D12DrawPlan
{
	enum class EOperation
	{
		Pipeline,
		Topology,
		Vertices,
		Indices,
		Stencil,
		Blend,
		Scissor,
		Root,
		Heaps,
		Constant,
		Table,
		Draw
	};

	struct FCommand
	{
		EOperation Operation;
		UINT Slot{};
		std::uint64_t Value{};
		std::array<std::uint32_t, 4> Arguments{};
		void* Pointer{};
	};

	std::vector<FCommand> Commands;
	// Root, heap, constant, table, pipeline, geometry, dynamic binds.
	std::array<std::uint64_t, 7> Binds{};
};

namespace
{
void RetainConstantPages(const FD3D12DeviceState& InState, const std::shared_ptr<const FPassCommands>& InCommands)
{
	const std::shared_ptr<const void> Owner = InCommands->SharedDraws
	                                              ? std::static_pointer_cast<const void>(InCommands->SharedDraws)
	                                              : std::static_pointer_cast<const void>(InCommands);
	for (const auto& Draw : InCommands->GetDraws())
	{
		for (const auto& Constant : Draw.ConstantBindings)
		{
			const auto& Buffer = NativeResource<FD3D12Buffer>(Constant.Slice.Buffer.Payload, &InState);
			std::lock_guard Lock(Buffer.ConstantMutex);
			std::erase_if(Buffer.ConstantOwners,
			              [](const auto& InOwner)
			              {
				              return InOwner.expired();
			              });
			if (std::none_of(Buffer.ConstantOwners.begin(), Buffer.ConstantOwners.end(),
			                 [&](const auto& InOwner)
			                 {
				                 return !InOwner.owner_before(Owner) && !Owner.owner_before(InOwner);
			                 }))
			{
				Buffer.ConstantOwners.push_back(Owner);
			}
		}
	}
}

struct FPlanBuilder
{
	FD3D12DrawPlan Plan;
	const FD3D12DeviceState& Device;
	FD3D12GraphicsBindingState Bindings;
	ID3D12PipelineState* Pipeline{};
	D3D_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	std::optional<FGraphicsDynamicState> Dynamic;
	FRect Scissor;
	D3D12_VERTEX_BUFFER_VIEW Vertices{};
	D3D12_INDEX_BUFFER_VIEW Indices{};

	void State(const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline)
	{
		if (Pipeline != InPipeline.Pipeline.Get())
		{
			Pipeline = InPipeline.Pipeline.Get();
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Pipeline, 0, 0, {}, Pipeline});
			++Plan.Binds[4];
		}
		const auto NewTopology = NativeTopology(InPipeline.Topology);
		if (Topology != NewTopology)
		{
			Topology = NewTopology;
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Topology, UINT(Topology)});
			++Plan.Binds[5];
		}
		if (!Dynamic || Dynamic->StencilReference != InDraw.DynamicState.StencilReference)
		{
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Stencil, InDraw.DynamicState.StencilReference});
			++Plan.Binds[6];
		}
		if (!Dynamic || Dynamic->BlendConstants != InDraw.DynamicState.BlendConstants)
		{
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Blend, 0, 0,
			                         std::bit_cast<std::array<std::uint32_t, 4>>(InDraw.DynamicState.BlendConstants)});
			++Plan.Binds[6];
		}
		if (!Dynamic || Scissor.Left != InDraw.Scissor.Left || Scissor.Top != InDraw.Scissor.Top ||
		    Scissor.Right != InDraw.Scissor.Right || Scissor.Bottom != InDraw.Scissor.Bottom)
		{
			Scissor = InDraw.Scissor;
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Scissor,
			                         0,
			                         0,
			                         {std::uint32_t(Scissor.Left), std::uint32_t(Scissor.Top),
			                          std::uint32_t(Scissor.Right), std::uint32_t(Scissor.Bottom)}});
			++Plan.Binds[6];
		}
		Dynamic = InDraw.DynamicState;
	}

	void Geometry(const FDrawPacket& InDraw)
	{
		const auto& VertexBuffer = NativeResource<FD3D12Buffer>(InDraw.Vertices.Payload, &Device);
		const auto& IndexBuffer = NativeResource<FD3D12Buffer>(InDraw.Indices.Payload, &Device);
		const D3D12_VERTEX_BUFFER_VIEW NewVertices{VertexBuffer.Resource->GetGPUVirtualAddress(),
		                                           UINT(VertexBuffer.Size), InDraw.VertexStride};
		if (Vertices.BufferLocation != NewVertices.BufferLocation || Vertices.SizeInBytes != NewVertices.SizeInBytes ||
		    Vertices.StrideInBytes != NewVertices.StrideInBytes)
		{
			Vertices = NewVertices;
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Vertices,
			                         0,
			                         Vertices.BufferLocation,
			                         {Vertices.SizeInBytes, Vertices.StrideInBytes}});
			++Plan.Binds[5];
		}
		const D3D12_INDEX_BUFFER_VIEW NewIndices{IndexBuffer.Resource->GetGPUVirtualAddress(), UINT(IndexBuffer.Size),
		                                         DXGI_FORMAT_R32_UINT};
		if (Indices.BufferLocation != NewIndices.BufferLocation || Indices.SizeInBytes != NewIndices.SizeInBytes)
		{
			Indices = NewIndices;
			Plan.Commands.push_back(
			    {FD3D12DrawPlan::EOperation::Indices, 0, Indices.BufferLocation, {Indices.SizeInBytes}});
			++Plan.Binds[5];
		}
	}

	void Resources(const FDrawPacket& InDraw, const FD3D12Pipeline& InPipeline)
	{
		const auto& Layout = NativeResource<FD3D12BindingLayout>(InPipeline.Layout.Payload, &Device);
		if (Bindings.Root != InPipeline.Root.Get())
		{
			Bindings.Root = InPipeline.Root.Get();
			Bindings.Constants.fill(0);
			Bindings.Tables.fill(0);
			Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Root, 0, 0, {}, Bindings.Root});
			++Plan.Binds[0];
		}
		const std::array<ID3D12DescriptorHeap*, 2> Heaps{Device.ResourceTables.GetHeap(),
		                                                 Device.SamplerTables.GetHeap()};
		if (Bindings.Heaps != Heaps)
		{
			Bindings.Heaps = Heaps;
			Bindings.Tables.fill(0);
			Plan.Commands.push_back(
			    {FD3D12DrawPlan::EOperation::Heaps, 0, reinterpret_cast<std::uintptr_t>(Heaps[1]), {}, Heaps[0]});
			++Plan.Binds[1];
		}
		for (const auto& Constant : InDraw.ConstantBindings)
		{
			const auto& Buffer = NativeResource<FD3D12Buffer>(Constant.Slice.Buffer.Payload, &Device);
			const auto Root = Layout.Slots[Constant.Slot].RootParameter;
			const auto Address = Buffer.Resource->GetGPUVirtualAddress() + Constant.Slice.Offset;
			if (Bindings.Constants.at(Root) != Address)
			{
				Bindings.Constants[Root] = Address;
				Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Constant, Root, Address});
				++Plan.Binds[2];
			}
		}
		if (InDraw.Bindings)
		{
			const auto& Set = NativeResource<FD3D12BindingSet>(InDraw.Bindings.Payload, &Device);
			for (std::size_t Index = 0; Index < Layout.Tables.size(); ++Index)
			{
				const auto& Arena = Layout.Tables[Index].bSampler ? Device.SamplerTables : Device.ResourceTables;
				const auto Root = Layout.Tables[Index].RootParameter;
				const auto Handle = Arena.Gpu(Set.Tables[Index].Offset);
				if (Bindings.Tables.at(Root) != Handle.ptr)
				{
					Bindings.Tables[Root] = Handle.ptr;
					Plan.Commands.push_back({FD3D12DrawPlan::EOperation::Table, Root, Handle.ptr});
					++Plan.Binds[3];
				}
			}
		}
	}
};

std::shared_ptr<const FD3D12DrawPlan> BuildPlan(const FD3D12DeviceState& InState, const FPassCommands& InCommands)
{
	HYP_PERF_SCOPE_C(Rhi, BuildNativeDrawPlan);
	FPlanBuilder Builder{{}, InState};
	Builder.Plan.Commands.reserve(InCommands.GetDraws().size() * 2 + 16);
	for (const auto& Draw : InCommands.GetDraws())
	{
		const auto& Pipeline = NativeResource<FD3D12Pipeline>(Draw.Pipeline.Payload, &InState);
		Builder.State(Draw, Pipeline);
		Builder.Geometry(Draw);
		Builder.Resources(Draw, Pipeline);
		Builder.Plan.Commands.push_back(
		    {FD3D12DrawPlan::EOperation::Draw,
		     0,
		     0,
		     {Draw.IndexCount, Draw.InstanceCount, Draw.FirstIndex, std::bit_cast<std::uint32_t>(Draw.VertexOffset)}});
	}
	return std::make_shared<const FD3D12DrawPlan>(std::move(Builder.Plan));
}

void ExecuteCommand(ID3D12GraphicsCommandList& InList, const FD3D12DrawPlan::FCommand& InCommand)
{
	const auto& Args = InCommand.Arguments;
	switch (InCommand.Operation)
	{
		case FD3D12DrawPlan::EOperation::Pipeline:
			InList.SetPipelineState(static_cast<ID3D12PipelineState*>(InCommand.Pointer));
			break;
		case FD3D12DrawPlan::EOperation::Topology:
			InList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(InCommand.Slot));
			break;
		case FD3D12DrawPlan::EOperation::Vertices:
		{
			const D3D12_VERTEX_BUFFER_VIEW View{InCommand.Value, Args[0], Args[1]};
			InList.IASetVertexBuffers(0, 1, &View);
			break;
		}
		case FD3D12DrawPlan::EOperation::Indices:
		{
			const D3D12_INDEX_BUFFER_VIEW View{InCommand.Value, Args[0], DXGI_FORMAT_R32_UINT};
			InList.IASetIndexBuffer(&View);
			break;
		}
		case FD3D12DrawPlan::EOperation::Stencil:
			InList.OMSetStencilRef(InCommand.Slot);
			break;
		case FD3D12DrawPlan::EOperation::Blend:
		{
			const auto Values = std::bit_cast<std::array<float, 4>>(Args);
			InList.OMSetBlendFactor(Values.data());
			break;
		}
		case FD3D12DrawPlan::EOperation::Scissor:
		{
			const D3D12_RECT Rect{LONG(Args[0]), LONG(Args[1]), LONG(Args[2]), LONG(Args[3])};
			InList.RSSetScissorRects(1, &Rect);
			break;
		}
		case FD3D12DrawPlan::EOperation::Root:
			InList.SetGraphicsRootSignature(static_cast<ID3D12RootSignature*>(InCommand.Pointer));
			break;
		case FD3D12DrawPlan::EOperation::Heaps:
		{
			const std::array Heaps{static_cast<ID3D12DescriptorHeap*>(InCommand.Pointer),
			                       reinterpret_cast<ID3D12DescriptorHeap*>(InCommand.Value)};
			InList.SetDescriptorHeaps(UINT(Heaps.size()), Heaps.data());
			break;
		}
		case FD3D12DrawPlan::EOperation::Constant:
			InList.SetGraphicsRootConstantBufferView(InCommand.Slot, InCommand.Value);
			break;
		case FD3D12DrawPlan::EOperation::Table:
			InList.SetGraphicsRootDescriptorTable(InCommand.Slot, {InCommand.Value});
			break;
		case FD3D12DrawPlan::EOperation::Draw:
			InList.DrawIndexedInstanced(Args[0], Args[1], Args[2], std::bit_cast<std::int32_t>(Args[3]), 0);
			break;
	}
}
} // namespace

std::shared_ptr<const FD3D12DrawPlan> PrepareNativeDraws(const FD3D12DeviceState& InState,
                                                         const std::shared_ptr<const FPassCommands>& InOwnedCommands,
                                                         FD3D12DrawCache& InCache)
{
	const auto& InCommands = *InOwnedCommands;
	InCommands.GetDraws(); // Validate command-shell storage even when its shared stream has a cached plan.
	const std::array<unsigned, 7> Target{
	    InCommands.IsSrgb(),
	    InCommands.HasColor(),
	    InCommands.HasDepth(),
	    InCommands.HasStencil(),
	    unsigned(InCommands.GetDepthFormat()),
	    unsigned(InCommands.Color ? InCommands.Color->Target.Kind : ERenderTargetKind::None),
	    unsigned(InCommands.DepthStencil ? InCommands.DepthStencil->Target.Kind : ERenderTargetKind::None)};
	std::lock_guard Lock(InCache.Mutex);
	if (InCommands.SharedDraws && InCache.Owner.lock() == InCommands.SharedDraws && InCache.Target == Target &&
	    InCache.DepthTarget.lock() == InCommands.GetDepthTexture().Payload &&
	    InCache.Reads.size() == InCommands.SampledTextures.size() &&
	    std::equal(InCache.Reads.begin(), InCache.Reads.end(), InCommands.SampledTextures.begin(),
	               [](const auto& InWeak, const auto& InTexture)
	               {
		               return InWeak.lock() == InTexture.Payload;
	               }))
	{
		// The validated stream has registered constant-page ownership. Reset remains blocked until it expires,
		// including when recordings/submissions share the stream's sole nested buffer handle.
		if (!InCache.Plan)
		{
			InCache.Plan = BuildPlan(InState, InCommands);
		}
		HYP_PERF_PLOT(Rhi, NativeDrawPlanReuses, 1.0);
		return InCache.Plan;
	}
	// Register before validation: a reset either completes first (and stale validation fails), or is rejected.
	// Allocation failure cannot publish a reuse proof or successful recording without every page protected.
	RetainConstantPages(InState, InOwnedCommands);
	ValidateDraws(InState, InCommands);
	InCache.Plan.reset();
	InCache.Owner = InCommands.SharedDraws;
	InCache.Target = Target;
	InCache.DepthTarget = InCommands.GetDepthTexture().Payload;
	InCache.Reads.clear();
	for (const auto& Texture : InCommands.SampledTextures)
	{
		InCache.Reads.push_back(Texture.Payload);
	}
	HYP_PERF_PLOT(Rhi, NativeDrawPlanReuses, 0.0);
	return {};
}

void RecordNativeDrawPlan(ID3D12GraphicsCommandList& InList, const FD3D12DrawPlan& InPlan, FD3D12DeviceState& InState)
{
	HYP_PERF_SCOPE_C(Rhi, RecordNativeDraws);
	HYP_PERF_SCOPE_C(Detail, ExecuteNativeDrawPlan);
	for (const auto& Command : InPlan.Commands)
	{
		ExecuteCommand(InList, Command);
	}
	InState.GraphicsRootBinds += InPlan.Binds[0];
	InState.GraphicsHeapBinds += InPlan.Binds[1];
	InState.GraphicsConstantBinds += InPlan.Binds[2];
	InState.GraphicsTableBinds += InPlan.Binds[3];
	InState.GraphicsPipelineBinds += InPlan.Binds[4];
	InState.GraphicsGeometryBinds += InPlan.Binds[5];
	InState.GraphicsDynamicBinds += InPlan.Binds[6];
	HYP_PERF_PLOT(Rhi, GraphicsRootBinds, double(InPlan.Binds[0]));
	HYP_PERF_PLOT(Rhi, GraphicsHeapBinds, double(InPlan.Binds[1]));
	HYP_PERF_PLOT(Rhi, GraphicsConstantBinds, double(InPlan.Binds[2]));
	HYP_PERF_PLOT(Rhi, GraphicsTableBinds, double(InPlan.Binds[3]));
	HYP_PERF_PLOT(Rhi, GraphicsPipelineBinds, double(InPlan.Binds[4]));
	HYP_PERF_PLOT(Rhi, GraphicsGeometryBinds, double(InPlan.Binds[5]));
	HYP_PERF_PLOT(Rhi, GraphicsDynamicBinds, double(InPlan.Binds[6]));
}
} // namespace Hyperion
