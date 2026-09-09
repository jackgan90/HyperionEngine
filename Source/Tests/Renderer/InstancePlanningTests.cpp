#include "Renderer/InstanceBatchSupport.h"
#include <algorithm>

namespace Hyperion::InstanceTests
{
FRenderSceneSnapshot Snapshot(FFixture& InFixture, std::size_t InCount)
{
	FRenderSceneSnapshot Result;
	Result.View = InFixture.View;
	Result.DepthFormat = ERHIDepthFormat::D32S8;
	for (std::size_t Index = 0; Index < InCount; ++Index)
	{
		FRenderItem Item;
		Item.State.Resource = InFixture.Resource;
		Item.State.Surface = InFixture.Resource->GetMaterial(0);
		Item.Primitive = {99, static_cast<std::uint32_t>(Index), 1};
		Item.Group = Index;
		Item.LocalItemId = 0;
		Item.Lifetime = std::make_shared<int>(0);
		Item.Context.Scopes[static_cast<std::size_t>(EMaterialScope::Object)] = {{99, 1, {Index}}, Item.Lifetime};
		Item.Context.ObjectParameters = {
		    {"Placement",
		     FMaterialValue::Float(FVec4{-.75f + float(Index % 4) * .5f, -.45f + float(Index / 4) * .9f, 0, 1})}};
		const auto Compiled = Item.State.Surface->GetCompiled();
		Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
		    Item.State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), Item.Context));
		Result.Items.push_back(std::move(Item));
	}
	return Result;
}

std::vector<FPassCommands> Prepare(FFixture& InFixture, FRenderBatchSystem& InBatches, FRenderSceneSnapshot InSnapshot)
{
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              InSnapshot.Batches = InBatches.Build(InSnapshot);
	                                              }));
	std::vector<FColorPass> Passes;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [&]
	                                              {
		                                              Passes =
		                                                  InFixture.Session->GetResources().BuildPasses(InSnapshot);
	                                              }));
	FRenderGraph Graph;
	FColorPass Clear;
	Clear.Commands.Name = "Clear planning";
	Clear.Load = EColorLoad::Clear;
	Graph.Add(std::move(Clear));
	for (auto& Pass : Passes)
	{
		Graph.Add(std::move(Pass));
	}
	return Graph.Compile();
}

namespace
{
std::shared_ptr<const FRenderBatchPlan> Plan(FFixture& InFixture, FRenderBatchSystem& InBatches,
                                             const FRenderSceneSnapshot& InSnapshot)
{
	std::shared_ptr<const FRenderBatchPlan> Result;
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Result = InBatches.Build(InSnapshot);
	                                              }));
	std::vector<unsigned> Coverage(InSnapshot.Items.size());
	for (const auto& Batch : Result->Batches)
	{
		for (const auto Index : Batch.Items)
		{
			HYP_CHECK(Index < Coverage.size());
			++Coverage[Index];
		}
	}
	HYP_CHECK(std::all_of(Coverage.begin(), Coverage.end(),
	                      [](auto InCount)
	                      {
		                      return InCount == 1;
	                      }));
	return Result;
}

class FTwoItems final : public IRenderBatchStrategy
{
public:
	explicit FTwoItems(std::shared_ptr<unsigned> InCalls) : Calls(std::move(InCalls))
	{
	}

	FRenderBatchDecision Evaluate(const FRenderBatchCandidate& InCandidate,
	                              const FRHICapabilities& InCaps) const override
	{
		++*Calls;
		auto Result = Strategy.Evaluate(InCandidate, InCaps);
		Result.Capacity = std::min(2U, Result.Capacity);
		return Result;
	}

	bool CanCombine(const FRenderBatchCandidate& InA, const FRenderBatchCandidate& InB) const override
	{
		return Strategy.CanCombine(InA, InB);
	}

private:
	std::shared_ptr<unsigned> Calls;
	FInstanceBatchStrategy Strategy;
};

void CheckStrategies(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto OtherCalls = std::make_shared<unsigned>(0);
	auto FirstCalls = std::make_shared<unsigned>(0);
	Batches.Register(std::make_unique<FTwoItems>(OtherCalls));
	Batches.Register(std::make_unique<FTwoItems>(FirstCalls));
	const auto Items = Snapshot(InFixture, 8);
	const auto Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Batches.size() == 4 && *FirstCalls == 8 && *OtherCalls == 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
	auto Caps = InFixture.Device->GetCapabilities();
	Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = false;
	FRenderBatchSystem Disabled(InFixture.Tasks, Caps);
	HYP_CHECK(Plan(InFixture, Disabled, Items)->Batches.size() == 8);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Disabled.Clear();
	                                              }));
}

void PublishShared(FRenderSceneSnapshot& InSnapshot, std::string_view InName, FMaterialValue InValue)
{
	const auto Schema = InSnapshot.Items.front().State.Surface->GetCompiled()->Interface.Schema;
	std::vector<std::optional<std::shared_ptr<const FMaterialValue>>> Values(Schema->GetParameters().size());
	Values[Schema->Find(InName).Index] = std::make_shared<const FMaterialValue>(std::move(InValue));
	const auto Shared = std::make_shared<const FMaterialValueTable::FSharedValues>(std::move(Values));
	for (auto& Item : InSnapshot.Items)
	{
		auto Updated = std::make_shared<FResolvedMaterialParameters>(*Item.ResolvedParameters);
		Updated->Values.SetShared(Shared);
		Item.ResolvedParameters = std::move(Updated);
	}
}

void CheckSharedRefresh(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 8);
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{1, 1, 1, 1}));
	const auto Original = Plan(InFixture, Batches, Items);
	const auto Frozen = Items;
	const auto OldDraws = Prepare(InFixture, Batches, Items);
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{.25f, 1, 1, 1}));
	const auto Refreshed = Plan(InFixture, Batches, Items);
	HYP_CHECK(Refreshed->Statistics.PlanReuses == 1 && Refreshed->Statistics.RebuiltChunks == 0);
	HYP_CHECK(Refreshed->Batches[0].Instances == Original->Batches[0].Instances);
	const auto NewDraws = Prepare(InFixture, Batches, Items);
	HYP_CHECK(InFixture.Draw(OldDraws).Rgba != InFixture.Draw(NewDraws).Rgba);
	const auto Tint = Items.Items[0].State.Surface->GetCompiled()->Interface.Schema->Find("SharedTint").Index;
	HYP_CHECK(*Frozen.Items[0].ResolvedParameters->Values[Tint] == FMaterialValue::Float(FVec4{1, 1, 1, 1}));
	// A different overlay in one member must split compatibility, even though every local page is unchanged.
	Items.Items[1].ResolvedParameters = Frozen.Items[1].ResolvedParameters;
	const auto Split = Plan(InFixture, Batches, Items);
	HYP_CHECK(Split->Statistics.PlanReuses == 0 && Split->Batches.size() > Refreshed->Batches.size());
	// View-dependent data can also appear in an instance block; it must rebuild its payload.
	PublishShared(Items, "Placement", FMaterialValue::Float(FVec4{0, 0, 0, 1}));
	Plan(InFixture, Batches, Items);
	PublishShared(Items, "Placement", FMaterialValue::Float(FVec4{.5f, 0, 0, 1}));
	HYP_CHECK(Plan(InFixture, Batches, Items)->Statistics.RebuiltChunks > 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));

	FRenderBatchSystem Custom(InFixture.Tasks, InFixture.Device->GetCapabilities());
	const auto Calls = std::make_shared<unsigned>(0);
	Custom.Register(std::make_unique<FTwoItems>(Calls));
	Items = Frozen;
	Plan(InFixture, Custom, Items);
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{.5f, 1, 1, 1}));
	HYP_CHECK(Plan(InFixture, Custom, Items)->Statistics.PlanReuses == 0 && *Calls == 16);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Custom.Clear();
	                                              }));
}

void CheckBudget(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {3, 1, 65536});
	auto Items = Snapshot(InFixture, 8);
	auto Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedChunks <= 1 && Result->Statistics.CachedBytes <= 65536);
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedChunks <= 1 && Result->Statistics.Evictions > 0);
	Items.Items.erase(Items.Items.begin(), Items.Items.begin() + 4);
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Batches.size() == 1 && Result->Batches[0].Instances->InstanceCount == 4);
	Items.Items.clear();
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedChunks == 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckPlanValueRetirement(FFixture& InFixture, bool bInInvalidate)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {16, 16, 65536});
	std::weak_ptr<const FMaterialTextureSource> Released;
	FRenderSceneSnapshot Retained;
	{
		auto Items = Snapshot(InFixture, 8);
		const auto Source = std::make_shared<const FMaterialTextureSource>(
		    EMaterialTextureEncoding::Linear, std::vector<FMaterialTextureMip>{{1, 1, {200, 120, 80, 255}}});
		Released = Source;
		for (auto& Item : Items.Items)
		{
			const auto Compiled = Item.State.Surface->GetCompiled();
			Item.Context.ObjectParameters.push_back(
			    {"Maps",
			     FMaterialValue::Array({FMaterialValue::FromTexture(Source), FMaterialValue::FromTexture(Source)})});
			Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
			    Item.State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), Item.Context));
		}
		HYP_CHECK(!Plan(InFixture, Batches, Items)->Batches.empty());
		HYP_CHECK(Plan(InFixture, Batches, Items)->Statistics.PlanReuses == 1);
		Retained = Items;
	}
	if (bInInvalidate)
	{
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
		                                              [&]
		                                              {
			                                              Batches.InvalidatePlans();
		                                              }));
		HYP_CHECK(!Released.expired()); // An independently retained frame keeps its original source.
		Retained = {};
		HYP_CHECK(Released.expired()); // No subsequent family/build is needed after invalidation.
	}
	else
	{
		Retained = {};
		HYP_CHECK(!Released.expired());
		auto OtherView = Snapshot(InFixture, 8);
		++OtherView.View.Identity;
		Plan(InFixture, Batches, OtherView);
		HYP_CHECK(Released.expired());
		auto ThirdView = Snapshot(InFixture, 8);
		ThirdView.View.Identity = OtherView.View.Identity + 1;
		Plan(InFixture, Batches, ThirdView);
		// The two live views fit only if retirement returned the dead view's eight-item budget.
		HYP_CHECK(Plan(InFixture, Batches, OtherView)->Statistics.PlanReuses == 1);
		HYP_CHECK(Plan(InFixture, Batches, ThirdView)->Statistics.PlanReuses == 1);
	}
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void SelectMaterial(FRenderItem& InItem, std::shared_ptr<const FRenderMaterial> InMaterial)
{
	InItem.State.Surface = std::move(InMaterial);
	const auto Compiled = InItem.State.Surface->GetCompiled();
	InItem.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(ResolveMaterialBindingContext(
	    InItem.State.Surface->GetSnapshot(), *Compiled, Compiled->GetPass(), InItem.Context));
}

void CheckCompatibility(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 4);
	const auto Original = Items.Items[1];
	auto Desc = Original.State.Surface->GetSnapshot()->Definition->GetDescription();
	Desc.Parameters[1].Default = Payload(.75f);
	SelectMaterial(Items.Items[1], InFixture.Material(Desc));
	HYP_CHECK(Plan(InFixture, Batches, Items)->Batches.size() == 1); // Different definitions and numeric values.
	Items.Items[1].DynamicState = FMaterialDynamicState{2};
	HYP_CHECK(Plan(InFixture, Batches, Items)->Batches.size() == 2);
	Items.Items[1] = Original;
	Items.Items[1].State.Section = 1;
	HYP_CHECK(Plan(InFixture, Batches, Items)->Batches.size() == 2);
	Items.Items[1] = Original;
	Items.Items[1].State.World = Scale({-1, 1, 1});
	HYP_CHECK(Plan(InFixture, Batches, Items)->Batches.size() == 2);
	Items.Items[1] = Original;
	for (const auto Queue : {EMaterialQueue::Transparent, EMaterialQueue::Overlay})
	{
		Desc.Passes[0].Queue = Queue;
		SelectMaterial(Items.Items[1], InFixture.Material(Desc));
		const auto Result = Plan(InFixture, Batches, Items);
		HYP_CHECK(Result->Batches.size() == 3 && Result->Batches[0].Items.size() == 1 &&
		          Result->Batches[2].Items.size() == 2);
	}
	Items.Items[1] = Original;
	Desc = Original.State.Surface->GetSnapshot()->Definition->GetDescription();
	Desc.Passes[0].State.bStencil = true;
	SelectMaterial(Items.Items[1], InFixture.Material(Desc));
	HYP_CHECK(Plan(InFixture, Batches, Items)->Batches.size() == 3);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}
} // namespace

void RunInstancePlanningTests(FFixture& InFixture)
{
	CheckStrategies(InFixture);
	CheckSharedRefresh(InFixture);
	CheckBudget(InFixture);
	CheckPlanValueRetirement(InFixture, false);
	CheckPlanValueRetirement(InFixture, true);
	CheckCompatibility(InFixture);
}
} // namespace Hyperion::InstanceTests
