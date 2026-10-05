#include "Support/SceneChangeTestSupport.h"

namespace
{
using namespace Hyperion;
using namespace Hyperion::Tests;

std::vector<FSceneNode> MakeTree(std::size_t InChildren)
{
	std::vector<FSceneNode> Nodes(InChildren + 1);
	for (std::size_t Index = 0; Index < Nodes.size(); ++Index)
	{
		auto& Node = Nodes[Index];
		Node.Id = std::to_string(Index);
		if (Index)
		{
			Node.Parent() = "0";
		}
		for (unsigned Component = 0; Component < 4; ++Component)
		{
			AddSceneFactValue(Node, "value" + std::to_string(Component));
		}
	}
	return Nodes;
}

void CheckInheritedChanges(std::size_t InChildren, bool bInEnabled)
{
	FScene Scene;
	const auto Handles = Scene.LoadNodes(MakeTree(InChildren));
	Scene.Acknowledge(Scene.GetRevision());
	SceneFactFault.EqualCalls = 0;
	if (bInEnabled)
	{
		CheckSceneFact(Scene.SetEnabled(Handles[0], false));
	}
	else
	{
		CheckSceneFact(Scene.SetLocalTransform(Handles[0], Translation({3, 0, 0})));
	}
	CheckSceneFact(SceneFactFault.EqualCalls <= 8);
	const auto Changes = Scene.GetChanges();
	CheckSceneFact(Changes.size() == Handles.size());
	for (const auto& Change : Changes)
	{
		CheckSceneFact(HasChange(Change.Mask, bInEnabled ? ESceneChangeMask::Enabled : ESceneChangeMask::Transform));
		CheckSceneFact(bInEnabled ? !Change.bEffectiveEnabled : Change.World.Values[12] == 3);
		if (Change.Handle != Handles[0])
		{
			CheckSceneFact(Change.ComponentChanges.empty());
		}
	}
}

void CheckAuthoredAndAccumulatedChanges()
{
	FScene Scene;
	const auto Handles = Scene.LoadNodes(MakeTree(1000));
	Scene.Acknowledge(Scene.GetRevision());
	auto Root = *Scene.FindNode(Handles[0]);
	auto Child = *Scene.FindNode(Handles[1]);
	Root.Local() = Translation({2, 0, 0});
	SetSceneFactValue(Child, "value0", 1);
	SceneFactFault.EqualCalls = 0;
	CheckSceneFact(Scene.EditNodes({{Handles[0], Root}, {Handles[1], Child}}, Scene.GetRevision()));
	CheckSceneFact(SceneFactFault.EqualCalls < 20);
	CheckSceneFacts(Scene, Handles[1], {{"test.scene-fact", "value0", ESceneComponentChangeFlags::Modified}});
	SceneFactFault.EqualCalls = 0;
	CheckSceneFact(Scene.SetEnabled(Handles[0], false));
	CheckSceneFact(SceneFactFault.EqualCalls <= 8);
	CheckSceneFacts(Scene, Handles[1], {{"test.scene-fact", "value0", ESceneComponentChangeFlags::Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	Scene.BeginSynchronization();
	Scene.EndSynchronization();
	const auto Changes = Scene.GetChanges();
	CheckSceneFact(Changes.size() == Handles.size() + 1);
	for (const auto& Change : Changes)
	{
		if (Change.Settings)
		{
			continue;
		}
		CheckSceneFact(Change.ComponentChanges.size() == 5);
		for (const auto& Component : Change.ComponentChanges)
		{
			CheckSceneFact(Component.Flags == ESceneComponentChangeFlags::Added);
		}
	}
}
} // namespace

void CheckSceneComponentCosts()
{
	RegisterSceneFactTypes();
	for (const std::size_t Children : {1000u, 10000u})
	{
		CheckInheritedChanges(Children, false);
		CheckInheritedChanges(Children, true);
	}
	CheckAuthoredAndAccumulatedChanges();
}
