#include "Hyperion/Renderer/MaterialPipeline.h"
#include "Renderer/InstanceBatchSupport.h"

namespace Hyperion::InstanceTests
{
namespace
{
FMaterialDescription NativeLimitDescription(bool bInConstantLimit)
{
	FMaterialDescription Result;
	Result.Name = "Optional instance native limits";
	FMaterialPass Pass;
	Pass.Vertex = {"InstanceLimits.hlsl", "VSMain"};
	Pass.Pixel = {"InstanceLimits.hlsl", "PSMain"};
	const std::string Define = bInConstantLimit ? "HYP_TEST_INSTANCE_CONSTANT_LIMIT" : "HYP_TEST_INSTANCE_INPUT";
	Pass.Vertex.Defines = {{Define, "1"}};
	Pass.Pixel.Defines = Pass.Vertex.Defines;
	Pass.InstanceArrays = {{"InstanceData", "Records"}};
	Pass.bAllowBatchReordering = true;
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = true;
	Pass.State.Cull = EMaterialCull::None;
	FMaterialParameterDeclaration Placement;
	Placement.Name = "Placement";
	Placement.Default = FMaterialValue::Float(FVec4{0, 0, 0, 1});
	Placement.Type = Placement.Default->Type;
	Placement.Targets = {"InstanceData.Placement"};
	if (bInConstantLimit)
	{
		for (unsigned Index = 1; Index <= 14; ++Index)
		{
			const auto Block = "Limit" + std::to_string(Index);
			Pass.InstanceArrays.push_back({Block, "Extra" + std::to_string(Index)});
			Placement.Targets.push_back(Block + ".Placement");
		}
	}
	Result.Passes.push_back(std::move(Pass));
	Result.Parameters.push_back(std::move(Placement));
	return Result;
}

void CheckOptionalNativeFallback(FFixture& InFixture, bool bInConstantLimit)
{
	// Ordinary shader is valid; only its optional layout or vertex inputs fail native preparation.
	const auto Material = InFixture.Material(NativeLimitDescription(bInConstantLimit));
	const auto Compiled = Material->GetCompiled();
	if (!Compiled->FindInstancePass())
	{
		throw std::runtime_error(
		    "Native-limit fixture requires a compiled optional pass: " +
		    (Compiled->InstanceDiagnostics.empty() ? "no diagnostic" : Compiled->InstanceDiagnostics.front()));
	}
	HYP_CHECK(Material->GetStatus() == ERenderMaterialStatus::Ready && Compiled->FindInstancePass());
	auto Items = Snapshot(InFixture, 4);
	for (auto& Item : Items.Items)
	{
		Item.State.Surface = Material;
		Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(
		    ResolveMaterialBindingContext(Material->GetSnapshot(), *Compiled, Compiled->GetPass(), Item.Context));
	}
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	const auto Fallback = Prepare(InFixture, Batches, Items);
	FRenderBatchStats Stats;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [&]
	                                              {
		                                              Stats = InFixture.Session->GetResources().Statistics().Batches;
	                                              }));
	const auto Reason = bInConstantLimit ? ERenderBatchFallback::Device : ERenderBatchFallback::Preparation;
	HYP_CHECK(Stats.SingleDraws == 4 && Stats.InstancedItems == 0 && Stats.FailedItems == 0);
	HYP_CHECK(Stats.Fallbacks[static_cast<std::size_t>(Reason)] == 4);
	auto Caps = InFixture.Device->GetCapabilities();
	Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = false;
	FRenderBatchSystem Disabled(InFixture.Tasks, Caps);
	const auto Ordinary = Prepare(InFixture, Disabled, Items);
	HYP_CHECK(DrawCount(Fallback) == 4 && InstanceCount(Fallback) == 4);
	HYP_CHECK(InFixture.Draw(Fallback).Rgba == InFixture.Draw(Ordinary).Rgba);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
		                                              Disabled.Clear();
	                                              }));
}

void CheckLateGroupFailure(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 3);
	Items.Items[0].Group = 10;
	Items.Items[1].Group = 20;
	Items.Items[2].Group = 10;
	auto Desc = Items.Items[0].State.Surface->GetSnapshot()->Definition->GetDescription();
	Desc.Passes[0].Vertex.Defines = {{"MISSING_INPUT", "1"}};
	Items.Items[2].State.Surface = InFixture.Material(Desc);
	const auto Compiled = Items.Items[2].State.Surface->GetCompiled();
	Items.Items[2].ResolvedParameters =
	    std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
	        Items.Items[2].State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), Items.Items[2].Context));
	std::array<FRenderDrawResult, 3> Reports;
	for (std::size_t Index = 0; Index < 3; ++Index)
	{
		Items.Items[Index].Report = [&, Index](auto InResult)
		{
			Reports[Index] = std::move(InResult);
		};
	}
	const auto Repaired = Prepare(InFixture, Batches, Items);
	if (DrawCount(Repaired) != 1 || InstanceCount(Repaired) != 1)
	{
		throw std::runtime_error("Group repair draws=" + std::to_string(DrawCount(Repaired)) +
		                         " instances=" + std::to_string(InstanceCount(Repaired)) +
		                         " errors=" + Reports[0].Error + "/" + Reports[1].Error + "/" + Reports[2].Error);
	}
	HYP_CHECK(DrawCount(Repaired) == 1 && InstanceCount(Repaired) == 1);
	HYP_CHECK(!Reports[0].Error.empty() && Reports[1].Error.empty() && !Reports[2].Error.empty());
	Items.Items = {Items.Items[1]};
	const auto Healthy = Prepare(InFixture, Batches, Items);
	HYP_CHECK(InFixture.Draw(Repaired).Rgba == InFixture.Draw(Healthy).Rgba);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckForgedLayout(FFixture& InFixture)
{
	const auto Compiled = InFixture.Resource->GetMaterial(0)->GetCompiled();
	const auto& Pass = *Compiled->FindInstancePass();
	auto Layout = DescribeMaterialLayout(Pass);
	for (auto& Slot : Layout.Slots)
	{
		if (Slot.InstanceStride)
		{
			Slot.InstanceStride *= 2;
			Slot.InstanceCapacity /= 2;
		}
	}
	bool bRejected = false;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [&]
	                                              {
		                                              FPipelineDesc Pipeline;
		                                              Pipeline.Vertex = Pass.Vertex;
		                                              Pipeline.Pixel = Pass.Pixel;
		                                              Pipeline.Layout = InFixture.Device->CreateBindingLayout(Layout);
		                                              Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float3, 0}};
		                                              Pipeline.VertexStride = sizeof(FVec3);
		                                              try
		                                              {
			                                              InFixture.Device->CreatePipeline(Pipeline);
		                                              }
		                                              catch (const std::invalid_argument&)
		                                              {
			                                              bRejected = true;
		                                              }
	                                              }));
	HYP_CHECK(bRejected);
}
} // namespace

void RunInstanceFailureTests(FFixture& InFixture)
{
	InFixture.Emission->Invalid = 6;
	const auto Failed = InFixture.Build();
	HYP_CHECK(DrawCount(Failed) == 0 && InFixture.Statistics.Batches.FailedItems == 8);
	HYP_CHECK(!InFixture.Binding.GetLastDrawResult().Error.empty());
	InFixture.Emission->Invalid.reset();
	const auto Recovered = InFixture.Build();
	HYP_CHECK(DrawCount(Recovered) == 2 && InFixture.Binding.GetLastDrawResult().Error.empty());
	for (const auto Count : {0U, 5U})
	{
		auto Invalid = Recovered;
		Invalid[1].Draws[0].InstanceCount = Count;
		bool bRejected = false;
		try
		{
			InFixture.Draw(Invalid);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
	HYP_CHECK(InFixture.Draw(Recovered).Rgba == InFixture.Draw(InFixture.Build(false)).Rgba);
	CheckLateGroupFailure(InFixture);
	CheckForgedLayout(InFixture);
	InFixture.Emission->Count = 3;
	auto TooShort = InFixture.Build();
	TooShort[1].Draws[0].InstanceCount = 4; // Within shader capacity, but only three records were published.
	bool bRejected = false;
	try
	{
		InFixture.Draw(TooShort);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	InFixture.Emission->Count = 8;
	CheckOptionalNativeFallback(InFixture, true);
	CheckOptionalNativeFallback(InFixture, false);
}
} // namespace Hyperion::InstanceTests
