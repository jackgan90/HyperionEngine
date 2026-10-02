#include "Support/DispatchAllocationFailure.h"
#include "Support/SceneChangeTestSupport.h"
#include <iostream>

namespace
{
using namespace Hyperion;
using namespace Hyperion::Tests;

void CheckSceneFactAllocation(unsigned InCase)
{
	FScene Scene;
	AddSceneFactQueryModel(Scene);
	auto Parent = MakeSceneCameraNode("allocation-parent");
	FSceneNode Child;
	Child.Id = "allocation-child";
	Child.Parent() = Parent.Id;
	AddSceneFactValue(Child, "value", 2);
	const auto Handles = Scene.AddNodes({Parent, Child});
	Scene.SetSettings({Handles[0], {}});
	Scene.Acknowledge(Scene.GetRevision());
	Scene.SetName(Handles[1], "pending-child");
	const auto Before = SnapshotSceneFacts(Scene);
	FSceneNode New;
	New.Id = "new-node";
	AddSceneFactValue(New, "added-value", 4);
	bool bRejected{};
	{
		// map::emplace below the new helper is recoverable; iterator proxy construction is excluded.
		FDispatchAllocationFailure Failure(0, "Hyperion::BuildSceneComponentDifference", "FSceneComponentPair");
		try
		{
			if (InCase == 0)
			{
				Scene.AddNode(New);
			}
			else if (InCase == 1)
			{
				Scene.RemoveSubtree(Handles[0]);
			}
			else if (InCase == 2)
			{
				Scene.BeginSynchronization();
			}
			else if (InCase == 3)
			{
				Scene.Clear();
			}
			else
			{
				Scene.RemoveNodeKeepChildren(Handles[0]);
			}
		}
		catch (const std::bad_alloc&)
		{
			bRejected = true;
		}
		CheckSceneFact(bRejected && Failure.WasInjected());
	}
	CheckSceneFactSnapshot(Scene, Before);
	if (InCase == 2)
	{
		Scene.BeginSynchronization();
		CheckSceneFact(Scene.GetRevision() == Before.Revision);
		Scene.EndSynchronization();
	}
	std::cout << "Scene component allocation failure case " << InCase
	          << ": authority, pending facts and warm query unchanged\n";
}
} // namespace

void CheckSceneComponentAllocations()
{
	RegisterSceneFactTypes();
	for (unsigned Case = 0; Case < 5; ++Case)
	{
		CheckSceneFactAllocation(Case);
	}
}
