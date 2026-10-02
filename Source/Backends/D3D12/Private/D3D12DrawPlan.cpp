#include "D3D12Bindings.h"
#include "D3D12DrawArguments.h"
#include "D3D12DrawPlanInspection.h"
#include "D3D12Draws.h"
#include "D3D12GraphicsState.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <type_traits>

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

	struct FConstantArguments
	{
		UINT RootParameter;
		D3D12_GPU_VIRTUAL_ADDRESS Address;
	};

	struct FTableArguments
	{
		UINT RootParameter;
		D3D12_GPU_DESCRIPTOR_HANDLE Handle;
	};

	struct FCommand
	{
		EOperation Operation;

		union
		{
			ID3D12PipelineState* Pipeline;
			D3D_PRIMITIVE_TOPOLOGY Topology;
			D3D12_VERTEX_BUFFER_VIEW Vertices;
			D3D12_INDEX_BUFFER_VIEW Indices;
			UINT StencilReference;
			std::array<float, 4> BlendConstants;
			D3D12_RECT Scissor;
			ID3D12RootSignature* Root;
			std::array<ID3D12DescriptorHeap*, 2> Heaps;
			FConstantArguments Constant;
			FTableArguments Table;
			FD3D12IndexedDrawArguments Draw;
		};

		explicit FCommand(ID3D12PipelineState* InPipeline) noexcept
		    : Operation(EOperation::Pipeline), Pipeline(InPipeline)
		{
		}

		explicit FCommand(D3D_PRIMITIVE_TOPOLOGY InTopology) noexcept
		    : Operation(EOperation::Topology), Topology(InTopology)
		{
		}

		explicit FCommand(const D3D12_VERTEX_BUFFER_VIEW& InVertices) noexcept
		    : Operation(EOperation::Vertices), Vertices(InVertices)
		{
		}

		explicit FCommand(const D3D12_INDEX_BUFFER_VIEW& InIndices) noexcept
		    : Operation(EOperation::Indices), Indices(InIndices)
		{
		}

		explicit FCommand(UINT InStencilReference) noexcept
		    : Operation(EOperation::Stencil), StencilReference(InStencilReference)
		{
		}

		explicit FCommand(const std::array<float, 4>& InBlendConstants) noexcept
		    : Operation(EOperation::Blend), BlendConstants(InBlendConstants)
		{
		}

		explicit FCommand(const D3D12_RECT& InScissor) noexcept : Operation(EOperation::Scissor), Scissor(InScissor)
		{
		}

		explicit FCommand(ID3D12RootSignature* InRoot) noexcept : Operation(EOperation::Root), Root(InRoot)
		{
		}

		explicit FCommand(const std::array<ID3D12DescriptorHeap*, 2>& InHeaps) noexcept
		    : Operation(EOperation::Heaps), Heaps(InHeaps)
		{
		}

		explicit FCommand(FConstantArguments InConstant) noexcept
		    : Operation(EOperation::Constant), Constant(InConstant)
		{
		}

		explicit FCommand(FTableArguments InTable) noexcept : Operation(EOperation::Table), Table(InTable)
		{
		}

		explicit FCommand(const FD3D12IndexedDrawArguments& InDraw) noexcept : Operation(EOperation::Draw), Draw(InDraw)
		{
		}
	};

	struct FBindCounts
	{
		std::uint64_t Root{};
		std::uint64_t Heap{};
		std::uint64_t Constant{};
		std::uint64_t Table{};
		std::uint64_t Pipeline{};
		std::uint64_t Geometry{};
		std::uint64_t Dynamic{};
	};

	std::vector<FCommand> Commands;
	FBindCounts Binds;
};

static_assert(std::is_trivially_copyable_v<FD3D12DrawPlan::FConstantArguments>);
static_assert(std::is_trivially_copyable_v<FD3D12DrawPlan::FTableArguments>);
static_assert(std::is_trivially_copyable_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_standard_layout_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_trivially_destructible_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_trivially_copy_constructible_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_trivially_move_constructible_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_nothrow_copy_constructible_v<FD3D12DrawPlan::FCommand>);
static_assert(std::is_nothrow_move_constructible_v<FD3D12DrawPlan::FCommand>);
static_assert(sizeof(FD3D12DrawPlan::FCommand) == 24);
static_assert(alignof(FD3D12DrawPlan::FCommand) == 8);
static_assert(sizeof(FD3D12DrawPlan::FBindCounts) == 7 * sizeof(std::uint64_t));

FNativeDrawPlanMetrics DescribeNativeDrawPlan(const FD3D12DrawPlan* InPlan)
{
	FNativeDrawPlanMetrics Result;
	Result.CommandSize = sizeof(FD3D12DrawPlan::FCommand);
	Result.CommandAlignment = alignof(FD3D12DrawPlan::FCommand);
	if (InPlan)
	{
		Result.bHasPlan = true;
		Result.CommandCount = InPlan->Commands.size();
		Result.CommandCapacity = InPlan->Commands.capacity();
		Result.UsedBytes = Result.CommandCount * Result.CommandSize;
		Result.CapacityBytes = Result.CommandCapacity * Result.CommandSize;
		const auto& Binds = InPlan->Binds;
		Result.Binds.Root = Binds.Root;
		Result.Binds.Heap = Binds.Heap;
		Result.Binds.Constant = Binds.Constant;
		Result.Binds.Table = Binds.Table;
		Result.Binds.Pipeline = Binds.Pipeline;
		Result.Binds.Geometry = Binds.Geometry;
		Result.Binds.Dynamic = Binds.Dynamic;
	}
	return Result;
}

namespace
{
FNativeDrawObservation InspectCommand(const FD3D12DrawPlan::FCommand& InCommand)
{
	FNativeDrawObservation Result;
	switch (InCommand.Operation)
	{
		case FD3D12DrawPlan::EOperation::Pipeline:
			Result.Operation = ENativeDrawObservation::Pipeline;
			Result.Pipeline = InCommand.Pipeline;
			break;
		case FD3D12DrawPlan::EOperation::Topology:
			Result.Operation = ENativeDrawObservation::Topology;
			Result.Topology = InCommand.Topology;
			break;
		case FD3D12DrawPlan::EOperation::Vertices:
			Result.Operation = ENativeDrawObservation::Vertices;
			Result.Vertices = InCommand.Vertices;
			break;
		case FD3D12DrawPlan::EOperation::Indices:
			Result.Operation = ENativeDrawObservation::Indices;
			Result.Indices = InCommand.Indices;
			break;
		case FD3D12DrawPlan::EOperation::Stencil:
			Result.Operation = ENativeDrawObservation::Stencil;
			Result.StencilReference = InCommand.StencilReference;
			break;
		case FD3D12DrawPlan::EOperation::Blend:
			Result.Operation = ENativeDrawObservation::Blend;
			Result.BlendConstants = InCommand.BlendConstants;
			break;
		case FD3D12DrawPlan::EOperation::Scissor:
			Result.Operation = ENativeDrawObservation::Scissor;
			Result.Scissor = InCommand.Scissor;
			break;
		case FD3D12DrawPlan::EOperation::Root:
			Result.Operation = ENativeDrawObservation::Root;
			Result.Root = InCommand.Root;
			break;
		case FD3D12DrawPlan::EOperation::Heaps:
			Result.Operation = ENativeDrawObservation::Heaps;
			Result.Heaps = InCommand.Heaps;
			break;
		case FD3D12DrawPlan::EOperation::Constant:
			Result.Operation = ENativeDrawObservation::Constant;
			Result.RootParameter = InCommand.Constant.RootParameter;
			Result.ConstantAddress = InCommand.Constant.Address;
			break;
		case FD3D12DrawPlan::EOperation::Table:
			Result.Operation = ENativeDrawObservation::Table;
			Result.RootParameter = InCommand.Table.RootParameter;
			Result.TableHandle = InCommand.Table.Handle;
			break;
		case FD3D12DrawPlan::EOperation::Draw:
			Result.Operation = ENativeDrawObservation::Draw;
			Result.IndexCount = InCommand.Draw.IndexCount;
			Result.InstanceCount = InCommand.Draw.InstanceCount;
			Result.FirstIndex = InCommand.Draw.FirstIndex;
			Result.VertexOffset = InCommand.Draw.VertexOffset;
			Result.FirstInstance = FD3D12IndexedDrawArguments::FirstInstance;
			break;
	}
	return Result;
}
} // namespace

std::vector<FNativeDrawObservation> InspectNativeDrawPlan(const FD3D12DrawPlan& InPlan)
{
	std::vector<FNativeDrawObservation> Result;
	Result.reserve(InPlan.Commands.size());
	for (const auto& Command : InPlan.Commands)
	{
		Result.push_back(InspectCommand(Command));
	}
	return Result;
}

namespace
{
void RetainConstantPages(const FD3D12DeviceState& InState, const std::shared_ptr<const FPassCommands>& InCommands)
{
	const std::shared_ptr<const void> Owner = InCommands->SharedDraws
	                                              ? std::static_pointer_cast<const void>(InCommands->SharedDraws)
	                                              : std::static_pointer_cast<const void>(InCommands);
	for (const auto& Draw : InCommands->GetDraws())
	{
		RetainShaderConstantPages(InState, Draw.ConstantBindings, Owner);
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
			Plan.Commands.emplace_back(Pipeline);
			++Plan.Binds.Pipeline;
		}
		const auto NewTopology = NativeTopology(InPipeline.Topology);
		if (Topology != NewTopology)
		{
			Topology = NewTopology;
			Plan.Commands.emplace_back(Topology);
			++Plan.Binds.Geometry;
		}
		const auto StencilReference = NativeStencilReference(InDraw.DynamicState);
		if (!Dynamic || Dynamic->StencilReference != StencilReference)
		{
			Plan.Commands.emplace_back(StencilReference);
			++Plan.Binds.Dynamic;
		}
		const auto& BlendConstants = NativeBlendConstants(InDraw.DynamicState);
		if (!Dynamic || Dynamic->BlendConstants != BlendConstants)
		{
			Plan.Commands.emplace_back(BlendConstants);
			++Plan.Binds.Dynamic;
		}
		if (!Dynamic || Scissor.Left != InDraw.Scissor.Left || Scissor.Top != InDraw.Scissor.Top ||
		    Scissor.Right != InDraw.Scissor.Right || Scissor.Bottom != InDraw.Scissor.Bottom)
		{
			Scissor = InDraw.Scissor;
			const auto NativeRect = NativeScissor(InDraw.Scissor);
			Plan.Commands.emplace_back(NativeRect);
			++Plan.Binds.Dynamic;
		}
		Dynamic = InDraw.DynamicState;
	}

	void Geometry(const FDrawPacket& InDraw)
	{
		const auto& VertexBuffer = NativeResource<FD3D12Buffer>(InDraw.Vertices.Payload, &Device);
		const auto& IndexBuffer = NativeResource<FD3D12Buffer>(InDraw.Indices.Payload, &Device);
		const auto NewVertices = NativeVertexBufferView(VertexBuffer, InDraw.VertexStride);
		if (Vertices.BufferLocation != NewVertices.BufferLocation || Vertices.SizeInBytes != NewVertices.SizeInBytes ||
		    Vertices.StrideInBytes != NewVertices.StrideInBytes)
		{
			Vertices = NewVertices;
			Plan.Commands.emplace_back(Vertices);
			++Plan.Binds.Geometry;
		}
		const auto NewIndices = NativeIndexBufferView(IndexBuffer);
		if (Indices.BufferLocation != NewIndices.BufferLocation || Indices.SizeInBytes != NewIndices.SizeInBytes ||
		    Indices.Format != NewIndices.Format)
		{
			Indices = NewIndices;
			Plan.Commands.emplace_back(Indices);
			++Plan.Binds.Geometry;
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
			Plan.Commands.emplace_back(Bindings.Root);
			++Plan.Binds.Root;
		}
		const std::array<ID3D12DescriptorHeap*, 2> Heaps{Device.ResourceTables.GetHeap(),
		                                                 Device.SamplerTables.GetHeap()};
		if (Bindings.Heaps != Heaps)
		{
			Bindings.Heaps = Heaps;
			Bindings.Tables.fill(0);
			Plan.Commands.emplace_back(Heaps);
			++Plan.Binds.Heap;
		}
		for (const auto& Constant : InDraw.ConstantBindings)
		{
			const auto& Buffer = NativeResource<FD3D12Buffer>(Constant.Slice.Buffer.Payload, &Device);
			const auto Root = Layout.Slots[Constant.Slot].RootParameter;
			const auto Address = Buffer.Resource->GetGPUVirtualAddress() + Constant.Slice.Offset;
			if (Bindings.Constants.at(Root) != Address)
			{
				Bindings.Constants[Root] = Address;
				Plan.Commands.emplace_back(FD3D12DrawPlan::FConstantArguments{Root, Address});
				++Plan.Binds.Constant;
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
					Plan.Commands.emplace_back(FD3D12DrawPlan::FTableArguments{Root, Handle});
					++Plan.Binds.Table;
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
		const auto Arguments = NativeIndexedDrawArguments(Draw);
		Builder.Plan.Commands.emplace_back(Arguments);
	}
	return std::make_shared<const FD3D12DrawPlan>(std::move(Builder.Plan));
}

void ExecuteCommand(ID3D12GraphicsCommandList& InList, const FD3D12DrawPlan::FCommand& InCommand)
{
	switch (InCommand.Operation)
	{
		case FD3D12DrawPlan::EOperation::Pipeline:
			InList.SetPipelineState(InCommand.Pipeline);
			break;
		case FD3D12DrawPlan::EOperation::Topology:
			InList.IASetPrimitiveTopology(InCommand.Topology);
			break;
		case FD3D12DrawPlan::EOperation::Vertices:
			InList.IASetVertexBuffers(0, 1, &InCommand.Vertices);
			break;
		case FD3D12DrawPlan::EOperation::Indices:
			InList.IASetIndexBuffer(&InCommand.Indices);
			break;
		case FD3D12DrawPlan::EOperation::Stencil:
			InList.OMSetStencilRef(InCommand.StencilReference);
			break;
		case FD3D12DrawPlan::EOperation::Blend:
			InList.OMSetBlendFactor(InCommand.BlendConstants.data());
			break;
		case FD3D12DrawPlan::EOperation::Scissor:
			InList.RSSetScissorRects(1, &InCommand.Scissor);
			break;
		case FD3D12DrawPlan::EOperation::Root:
			InList.SetGraphicsRootSignature(InCommand.Root);
			break;
		case FD3D12DrawPlan::EOperation::Heaps:
			InList.SetDescriptorHeaps(static_cast<UINT>(InCommand.Heaps.size()), InCommand.Heaps.data());
			break;
		case FD3D12DrawPlan::EOperation::Constant:
			InList.SetGraphicsRootConstantBufferView(InCommand.Constant.RootParameter, InCommand.Constant.Address);
			break;
		case FD3D12DrawPlan::EOperation::Table:
			InList.SetGraphicsRootDescriptorTable(InCommand.Table.RootParameter, InCommand.Table.Handle);
			break;
		case FD3D12DrawPlan::EOperation::Draw:
			InList.DrawIndexedInstanced(InCommand.Draw.IndexCount, InCommand.Draw.InstanceCount,
			                            InCommand.Draw.FirstIndex, InCommand.Draw.VertexOffset,
			                            FD3D12IndexedDrawArguments::FirstInstance);
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
	const auto Colors = InCommands.GetColors();
	const auto GraphicsTarget = InCommands.GetGraphicsTarget();
	std::lock_guard Lock(InCache.Mutex);
	if (InCache.BufferAccesses.size() == InCommands.BufferAccesses.size() &&
	    std::equal(InCache.BufferAccesses.begin(), InCache.BufferAccesses.end(), InCommands.BufferAccesses.begin(),
	               [](const auto& InKey, const auto& InAccess)
	               {
		               return InKey.Buffer.lock() == InAccess.View.Buffer.Payload &&
		                      InKey.Offset == InAccess.View.Offset && InKey.Size == InAccess.View.Size &&
		                      InKey.State == InAccess.State;
	               }) &&
	    InCache.TextureAccesses.size() == InCommands.TextureAccesses.size() &&
	    std::equal(InCache.TextureAccesses.begin(), InCache.TextureAccesses.end(), InCommands.TextureAccesses.begin(),
	               [](const auto& InKey, const auto& InAccess)
	               {
		               return InKey.Texture.lock() == InAccess.View.Texture.Payload &&
		                      InKey.FirstMip == InAccess.View.FirstMip && InKey.MipCount == InAccess.View.MipCount &&
		                      InKey.State == InAccess.State;
	               }) &&
	    InCommands.SharedDraws && InCache.Owner.lock() == InCommands.SharedDraws && InCache.Target == Target &&
	    InCache.GraphicsTarget == GraphicsTarget && InCache.ColorTargets.size() == Colors.size() &&
	    std::equal(InCache.ColorTargets.begin(), InCache.ColorTargets.end(), Colors.begin(),
	               [](const auto& InWeak, const auto& InColor)
	               {
		               return InWeak.lock() == InColor.Target.Texture.Payload;
	               }) &&
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
	InCache.GraphicsTarget = GraphicsTarget;
	InCache.ColorTargets.clear();
	for (const auto& Color : Colors)
	{
		InCache.ColorTargets.push_back(Color.Target.Texture.Payload);
	}
	InCache.DepthTarget = InCommands.GetDepthTexture().Payload;
	InCache.Reads.clear();
	InCache.BufferAccesses.clear();
	for (const auto& Access : InCommands.BufferAccesses)
	{
		InCache.BufferAccesses.push_back(
		    {Access.View.Buffer.Payload, Access.View.Offset, Access.View.Size, Access.State});
	}
	InCache.TextureAccesses.clear();
	for (const auto& Access : InCommands.TextureAccesses)
	{
		InCache.TextureAccesses.push_back(
		    {Access.View.Texture.Payload, Access.View.FirstMip, Access.View.MipCount, Access.State});
	}
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
	InState.GraphicsRootBinds += InPlan.Binds.Root;
	InState.GraphicsHeapBinds += InPlan.Binds.Heap;
	InState.GraphicsConstantBinds += InPlan.Binds.Constant;
	InState.GraphicsTableBinds += InPlan.Binds.Table;
	InState.GraphicsPipelineBinds += InPlan.Binds.Pipeline;
	InState.GraphicsGeometryBinds += InPlan.Binds.Geometry;
	InState.GraphicsDynamicBinds += InPlan.Binds.Dynamic;
	HYP_PERF_PLOT(Rhi, GraphicsRootBinds, double(InPlan.Binds.Root));
	HYP_PERF_PLOT(Rhi, GraphicsHeapBinds, double(InPlan.Binds.Heap));
	HYP_PERF_PLOT(Rhi, GraphicsConstantBinds, double(InPlan.Binds.Constant));
	HYP_PERF_PLOT(Rhi, GraphicsTableBinds, double(InPlan.Binds.Table));
	HYP_PERF_PLOT(Rhi, GraphicsPipelineBinds, double(InPlan.Binds.Pipeline));
	HYP_PERF_PLOT(Rhi, GraphicsGeometryBinds, double(InPlan.Binds.Geometry));
	HYP_PERF_PLOT(Rhi, GraphicsDynamicBinds, double(InPlan.Binds.Dynamic));
}
} // namespace Hyperion
