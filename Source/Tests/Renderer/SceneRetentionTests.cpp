#include "Hyperion/Renderer/RenderSession.h"
#include "Support/TestSupport.h"

using namespace Hyperion;

namespace
{
class FManyItems final : public IRenderPrimitive
{
public:
	explicit FManyItems(FTaskSystem& InTasks) : IRenderPrimitive(InTasks)
	{
	}

	bool IsStaticCollection() const override
	{
		return true;
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		for (std::uint64_t Index = 0; Index < 65; ++Index)
		{
			FRenderItem Item;
			Item.State = GetState();
			Item.LocalItemId = Index;
			OutItems.push_back(std::move(Item));
		}
	}
};

FRenderSceneSnapshot Collect(FTaskSystem& InTasks, const FRenderSceneClient& InScene, FRenderView InView,
                             FRenderSceneSnapshot* InPrevious = nullptr, std::uint64_t InResourceRevision = 1)
{
	FRenderSceneSnapshot Result;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              Result = InScene.Collect(InView, true, InResourceRevision, InPrevious);
	                              }));
	return Result;
}

FRenderSceneSnapshot CollectPrepared(FTaskSystem& InTasks, const FRenderSceneClient& InScene, FRenderView InView,
                                     FRenderSceneSnapshot* InPrevious = nullptr)
{
	FRenderSceneSnapshot Result;
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [&]
	                              {
		                              InScene.BeginViews();
		                              Result = InScene.CollectPrepared(InView, 1, InPrevious);
	                              }));
	return Result;
}

void CheckPreparedMembership(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.LocalBounds = {{-.08f, -.08f, .4f}, {.08f, .08f, .6f}, true};
	std::vector<FRenderPrimitiveState> States(5, State);
	for (std::size_t Index = 0; Index < States.size(); ++Index)
	{
		States[Index].World = Translation({(float(Index) - 2) * .4f, 0, 0});
	}
	auto Bindings = Scene.CreateBatch(std::move(States));
	InTasks.Wait(Scene.Flush());
	FRenderView View;
	auto Previous = CollectPrepared(InTasks, Scene, View);
	HYP_CHECK(Previous.Items.Size() == 5);
	const auto* Middle = &Previous.Items[2];
	View.ViewProjection = Translation({.01f, 0, 0});
	auto Current = CollectPrepared(InTasks, Scene, View, &Previous);
	HYP_CHECK(Current.Statistics.MembershipReuses == 1 && &Current.Items[2] == Middle);
	HYP_CHECK(Current.Statistics.ContainedItemTests == 5);
	auto Independent = Current;
	for (unsigned Iteration = 0; Iteration < 60; ++Iteration)
	{
		View.ViewProjection = Translation({(float(Iteration % 13) - 6) * .21f, 0, 0});
		auto Expected = PrepareSceneSnapshot(Collect(InTasks, Scene, View));
		auto Next = CollectPrepared(InTasks, Scene, View, &Current);
		HYP_CHECK(Next.Items.Size() == Expected.Items.Size());
		for (std::size_t Index = 0; Index < Next.Items.Size(); ++Index)
		{
			HYP_CHECK(Next.Items[Index].Primitive == Expected.Items[Index].Primitive);
			HYP_CHECK(Next.Items[Index].Ordinal == Expected.Items[Index].Ordinal);
		}
		HYP_CHECK(Next.Statistics.MembershipAdded <= 5 && Next.Statistics.MembershipRemoved <= 5);
		HYP_CHECK(Next.Items.Size() + Next.RetainedItems.Size() == 5);
		HYP_CHECK(Independent.Items.Size() == 5 && Independent.View.ViewProjection.Values[12] == .01f);
		Current = std::move(Next);
	}
	// Provenance mismatch must leave the foreign prepared snapshot and its owners untouched.
	FRenderSceneClient Other(InTasks);
	auto Foreign = CollectPrepared(InTasks, Other, View, &Current);
	HYP_CHECK(Foreign.Items.IsEmpty() && Foreign.RetainedItems.IsEmpty());
	HYP_CHECK(Foreign.DrawFrame != Current.DrawFrame);
	Other.Close();
	Scene.Close();
}

void CheckLookup(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource, bool bInSparse)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.LocalBounds = {{-.1f, -.1f, .4f}, {.1f, .1f, .6f}, true};
	std::vector<FRenderPrimitiveState> States(bInSparse ? 512 : 3, State);
	for (std::size_t Index = 0; Index < States.size(); ++Index)
	{
		States[Index].World = Translation({float(Index) * 10, 0, 0});
	}
	auto Bindings = Scene.CreateBatch(std::move(States));
	InTasks.Wait(Scene.Flush());
	if (bInSparse)
	{
		for (std::size_t Index = 2; Index + 1 < Bindings.size(); ++Index)
		{
			Bindings[Index].Remove();
		}
		InTasks.Wait(Scene.Flush());
	}
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, Scene, View);
	HYP_CHECK(Previous.Items.Size() == 3);
	const auto* Middle = &Previous.Items[1];
	View.CullingMode = ESceneCullingMode::Bvh;
	View.ViewProjection = Translation({-10, 0, 0});
	auto Current = Collect(InTasks, Scene, View, &Previous);
	HYP_CHECK(Current.Items.Size() == 1 && &Current.Items[0] == Middle);
	HYP_CHECK(Current.Statistics.ItemStorageReuses == 1 && Current.RetainedItems.Size() == 2);
	View.CullingMode = ESceneCullingMode::None;
	auto Restored = Collect(InTasks, Scene, View, &Current);
	HYP_CHECK(Restored.Items.Size() == 3 && Restored.Statistics.RetainedItemRestores == 2);
	HYP_CHECK(Restored.Statistics.ItemStorageReuses == 3 && &Restored.Items[1] == Middle);
	auto Copy = Restored;
	Copy.Items[0].State.World = Translation({100, 0, 0});
	HYP_CHECK(Copy.Items[0].State.World.Values != Restored.Items[0].State.World.Values);
	++Restored.Items[0].Primitive.Generation;
	const auto Revalidated = Collect(InTasks, Scene, View, &Restored);
	HYP_CHECK(Revalidated.Statistics.ItemStorageReuses == 2);
	HYP_CHECK(Revalidated.Items[0].Primitive == Bindings[0].GetHandle());
	Scene.Close();
}

void CheckRetentionLimit()
{
	FRenderItemList Previous;
	for (std::uint32_t Slot = 0; Slot < FRenderSceneSnapshot::RetainedItemLimit + 2; ++Slot)
	{
		FRenderItem Item;
		Item.Primitive = {1, Slot, 1};
		Item.Lifetime = std::make_shared<const int>(0);
		Previous.PushBack(std::move(Item));
	}
	const std::weak_ptr<const void> Oldest = Previous[FRenderSceneSnapshot::RetainedItemLimit].Lifetime;
	FRenderItemList Current;
	Current.MoveRemainingFrom(Previous, FRenderSceneSnapshot::RetainedItemLimit);
	HYP_CHECK(Current.Size() == FRenderSceneSnapshot::RetainedItemLimit && Oldest.expired());
	HYP_CHECK(Previous.IsEmpty());
	const std::weak_ptr<const void> Kept = Current[0].Lifetime;
	Current.Clear();
	HYP_CHECK(Kept.expired());
}

void CheckLargeEmission(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	auto Many = Scene.Create(State,
	                         [](FTaskSystem& InTasks)
	                         {
		                         return std::make_unique<FManyItems>(InTasks);
	                         });
	auto Single = Scene.Create(State);
	InTasks.Wait(Scene.Flush());
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, Scene, View);
	for (unsigned Iteration = 0; Iteration < 3; ++Iteration)
	{
		const auto* Last = &Previous.Items[65];
		auto Current = Collect(InTasks, Scene, View, &Previous);
		HYP_CHECK(Current.Items.Size() == 66 && &Current.Items[65] == Last);
		HYP_CHECK(Current.Items[64].LocalItemId == 64 && Current.Statistics.ItemStorageReuses == 1);
		HYP_CHECK(Current.RetainedItems.IsEmpty());
		Previous = std::move(Current);
	}
	Scene.Close();
}

void CheckRemovedPublication(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	auto Binding = Scene.Create(State);
	InTasks.Wait(Scene.Flush());
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, Scene, View);
	HYP_CHECK(Previous.Items.Size() == 1);
	const std::weak_ptr<const void> RemovedLifetime = Previous.Items[0].Lifetime;
	auto Independent = Previous;
	InTasks.Wait(Binding.Remove());
	for (unsigned Iteration = 0; Iteration < 3; ++Iteration)
	{
		auto Current = Collect(InTasks, Scene, View, &Previous);
		HYP_CHECK(Current.Items.IsEmpty() && Current.RetainedItems.IsEmpty());
		Previous = std::move(Current);
		HYP_CHECK(Independent.Items.Size() == 1 && !RemovedLifetime.expired());
	}
	Independent = {};
	HYP_CHECK(RemovedLifetime.expired());
	Scene.Close();
}

void CheckUpdatedPublication(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.LocalBounds = {{-.1f, -.1f, .4f}, {.1f, .1f, .6f}, true};
	auto Binding = Scene.Create(State);
	InTasks.Wait(Scene.Flush());
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, Scene, View);
	HYP_CHECK(Previous.Items.Size() == 1);
	const std::weak_ptr<const void> OldLifetime = Previous.Items[0].Lifetime;
	++State.Revision;
	State.World = Translation({10, 0, 0});
	InTasks.Wait(Scene.Update({{Binding.GetHandle(), State}}));
	View.CullingMode = ESceneCullingMode::Bvh;
	auto Current = Collect(InTasks, Scene, View, &Previous);
	HYP_CHECK(Current.Items.IsEmpty() && Current.RetainedItems.IsEmpty());
	HYP_CHECK(Previous.Items.Size() == 1 && !OldLifetime.expired());
	Previous = std::move(Current);
	HYP_CHECK(OldLifetime.expired());
	Scene.Close();
}

void CheckResourcePublication(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient Scene(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.LocalBounds = {{-.1f, -.1f, .4f}, {.1f, .1f, .6f}, true};
	auto Binding = Scene.Create(State);
	InTasks.Wait(Scene.Flush());
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, Scene, View);
	View.CullingMode = ESceneCullingMode::Bvh;
	View.ViewProjection = Translation({-10, 0, 0});
	auto Culled = Collect(InTasks, Scene, View, &Previous);
	HYP_CHECK(Culled.Items.IsEmpty() && Culled.RetainedItems.Size() == 1);
	const auto Current = Collect(InTasks, Scene, View, &Culled, 2);
	HYP_CHECK(Current.Items.IsEmpty() && Current.RetainedItems.IsEmpty());
	HYP_CHECK(Culled.RetainedItems.Size() == 1);
	Scene.Close();
}

void CheckForeignScene(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	FRenderSceneClient First(InTasks);
	FRenderSceneClient Second(InTasks);
	FRenderPrimitiveState State;
	State.Resource = InResource;
	State.LocalBounds = {{-.1f, -.1f, .4f}, {.1f, .1f, .6f}, true};
	auto FirstBinding = First.Create(State);
	State.World = Translation({10, 0, 0});
	auto SecondBinding = Second.Create(State);
	InTasks.Wait(First.Flush());
	InTasks.Wait(Second.Flush());
	FRenderView View;
	View.CullingMode = ESceneCullingMode::None;
	auto Previous = Collect(InTasks, First, View);
	View.CullingMode = ESceneCullingMode::Bvh;
	const auto Current = Collect(InTasks, Second, View, &Previous);
	HYP_CHECK(Current.Items.IsEmpty() && Current.RetainedItems.IsEmpty());
	HYP_CHECK(Current.DrawFrame != Previous.DrawFrame && Previous.Items.Size() == 1);
	First.Close();
	Second.Close();
}
} // namespace

void RunSceneRetentionTests(FTaskSystem& InTasks, const std::shared_ptr<const FRenderResource>& InResource)
{
	CheckPreparedMembership(InTasks, InResource);
	CheckLookup(InTasks, InResource, false);
	CheckLookup(InTasks, InResource, true);
	CheckLargeEmission(InTasks, InResource);
	CheckRetentionLimit();
	CheckRemovedPublication(InTasks, InResource);
	CheckUpdatedPublication(InTasks, InResource);
	CheckResourcePublication(InTasks, InResource);
	CheckForeignScene(InTasks, InResource);
}
