#include "Renderer/InstanceBatchSupport.h"
#include <algorithm>

namespace Hyperion::InstanceTests
{

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
	std::vector<unsigned> Coverage(InSnapshot.Items.Size());
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

void PublishShared(FRenderSceneSnapshot& InSnapshot, std::string_view InName, FMaterialValue InValue,
                   bool bInSeparate = false)
{
	const auto Schema = InSnapshot.Items.Front().State.Surface->GetCompiled()->Interface.Schema;
	std::vector<std::optional<std::shared_ptr<const FMaterialValue>>> Values(Schema->GetParameters().size());
	Values[Schema->Find(InName).Index] = std::make_shared<const FMaterialValue>(std::move(InValue));
	const auto Shared = std::make_shared<const FMaterialValueTable::FSharedValues>(std::move(Values));
	auto Scopes = std::make_shared<FMaterialResolvedScopes::FValues>();
	for (std::size_t Index = 0; Index < MaterialScopeCount; ++Index)
	{
		(*Scopes)[Index] = InSnapshot.Items.Front().ResolvedParameters->Scopes[Index];
	}
	const auto Update =
	    std::make_shared<const FMaterialSharedParameters>(FMaterialSharedParameters{Shared, {}, Scopes, 0});
	for (auto& Item : InSnapshot.Items)
	{
		if (bInSeparate)
		{
			Item.SharedParameters = Update;
			continue;
		}
		auto Updated = std::make_shared<FResolvedMaterialParameters>(*Item.ResolvedParameters);
		Updated->Values.SetShared(Shared);
		Item.ResolvedParameters = std::move(Updated);
	}
}

void CheckSharedRefresh(FFixture& InFixture, bool bInSeparate)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 8);
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{1, 1, 1, 1}), bInSeparate);
	const auto Original = Plan(InFixture, Batches, Items);
	const auto Frozen = Items;
	const auto OldDraws = Prepare(InFixture, Batches, Items);
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{.25f, 1, 1, 1}), bInSeparate);
	const auto Refreshed = Plan(InFixture, Batches, Items);
	HYP_CHECK(Refreshed->Statistics.PlanReuses == 1 && Refreshed->Statistics.RebuiltChunks == 0);
	HYP_CHECK(Refreshed->Batches[0].Instances == Original->Batches[0].Instances);
	const auto NewDraws = Prepare(InFixture, Batches, Items);
	HYP_CHECK(InFixture.Draw(OldDraws).Rgba != InFixture.Draw(NewDraws).Rgba);
	const auto Tint = Items.Items[0].State.Surface->GetCompiled()->Interface.Schema->Find("SharedTint").Index;
	HYP_CHECK(*Frozen.Items[0].GetMaterialValue(Tint) == FMaterialValue::Float(FVec4{1, 1, 1, 1}));
	// A different overlay in one member must split compatibility, even though every local page is unchanged.
	Items.Items[1].ResolvedParameters = Frozen.Items[1].ResolvedParameters;
	Items.Items[1].SharedParameters = Frozen.Items[1].SharedParameters;
	const auto Split = Plan(InFixture, Batches, Items);
	HYP_CHECK(Split->Statistics.PlanReuses == 0 && Split->Batches.size() > Refreshed->Batches.size());
	// View-dependent data can also appear in an instance block; it must rebuild its payload.
	PublishShared(Items, "Placement", FMaterialValue::Float(FVec4{0, 0, 0, 1}), bInSeparate);
	Plan(InFixture, Batches, Items);
	PublishShared(Items, "Placement", FMaterialValue::Float(FVec4{.5f, 0, 0, 1}), bInSeparate);
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
	PublishShared(Items, "SharedTint", FMaterialValue::Float(FVec4{.5f, 1, 1, 1}), bInSeparate);
	HYP_CHECK(Plan(InFixture, Custom, Items)->Statistics.PlanReuses == 0 && *Calls == 16);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Custom.Clear();
	                                              }));
}

void CheckPreparedInputs(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 12);
	const auto All = Plan(InFixture, Batches, Items);
	HYP_CHECK(All->Statistics.PreparedInputBuilds == 12 && All->Statistics.InstanceContractBuilds == 1);
	const auto Hidden = Items.Items[0];
	Items.Items.Erase(Items.Items.begin(), Items.Items.begin() + 1);
	const auto Visible = Plan(InFixture, Batches, Items);
	HYP_CHECK(Visible->Statistics.PreparedInputBuilds == 0 && Visible->Statistics.PreparedInputReuses == 11);
	HYP_CHECK(Visible->Statistics.InstanceContractBuilds == 0);
	// A new resolved object with identical effective instance values keeps the same prepared payload.
	Items.Items[4].ResolvedParameters =
	    std::make_shared<const FResolvedMaterialParameters>(*Items.Items[4].ResolvedParameters);
	const auto Republished = Plan(InFixture, Batches, Items);
	HYP_CHECK(Republished->Statistics.PreparedInputBuilds == 0 && Republished->Statistics.ReusedChunks == 3);
	const auto Placement = Items.Items[4].State.Surface->GetCompiled()->Interface.Schema->Find("Placement").Index;
	auto Equivalent = std::make_shared<FResolvedMaterialParameters>(*Items.Items[4].ResolvedParameters);
	Equivalent->Values.Set(Placement,
	                       std::make_shared<const FMaterialValue>(*Items.Items[4].GetMaterialValue(Placement)));
	Items.Items[4].ResolvedParameters = std::move(Equivalent);
	const auto EqualValue = Plan(InFixture, Batches, Items);
	HYP_CHECK(EqualValue->Statistics.PreparedInputBuilds == 0 && EqualValue->Statistics.ReusedChunks == 3);
	auto Values = std::make_shared<FResolvedMaterialParameters>(*Items.Items[4].ResolvedParameters);
	Values->Values.Set(Placement, std::make_shared<const FMaterialValue>(FMaterialValue::Float(FVec4{.5f, 0, 0, 1})));
	Items.Items[4].ResolvedParameters = Values;
	const auto Changed = Plan(InFixture, Batches, Items);
	HYP_CHECK(Changed->Statistics.PreparedInputBuilds == 1 && Changed->Statistics.PreparedInputReuses == 10);
	HYP_CHECK(Changed->Statistics.RebuiltChunks == 1 && Changed->Statistics.ReusedChunks == 2);
	Items.Items.PushBack(Hidden);
	const auto Returned = Plan(InFixture, Batches, Items);
	HYP_CHECK(Returned->Statistics.PreparedInputBuilds == 0 && Returned->Statistics.PreparedInputReuses == 12);
	++Items.Items[0].Primitive.Generation;
	HYP_CHECK(Plan(InFixture, Batches, Items)->Statistics.PreparedInputBuilds == 1);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckRemovedOverlay(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	auto Items = Snapshot(InFixture, 8);
	PublishShared(Items, "Placement", FMaterialValue::Float(FVec4{.5f, 0, 0, 1}), true);
	const auto Original = Plan(InFixture, Batches, Items);
	std::weak_ptr<const FMaterialSharedParameters> Old = Items.Items[0].SharedParameters;
	for (auto& Item : Items.Items)
	{
		Item.SharedParameters.reset();
	}
	HYP_CHECK(Old.expired());
	const auto Removed = Plan(InFixture, Batches, Items);
	HYP_CHECK(Removed->Statistics.PlanReuses == 0 && Removed->Statistics.RebuiltChunks == 2);
	for (const auto& Batch : Removed->Batches)
	{
		const auto Expected = PackInstanceBatch(Items, Batch.Items);
		HYP_CHECK(*Expected->Constants[0].Bytes == *Batch.Instances->Constants[0].Bytes);
	}
	HYP_CHECK(*Original->Batches[0].Instances->Constants[0].Bytes !=
	          *Removed->Batches[0].Instances->Constants[0].Bytes);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckBudget(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {3, 1, 65536});
	auto Items = Snapshot(InFixture, 8);
	auto Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedInputs <= 3 && Result->Statistics.CachedChunks <= 1 &&
	          Result->Statistics.CachedBytes <= 65536);
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedChunks <= 1 && Result->Statistics.Evictions > 0);
	Items.Items.Erase(Items.Items.begin(), Items.Items.begin() + 4);
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Batches.size() == 1 && Result->Batches[0].Instances->InstanceCount == 4);
	Items.Items.Clear();
	Result = Plan(InFixture, Batches, Items);
	HYP_CHECK(Result->Statistics.CachedChunks == 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckPlanningMetadataBudget(FFixture& InFixture)
{
	struct FCase
	{
		std::size_t Count;
		std::size_t Bytes;
		bool bDevice;
		bool bOrdering;
	};

	// Stable numeric proofs must survive payload pressure, including ordinary-only plans with no payload at all.
	for (const auto Case : {FCase{2048, 16 * 1024 * 1024, true, true}, FCase{1024, 65536, false, true},
	                        FCase{1024, 65536, true, false}, FCase{1024, 0, false, true}})
	{
		auto Caps = InFixture.Device->GetCapabilities();
		Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = Case.bDevice;
		FRenderBatchLimits Limits;
		Limits.MaxBytes = Case.Bytes;
		FRenderBatchSystem Batches(InFixture.Tasks, Caps, Limits);
		auto Items = Snapshot(InFixture, Case.Count);
		if (!Case.bOrdering)
		{
			auto Description = Items.Items[0].State.Surface->GetSnapshot()->Definition->GetDescription();
			Description.Passes[0].bAllowBatchReordering = false;
			const auto Surface = InFixture.Material(std::move(Description));
			const auto Program = Surface->GetCompiled();
			for (auto& Item : Items.Items)
			{
				Item.State.Surface = Surface;
				Item.ResolvedParameters = std::make_shared<const FResolvedMaterialParameters>(
				    ResolveMaterialBindingContext(Surface->GetSnapshot(), *Program, Program->GetPass(), Item.Context));
			}
		}
		const auto First = Plan(InFixture, Batches, Items);
		HYP_CHECK(First->Statistics.CachedInputs == Case.Count && First->Statistics.CachedBytes <= Case.Bytes);
		for (unsigned Iteration = 0; Iteration < 4; ++Iteration)
		{
			const auto Reused = Plan(InFixture, Batches, Items);
			HYP_CHECK(Reused->Statistics.PlanReuses == 1 && Reused->Statistics.PreparedInputBuilds == 0);
			HYP_CHECK(Reused->Statistics.RebuiltChunks == 0 && Reused->Statistics.CachedBytes <= Case.Bytes);
			HYP_CHECK(Reused->Statistics.CachedInputs == Case.Count && Reused->Statistics.CachedInputBytes > 0);
			HYP_CHECK(Reused->Batches.size() == First->Batches.size());
		}
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
		                                              [&]
		                                              {
			                                              Batches.Clear();
		                                              }));
	}
}

void CheckWeakInstanceValues(FFixture& InFixture)
{
	auto Caps = InFixture.Device->GetCapabilities();
	Caps.Features[static_cast<std::size_t>(ERHIFeature::InstancedDrawing)].bEnabled = false;
	FRenderBatchSystem Batches(InFixture.Tasks, Caps, {16, 16, 0});
	auto Items = Snapshot(InFixture, 4);
	const auto Placement = Items.Items[0].State.Surface->GetCompiled()->Interface.Schema->Find("Placement").Index;
	const std::weak_ptr<const FMaterialValue> Old = Items.Items[0].GetMaterialValue(Placement);
	Plan(InFixture, Batches, Items);
	{
		auto Values = std::make_shared<FResolvedMaterialParameters>(*Items.Items[0].ResolvedParameters);
		Values->Values.Set(Placement, {});
		Items.Items[0].ResolvedParameters = std::move(Values);
	}
	HYP_CHECK(Old.expired()); // Neither a plan nor its preparation may retain the obsolete numeric tree.
	const auto Missing = Plan(InFixture, Batches, Items);
	HYP_CHECK(Missing->Statistics.PreparedInputBuilds == 1 && Missing->Statistics.PreparedInputReuses == 3);
	HYP_CHECK(Missing->Statistics.CachedInputs == 4 && Missing->Statistics.CachedBytes == 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckOrphanedPreparations(FFixture& InFixture)
{
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities());
	std::size_t Inputs{};
	{
		auto Items = Snapshot(InFixture, 128);
		for (auto& Item : Items.Items)
		{
			Item.State.Section = UINT32_MAX; // Input values can be prepared, but no candidate can be cached.
		}
		const auto Failed = Plan(InFixture, Batches, Items);
		Inputs = Failed->Statistics.CachedInputs;
		HYP_CHECK(Inputs == 128 &&
		          Failed->Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Preparation)] == 128);
	}
	const auto Empty = Snapshot(InFixture, 0);
	HYP_CHECK(Plan(InFixture, Batches, Empty)->Statistics.CachedInputs < Inputs);
	for (unsigned Frame = 0; Frame < 4; ++Frame)
	{
		Inputs = Plan(InFixture, Batches, Empty)->Statistics.CachedInputs;
	}
	HYP_CHECK(Inputs == 0);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}

void CheckPlanValueRetirement(FFixture& InFixture, bool bInInvalidate)
{
	// Isolate the sixteen-item metadata budget from the separate instance chunk/record/block byte budgets.
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {16, 16, 262144});
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
		HYP_CHECK(Released.expired()); // Prepared plans retain only weak value and local-page identities.
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
	CheckSharedRefresh(InFixture, false);
	CheckSharedRefresh(InFixture, true);
	CheckPreparedInputs(InFixture);
	CheckRemovedOverlay(InFixture);
	CheckBudget(InFixture);
	CheckPlanningMetadataBudget(InFixture);
	CheckWeakInstanceValues(InFixture);
	CheckOrphanedPreparations(InFixture);
	CheckPlanValueRetirement(InFixture, false);
	CheckPlanValueRetirement(InFixture, true);
	CheckCompatibility(InFixture);
}
} // namespace Hyperion::InstanceTests
