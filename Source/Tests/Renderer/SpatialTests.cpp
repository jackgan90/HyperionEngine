#include "Hyperion/Renderer/RenderScene.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>
#include <random>

namespace
{
using namespace Hyperion;

void Compare(ISceneSpatialIndex& InIndex, const FMat4& InView)
{
	const FFrustumVisibility Visibility(InView);
	FSceneVisibilityStats LinearStats;
	FSceneVisibilityStats BvhStats;
	auto Linear = InIndex.Query(&Visibility, false, LinearStats);
	auto Bvh = InIndex.Query(&Visibility, true, BvhStats);
	std::sort(Linear.begin(), Linear.end());
	std::sort(Bvh.begin(), Bvh.end());
	HYP_CHECK(Linear == Bvh);
	HYP_CHECK(std::adjacent_find(Bvh.begin(), Bvh.end()) == Bvh.end());
}

void CheckSpatial()
{
	auto Index = CreateBvhSpatialIndex();
	FSceneVisibilityStats Stats;
	Index->Commit(Stats);
	Compare(*Index, Identity());
	const FBounds Unit{{-.2f, -.2f, .2f}, {.2f, .2f, .8f}, true};
	Index->Set(0, {});
	for (unsigned Id = 1; Id <= 4096; ++Id)
	{
		Index->Set(Id, TransformBounds(Unit, Translation({float(Id % 64) * 2 - 64, float(Id / 64) * 2 - 64, 0})));
	}
	Index->Commit(Stats);
	HYP_CHECK(Stats.IndexRebuilds == 1);
	const FFrustumVisibility Visibility(Identity());
	FSceneVisibilityStats Linear;
	FSceneVisibilityStats Bvh;
	Index->Query(&Visibility, false, Linear);
	Index->Query(&Visibility, true, Bvh);
	HYP_CHECK(Bvh.CandidateGroups == Linear.CandidateGroups && Bvh.VisitedNodes < Linear.GroupTests / 4);
	std::cout << "4097 groups: linear tests=" << Linear.GroupTests << ", BVH visits=" << Bvh.VisitedNodes
	          << ", candidates=" << Bvh.CandidateGroups << '\n';
	Stats = {};
	Index->Commit(Stats);
	HYP_CHECK(Stats.IndexRebuilds == 0 && Stats.IndexRefits == 0);
	std::mt19937 Random(217);
	std::uniform_real_distribution<float> Offset(-100, 100);
	for (unsigned Step = 0; Step < 50; ++Step)
	{
		for (unsigned Update = 0; Update < 20; ++Update)
		{
			const auto Id = Random() % 4096 + 1;
			Index->Set(Id, TransformBounds(Unit, Translation({Offset(Random), Offset(Random), Offset(Random)})));
		}
		Stats = {};
		Index->Commit(Stats);
		Compare(*Index, Multiply(Perspective(1, 1.3f, .1f, 300), LookAt({Offset(Random), 3, 100}, {0, 0, 0})));
	}
	for (unsigned Id = 1; Id < 4097; Id += 3)
	{
		Index->Remove(Id);
	}
	Index->Set(0, Unit); // Unknown becomes bounded independently of view.
	Index->Set(2, {});
	Stats = {};
	Index->Commit(Stats);
	Compare(*Index, Identity());
	HYP_CHECK(Stats.IndexRebuilds == 1 && Stats.UnboundedGroups == 1);
}

class FCountingPrimitive final : public IRenderPrimitive
{
public:
	FCountingPrimitive(FTaskSystem& InTasks, bool bInBounded) : IRenderPrimitive(InTasks), bBounded(bInBounded)
	{
	}

	FBounds GetWorldBounds() const override
	{
		return bBounded ? TransformBounds({{-.1f, -.1f, .2f}, {.1f, .1f, .8f}, true}, GetState().World) : FBounds{};
	}

	void Collect(const FRenderView&, std::vector<FRenderItem>& OutItems) const override
	{
		for (unsigned Index = 0; Index < 3; ++Index)
		{
			OutItems.push_back({GetState(), {}});
		}
	}

private:
	bool bBounded{};
};

void CheckCollection()
{
	FTaskSystem Tasks(1, 1);
	FRenderSceneClient Client(Tasks);
	const auto Factory = [](FTaskSystem& InTasks)
	{
		return std::make_unique<FCountingPrimitive>(InTasks, true);
	};
	FRenderPrimitiveState State;
	State.World = Translation({30, 0, 0});
	auto Group = Client.CreateBatch({State, State}, Factory);
	auto Visible = Client.Create({}, Factory);
	auto Unknown = Client.Create(State,
	                             [](FTaskSystem& InTasks)
	                             {
		                             return std::make_unique<FCountingPrimitive>(InTasks, false);
	                             });
	FRenderSceneSnapshot Snapshot;
	const auto Collect = [&]
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          Snapshot = Client.Collect({});
		                          }));
	};
	Collect();
	HYP_CHECK(Snapshot.Statistics.Groups == 3 && Snapshot.Statistics.CollectedPrimitives == 2 &&
	          Snapshot.Items.size() == 6);
	HYP_CHECK(Snapshot.Items[0].Primitive == Visible.GetHandle() && Snapshot.Items[3].Primitive == Unknown.GetHandle());
	State.World = Identity();
	State.Revision = 2;
	Tasks.Wait(Client.Update({{Group[0].GetHandle(), State}, {Group[1].GetHandle(), State}}));
	Collect();
	HYP_CHECK(Snapshot.Statistics.CollectedPrimitives == 4 && Snapshot.Items.size() == 12);
	const auto Old = Snapshot;
	Tasks.Wait(Client.RemoveBatch(Group));
	Collect();
	HYP_CHECK(Snapshot.Items.size() == 6 && Old.Items.size() == 12);
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          FRenderView Other;
		                          Other.ViewProjection = Translation({-30, 0, 0});
		                          HYP_CHECK(Client.Collect(Other).Items.size() == 3);
		                          HYP_CHECK(Client.Collect({}).Items.size() == 6);
	                          }));
	Client.Close();
}
} // namespace

int main()
{
	try
	{
		CheckSpatial();
		CheckCollection();
		std::cout << "BVH differential, incremental updates, conservative early collection and view isolation passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
