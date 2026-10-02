#include "Support/SceneChangeTestSupport.h"

namespace
{
using namespace Hyperion;
using namespace Hyperion::Tests;

template<class T>
void RejectSceneFactCallback(const T& InOperation, std::size_t InExpectedEqualCalls,
                             std::size_t InExpectedGetFailures = 0)
{
	bool bRejected{};
	try
	{
		InOperation();
	}
	catch (const std::runtime_error& Error)
	{
		bRejected = std::string_view(Error.what()).starts_with("Scene fact ");
	}
	const auto EqualCalls = SceneFactFault.EqualCalls;
	const auto GetFailures = SceneFactFault.GetFailures;
	SceneFactFault = {};
	CheckSceneFact(bRejected && EqualCalls == InExpectedEqualCalls && GetFailures == InExpectedGetFailures);
}

void CheckPreparedBatchFailure()
{
	FScene Scene;
	const auto Model = AddSceneFactQueryModel(Scene);
	auto ModelNode = *Scene.FindNode(Model);
	AddSceneFactValue(ModelNode, "value", 1);
	Scene.EditNode(Model, ModelNode, Scene.GetRevision());
	auto CameraNode = MakeSceneCameraNode("camera");
	AddSceneFactValue(CameraNode, "value", 2);
	const auto Camera = Scene.AddNode(CameraNode);
	Scene.SetSettings({Camera, {}});
	Scene.Acknowledge(Scene.GetRevision());
	Scene.SetName(Model, "pending-old-name");
	const auto Before = SnapshotSceneFacts(Scene);
	CheckSceneFact(Before.Ray.Status == ESceneRayStatus::Hit && Before.Ray.Handle == Model);
	ModelNode = *Scene.FindNode(Model);
	CameraNode = *Scene.FindNode(Camera);
	ModelNode.Name = "new-model-name";
	ModelNode.Local() = Translation({3, 0, 0});
	SetSceneFactValue(ModelNode, "value", 3);
	CameraNode.Name = "new-camera-name";
	CameraNode.Camera().reset();
	SceneFactFault = {.EqualValue = 2};
	RejectSceneFactCallback(
	    [&]
	    {
		    Scene.EditNodes({{Model, ModelNode}, {Camera, CameraNode}}, Scene.GetRevision());
	    },
	    2);
	CheckSceneFactSnapshot(Scene, Before);
	// Retry proves the same mutation can publish after the callback failure is removed.
	CheckSceneFact(Scene.EditNodes({{Model, ModelNode}, {Camera, CameraNode}}, Scene.GetRevision()));
	CheckSceneFact(Scene.GetRevision() == Before.Revision + 1 && !Scene.GetSettings().DefaultCamera);
	CheckSceneFact(Scene.Raycast(SceneFactRay()).Stats.Bounds.IndexRefits == 1);
}

void CheckPreparedGetFailure()
{
	FScene Scene;
	const auto Handle = AddSceneFactQueryModel(Scene);
	auto Node = *Scene.FindNode(Handle);
	AddSceneFactValue(Node, "value", 1);
	Scene.EditNode(Handle, Node, Scene.GetRevision());
	const auto Before = SnapshotSceneFacts(Scene);
	Node = *Scene.FindNode(Handle);
	Node.Name = "Get-failure-candidate";
	Node.Local() = Translation({2, 0, 0});
	// Candidate validation receives copied std::any storage. Only shared preparation reads this authority slot.
	SceneFactFault = {.GetState = &Scene.FindNode(Handle)->Components.Find("value")->State};
	RejectSceneFactCallback(
	    [&]
	    {
		    Scene.EditNode(Handle, Node, Scene.GetRevision());
	    },
	    0, 1);
	CheckSceneFactSnapshot(Scene, Before);
}

void CheckPreparedHierarchyFailure(bool bInKeepChildren)
{
	FScene Scene;
	AddSceneFactQueryModel(Scene);
	auto Parent = MakeSceneCameraNode("parent");
	Parent.bEnabled = false;
	FSceneNode Child;
	Child.Id = "child";
	Child.Parent() = "parent";
	AddSceneFactValue(Child, "value", 2);
	FSceneNode Grandchild;
	Grandchild.Id = "grandchild";
	Grandchild.Parent() = "child";
	const auto Handles = Scene.AddNodes({Parent, Child, Grandchild});
	Scene.SetSettings({Handles[0], {}});
	Scene.Acknowledge(Scene.GetRevision());
	Scene.SetName(Handles[2], "pending-grandchild");
	const auto Before = SnapshotSceneFacts(Scene);
	SceneFactFault = {.EqualValue = 2};
	RejectSceneFactCallback(
	    [&]
	    {
		    if (bInKeepChildren)
		    {
			    Scene.RemoveNodeKeepChildren(Handles[0]);
		    }
		    else
		    {
			    Scene.Reparent(Handles[1], std::nullopt, ESceneReparentMode::KeepWorld);
		    }
	    },
	    1);
	CheckSceneFactSnapshot(Scene, Before);
}
} // namespace

void CheckSceneComponentFailures()
{
	RegisterSceneFactTypes();
	CheckPreparedBatchFailure();
	CheckPreparedGetFailure();
	CheckPreparedHierarchyFailure(false);
	CheckPreparedHierarchyFailure(true);
}
