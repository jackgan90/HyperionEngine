#include "D3D12DrawPlanInspection.h"
#include "Hyperion/RHI/RHIDevice.h"
#include <ostream>

namespace Hyperion::D3D12Private
{
namespace
{
void Check(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

const FD3D12DeviceState& DeviceState(const FBuffer& InAnchor)
{
	const auto* Buffer = dynamic_cast<const FD3D12Buffer*>(InAnchor.Payload.get());
	Check(Buffer && Buffer->State && Buffer->GetDeviceIdentity() == Buffer->State.get(),
	      "Native draw fixture requires a checked D3D12 buffer owner");
	return *Buffer->State;
}

std::shared_ptr<const FPassCommands> Publish(std::vector<FDrawPacket> InDraws)
{
	FPassCommands Commands;
	Commands.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Commands.Draws = std::move(InDraws);
	Commands.ShareDraws();
	return std::make_shared<const FPassCommands>(std::move(Commands));
}

std::shared_ptr<const FD3D12DrawPlan> Prepare(const FD3D12DeviceState& InState,
                                              const std::shared_ptr<const FPassCommands>& InCommands,
                                              FD3D12DrawCache& InCache)
{
	Check(!PrepareNativeDraws(InState, InCommands, InCache), "First stream observation must validate without a plan");
	const auto Plan = PrepareNativeDraws(InState, InCommands, InCache);
	Check(bool(Plan), "Second stream observation must build a native plan");
	Check(PrepareNativeDraws(InState, InCommands, InCache) == Plan, "Third stream observation must reuse its plan");
	return Plan;
}

struct FObservations
{
	std::vector<FNativeDrawObservation> Values;
	std::size_t Position{};

	const FNativeDrawObservation& Take(ENativeDrawObservation InOperation)
	{
		Check(Position < Values.size(), "Native plan omitted an expected command");
		const auto& Value = Values[Position++];
		Check(Value.Operation == InOperation, "Native plan command order differs from the fixed fixture");
		return Value;
	}

	void Complete() const
	{
		Check(Position == Values.size(), "Native plan emitted unexpected redundant commands");
	}
};

void CheckDraw(FObservations& InValues, UINT InInstances, UINT InFirstIndex = 3)
{
	const auto& Draw = InValues.Take(ENativeDrawObservation::Draw);
	Check(Draw.IndexCount == 3 && Draw.InstanceCount == InInstances && Draw.FirstIndex == InFirstIndex &&
	          Draw.VertexOffset == -1 && Draw.FirstInstance == 0,
	      "Native indexed draw arguments differ from fixed fixture literals");
}

void CheckDynamic(FObservations& InValues, bool bInAlternate)
{
	Check(InValues.Take(ENativeDrawObservation::Stencil).StencilReference == (bInAlternate ? 19U : 7U),
	      "Native stencil reference differs from fixture");
	const auto Blend = bInAlternate ? std::array{.75f, .25f, .5f, 1.f} : std::array{.25f, .5f, .75f, 1.f};
	Check(InValues.Take(ENativeDrawObservation::Blend).BlendConstants == Blend, "Native blend constants differ");
	const auto Rect = InValues.Take(ENativeDrawObservation::Scissor).Scissor;
	Check(Rect.left == (bInAlternate ? 32 : -7) && Rect.top == (bInAlternate ? 0 : -11) &&
	          Rect.right == (bInAlternate ? 64 : 32) && Rect.bottom == 64,
	      "Native scissor lost signed coordinates or changed fixture bounds");
}

void CheckGeometry(FObservations& InValues, const FD3D12DeviceState& InState, const FDrawPacket& InDraw,
                   bool bInAlternate)
{
	const auto& VertexBuffer = NativeResource<FD3D12Buffer>(InDraw.Vertices.Payload, &InState);
	const auto& IndexBuffer = NativeResource<FD3D12Buffer>(InDraw.Indices.Payload, &InState);
	Check(VertexBuffer.Size == (bInAlternate ? 48U : 24U) && IndexBuffer.Size == (bInAlternate ? 36U : 24U),
	      "Native fixture resource sizes differ from authored data");
	const auto Vertices = InValues.Take(ENativeDrawObservation::Vertices).Vertices;
	Check(Vertices.BufferLocation == VertexBuffer.Resource->GetGPUVirtualAddress() &&
	          Vertices.SizeInBytes == (bInAlternate ? 48U : 24U) && Vertices.StrideInBytes == (bInAlternate ? 16U : 8U),
	      "Native vertex view differs from resource address and fixed size/stride");
	const auto Indices = InValues.Take(ENativeDrawObservation::Indices).Indices;
	Check(Indices.BufferLocation == IndexBuffer.Resource->GetGPUVirtualAddress() &&
	          Indices.SizeInBytes == (bInAlternate ? 36U : 24U) && Indices.Format == DXGI_FORMAT_R32_UINT,
	      "Native index view differs from resource address and fixed size/format");
}

void CheckArguments(FObservations& InValues, const FD3D12DeviceState& InState, const FDrawPacket& InDraw,
                    bool bInAlternate, bool bInWhite)
{
	const auto& Pipeline = NativeResource<FD3D12Pipeline>(InDraw.Pipeline.Payload, &InState);
	const auto& Layout = NativeResource<FD3D12BindingLayout>(Pipeline.Layout.Payload, &InState);
	const auto& Set = NativeResource<FD3D12BindingSet>(InDraw.Bindings.Payload, &InState);
	Check(Layout.Slots.size() == 3 && Layout.Tables.size() == 2 && Set.Tables.size() == 2,
	      "Native fixture requires exactly one constant and two descriptor tables");
	Check(Layout.Slots[bInAlternate ? 2 : 0].RootParameter == (bInAlternate ? 2U : 0U) &&
	          Layout.Tables[0].RootParameter == (bInAlternate ? 0U : 1U) && !Layout.Tables[0].bSampler &&
	          Layout.Tables[1].RootParameter == (bInAlternate ? 1U : 2U) && Layout.Tables[1].bSampler,
	      "Fixture layouts no longer have their fixed root-parameter contracts");
	const auto& Slice = InDraw.ConstantBindings.at(0).Slice;
	const auto& Buffer = NativeResource<FD3D12Buffer>(Slice.Buffer.Payload, &InState);
	Check(Slice.Offset == (bInWhite ? 256U : 0U) && Slice.Size == 16, "Fixture constant subrange changed");
	const auto& Constant = InValues.Take(ENativeDrawObservation::Constant);
	Check(Constant.RootParameter == (bInAlternate ? 2U : 0U) &&
	          Constant.ConstantAddress == Buffer.Resource->GetGPUVirtualAddress() + (bInWhite ? 256U : 0U),
	      "Native constant root slot or GPU address differs from fixed subrange");
	const auto& Texture = InValues.Take(ENativeDrawObservation::Table);
	Check(Texture.RootParameter == (bInAlternate ? 0U : 1U) &&
	          Texture.TableHandle.ptr == InState.ResourceTables.Gpu(Set.Tables[0].Offset).ptr,
	      "Native texture table root slot or GPU handle differs from fixture allocation");
	const auto& Sampler = InValues.Take(ENativeDrawObservation::Table);
	Check(Sampler.RootParameter == (bInAlternate ? 1U : 2U) &&
	          Sampler.TableHandle.ptr == InState.SamplerTables.Gpu(Set.Tables[1].Offset).ptr,
	      "Native sampler table root slot or GPU handle differs from fixture allocation");
}

void CheckInitial(FObservations& InValues, const FD3D12DeviceState& InState, const FDrawPacket& InA)
{
	const auto& Pipeline = NativeResource<FD3D12Pipeline>(InA.Pipeline.Payload, &InState);
	Check(InValues.Take(ENativeDrawObservation::Pipeline).Pipeline == Pipeline.Pipeline.Get(),
	      "Native initial pipeline differs from fixture PSO");
	Check(InValues.Take(ENativeDrawObservation::Topology).Topology == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	      "Native topology differs from fixed triangle-list fixture");
	CheckDynamic(InValues, false);
	CheckGeometry(InValues, InState, InA, false);
	Check(InValues.Take(ENativeDrawObservation::Root).Root == Pipeline.Root.Get(), "Native initial root differs");
	const std::array Heaps{InState.ResourceTables.GetHeap(), InState.SamplerTables.GetHeap()};
	Check(InValues.Take(ENativeDrawObservation::Heaps).Heaps == Heaps, "Native descriptor heap pair differs");
	CheckArguments(InValues, InState, InA, false, false);
	CheckDraw(InValues, 2);
}

void CheckSecond(FObservations& InValues, const FD3D12DeviceState& InState, const FDrawPacket& InB)
{
	const auto& Pipeline = NativeResource<FD3D12Pipeline>(InB.Pipeline.Payload, &InState);
	Check(InValues.Take(ENativeDrawObservation::Pipeline).Pipeline == Pipeline.Pipeline.Get(),
	      "Native alternate pipeline differs from fixture PSO");
	CheckDynamic(InValues, true);
	CheckGeometry(InValues, InState, InB, true);
	Check(InValues.Take(ENativeDrawObservation::Root).Root == Pipeline.Root.Get(), "Native alternate root differs");
	CheckArguments(InValues, InState, InB, true, true);
	CheckDraw(InValues, 3);
}

void CheckMetrics(const FD3D12DrawPlan& InPlan, std::size_t InCount, const FNativeDrawBindCounts& InBinds)
{
	const auto Metrics = DescribeNativeDrawPlan(&InPlan);
	Check(Metrics.bHasPlan && Metrics.CommandCount == InCount && Metrics.CommandCapacity >= InCount &&
	          Metrics.CommandSize > 0 && Metrics.CommandAlignment > 0 &&
	          Metrics.UsedBytes == Metrics.CommandCount * Metrics.CommandSize &&
	          Metrics.CapacityBytes == Metrics.CommandCapacity * Metrics.CommandSize && Metrics.Binds == InBinds,
	      "Native plan size, storage accounting or fixed bind counts differ");
}

void CheckDistinctResources(const FD3D12DeviceState& InState, const FDrawPacket& InA, const FDrawPacket& InB,
                            const FDrawPacket& InA2)
{
	const auto& A = NativeResource<FD3D12Pipeline>(InA.Pipeline.Payload, &InState);
	const auto& B = NativeResource<FD3D12Pipeline>(InB.Pipeline.Payload, &InState);
	const auto& SetA = NativeResource<FD3D12BindingSet>(InA.Bindings.Payload, &InState);
	const auto& SetB = NativeResource<FD3D12BindingSet>(InB.Bindings.Payload, &InState);
	const auto& SetA2 = NativeResource<FD3D12BindingSet>(InA2.Bindings.Payload, &InState);
	Check(A.Pipeline.Get() != B.Pipeline.Get() && A.Root.Get() != B.Root.Get() && InA.Vertices != InB.Vertices &&
	          InA.Indices != InB.Indices,
	      "Switching fixture must use distinct native pipelines, roots and geometry");
	Check(SetA.Tables.at(0).Offset != SetB.Tables.at(0).Offset &&
	          SetA.Tables.at(1).Offset != SetB.Tables.at(1).Offset &&
	          SetA.Tables.at(0).Offset != SetA2.Tables.at(0).Offset &&
	          SetA.Tables.at(1).Offset != SetA2.Tables.at(1).Offset,
	      "Switching fixture descriptor table handles must actually differ");
	Check(InA.ConstantBindings.at(0).Slice.Buffer == InB.ConstantBindings.at(0).Slice.Buffer,
	      "Switching fixture must compare distinct subranges of the same constant page");
}

void CheckSequence(const FD3D12DeviceState& InState, const FDrawPacket& InA, const FDrawPacket& InB,
                   const FDrawPacket& InA2)
{
	FD3D12DrawCache Cache;
	const auto AB = Publish({InA, InB});
	const auto PlanAB = Prepare(InState, AB, Cache);
	CheckMetrics(*PlanAB, 24, {2, 1, 2, 4, 2, 5, 6});
	FObservations ValuesAB{InspectNativeDrawPlan(*PlanAB)};
	CheckInitial(ValuesAB, InState, InA);
	CheckSecond(ValuesAB, InState, InB);
	ValuesAB.Complete();
	const auto AA = Publish({InA, InA});
	const auto PlanAA = Prepare(InState, AA, Cache);
	CheckMetrics(*PlanAA, 14, {1, 1, 1, 2, 1, 3, 3});
	FObservations ValuesAA{InspectNativeDrawPlan(*PlanAA)};
	CheckInitial(ValuesAA, InState, InA);
	CheckDraw(ValuesAA, 2);
	ValuesAA.Complete();
	const auto AA2 = Publish({InA, InA2});
	const auto PlanAA2 = Prepare(InState, AA2, Cache);
	CheckMetrics(*PlanAA2, 17, {1, 1, 2, 4, 1, 3, 3});
	FObservations ValuesAA2{InspectNativeDrawPlan(*PlanAA2)};
	CheckInitial(ValuesAA2, InState, InA);
	CheckArguments(ValuesAA2, InState, InA2, false, true);
	CheckDraw(ValuesAA2, 2);
	ValuesAA2.Complete();
	const auto Empty = Publish({});
	const auto EmptyPlan = Prepare(InState, Empty, Cache);
	CheckMetrics(*EmptyPlan, 0, {});
	Check(InspectNativeDrawPlan(*EmptyPlan).empty(), "Empty draw stream must produce an empty native plan");
	const auto Missing = DescribeNativeDrawPlan(nullptr);
	Check(!Missing.bHasPlan && Missing.CommandCount == 0 && Missing.CommandCapacity == 0,
	      "Missing plan must remain distinguishable from a built empty plan");
}

void CheckChangedStream(const FD3D12DeviceState& InState, const FDrawPacket& InA, const FDrawPacket& InB)
{
	FD3D12DrawCache Cache;
	const auto Original = Publish({InA, InB});
	const auto OriginalPlan = Prepare(InState, Original, Cache);
	auto Replacement = *Original;
	Replacement.MaterializeDraws();
	Replacement.Draws[0].FirstIndex = 0;
	Replacement.ShareDraws();
	const auto Changed = std::make_shared<const FPassCommands>(std::move(Replacement));
	const auto ChangedPlan = Prepare(InState, Changed, Cache);
	Check(ChangedPlan != OriginalPlan, "A new immutable owner must build its own plan");
	const auto OldValues = InspectNativeDrawPlan(*OriginalPlan);
	const auto NewValues = InspectNativeDrawPlan(*ChangedPlan);
	Check(OldValues.at(12).Operation == ENativeDrawObservation::Draw && OldValues[12].FirstIndex == 3 &&
	          NewValues.at(12).Operation == ENativeDrawObservation::Draw && NewValues[12].FirstIndex == 0,
	      "Retained old and replacement native plans must preserve independent indexed arguments");
	Check(Original->GetDraws()[0].FirstIndex == 3 && Changed->GetDraws()[0].FirstIndex == 0,
	      "Publishing replacement commands must not mutate the original stream");
}

void CheckCacheKeys(IRHIDevice& InDevice, const FD3D12DeviceState& InState, const FDrawPacket& InA)
{
	FD3D12DrawCache Cache;
	const auto Original = Publish({InA});
	const auto OriginalPlan = Prepare(InState, Original, Cache);
	const auto ColorA = InDevice.CreateColorTexture({64, 64, ERHIColorFormat::Rgba8Unorm});
	const auto ColorB = InDevice.CreateColorTexture({64, 64, ERHIColorFormat::Rgba8Unorm});
	const auto Buffer = InDevice.CreateBuffer({32, BufferUsage(ERHIBufferUsage::StructuredRead)});
	InDevice.WaitIdle();
	auto Target = *Original;
	Target.Color->Target = FRenderTarget::FromTexture(ColorA);
	const auto TextureTarget = std::make_shared<const FPassCommands>(Target);
	Prepare(InState, TextureTarget, Cache);
	Target.Color->Target = FRenderTarget::FromTexture(ColorB);
	Prepare(InState, std::make_shared<const FPassCommands>(Target), Cache);
	auto Sampled = Target;
	Sampled.SampledTextures = {ColorA};
	Prepare(InState, std::make_shared<const FPassCommands>(Sampled), Cache);
	auto BufferRead = Sampled;
	BufferRead.BufferAccesses = {{{Buffer, ERHIBufferViewKind::Structured, 0, 32, 4}, EResourceState::ShaderRead}};
	Prepare(InState, std::make_shared<const FPassCommands>(BufferRead), Cache);
	auto TextureRead = BufferRead;
	TextureRead.TextureAccesses = {{{ColorA, 0, 1}, EResourceState::ShaderRead}};
	Prepare(InState, std::make_shared<const FPassCommands>(TextureRead), Cache);
	Check(Original->SharedDraws == TextureRead.SharedDraws, "Cache-key fixture must retain the exact same draw owner");
	CheckMetrics(*OriginalPlan, 13, {1, 1, 1, 2, 1, 3, 3});
}

void WriteMetrics(const FNativeDrawPlanMetrics& InMetrics, std::ostream& InOutput)
{
	InOutput << "\"command_size\":" << InMetrics.CommandSize << ",\"command_alignment\":" << InMetrics.CommandAlignment
	         << ",\"command_count\":" << InMetrics.CommandCount << ",\"command_capacity\":" << InMetrics.CommandCapacity
	         << ",\"used_bytes\":" << InMetrics.UsedBytes << ",\"capacity_bytes\":" << InMetrics.CapacityBytes;
	const auto& Binds = InMetrics.Binds;
	InOutput << ",\"binds\":{\"root\":" << Binds.Root << ",\"heap\":" << Binds.Heap
	         << ",\"constant\":" << Binds.Constant << ",\"table\":" << Binds.Table << ",\"pipeline\":" << Binds.Pipeline
	         << ",\"geometry\":" << Binds.Geometry << ",\"dynamic\":" << Binds.Dynamic << '}';
}
} // namespace

void CheckNativeDrawPlanFixture(IRHIDevice& InDevice, const FBuffer& InAnchor, const FDrawPacket& InA,
                                const FDrawPacket& InB, const FDrawPacket& InA2)
{
	const auto& State = DeviceState(InAnchor);
	CheckDistinctResources(State, InA, InB, InA2);
	CheckSequence(State, InA, InB, InA2);
	CheckChangedStream(State, InA, InB);
	CheckCacheKeys(InDevice, State, InA);
}

void CheckNativeDrawPlanLifetime(IRHIDevice& InDevice, FDrawPacket InDraw)
{
	const auto& State = DeviceState(InDraw.Vertices);
	auto Page = InDevice.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const FVec4 Color{1, 0, 0, 1};
	InDraw.ConstantBindings = {{0, InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Color, 1)))}};
	auto Owner = Publish({InDraw});
	InDraw = {};
	Page = {};
	FD3D12DrawCache Cache;
	const auto Plan = Prepare(State, Owner, Cache);
	const auto& NestedPage = Owner->GetDraws()[0].ConstantBindings[0].Slice.Buffer;
	Check(NestedPage.Payload.use_count() == 1, "Live-stream lifetime fixture must have a sole nested page handle");
	bool bRejected = false;
	try
	{
		InDevice.ResetConstantBuffer(NestedPage);
	}
	catch (const std::logic_error&)
	{
		bRejected = true;
	}
	Check(bRejected, "Published native-plan owner must prevent premature constant-page reuse");
	Page = NestedPage;
	Owner.reset();
	InDevice.WaitIdle();
	Check(Page.Payload.use_count() == 1 && Cache.Owner.expired() && bool(Plan) && bool(Cache.Plan),
	      "Retained plan/cache must not extend constant-page or immutable-owner lifetime");
	InDevice.ResetConstantBuffer(Page);
	Check(NativeResource<FD3D12Buffer>(Page.Payload, &State).Published.empty(),
	      "Constant page must become reusable while the borrowing plan still exists");
}

void WriteNativeDrawPlanMetrics(const FBuffer& InAnchor, const std::shared_ptr<const FPassCommands>& InPrepared,
                                std::ostream& InOutput)
{
	const auto& State = DeviceState(InAnchor);
	auto Copy = *InPrepared;
	Copy.ShareDraws();
	const auto Owner = std::make_shared<const FPassCommands>(std::move(Copy));
	FD3D12DrawCache Cache;
	const auto Plan = Prepare(State, Owner, Cache);
	InOutput << "{\"draws\":" << Owner->GetDraws().size() << ',';
	WriteMetrics(DescribeNativeDrawPlan(Plan.get()), InOutput);
	auto Replacement = *Owner;
	Replacement.MaterializeDraws();
	if (!Replacement.Draws.empty())
	{
		++Replacement.Draws[0].DynamicState.StencilReference;
	}
	Replacement.ShareDraws();
	const auto Changed = std::make_shared<const FPassCommands>(std::move(Replacement));
	const auto ChangedPlan = Prepare(State, Changed, Cache);
	Check(ChangedPlan != Plan, "Measurement replacement stream must build a distinct retained plan");
	if (!Owner->GetDraws().empty())
	{
		const auto Before = InspectNativeDrawPlan(*Plan);
		const auto After = InspectNativeDrawPlan(*ChangedPlan);
		Check(Before.at(2).Operation == ENativeDrawObservation::Stencil &&
		          After.at(2).Operation == ENativeDrawObservation::Stencil &&
		          Before[2].StencilReference == Owner->GetDraws()[0].DynamicState.StencilReference &&
		          After[2].StencilReference == Changed->GetDraws()[0].DynamicState.StencilReference &&
		          After[2].StencilReference == Before[2].StencilReference + 1,
		      "Measurement replacement plan must contain its changed stencil argument");
	}
	InOutput << ",\"lifecycle\":{\"first_null\":true,\"second_built\":true,\"third_reused\":true,"
	            "\"new_owner_first_null\":true,\"new_owner_second_built\":true,\"old_plan_unchanged\":true}}";
}
} // namespace Hyperion::D3D12Private
