#include "Hyperion/Scene/SceneManifest.h"
#include "Support/SceneChangeTestSupport.h"

namespace
{
using namespace Hyperion;
using namespace Hyperion::Tests;
constexpr auto Added = ESceneComponentChangeFlags::Added;
constexpr auto Modified = ESceneComponentChangeFlags::Modified;
constexpr auto Removed = ESceneComponentChangeFlags::Removed;

void CheckIdentityFacts()
{
	FScene Scene;
	FSceneNode Node;
	Node.Id = "identity";
	AddSceneFactValue(Node, "second", 2);
	AddSceneFactValue(Node, "first", 1);
	const auto Handle = Scene.AddNode(Node);
	CheckSceneFacts(Scene, Handle,
	                {{"hyperion.scenetransform", "hyperion.scenetransform", Added},
	                 {"test.scene-fact", "first", Added},
	                 {"test.scene-fact", "second", Added}});
	Scene.Acknowledge(Scene.GetRevision());
	SetSceneFactValue(Node, "second", 3);
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle, {{"test.scene-fact", "second", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	const auto Revision = Scene.GetRevision();
	Node.Components.Remove("second");
	AddSceneFactValue(Node, "second", 3);
	Node.PointLight(); // An empty optional slot is not a live component.
	CheckSceneFact(Scene.EditNode(Handle, Node, Revision));
	CheckSceneFact(Scene.GetRevision() == Revision && Scene.GetChanges().empty());
	Node.Components.Rename("first", "test.scene-fact-other");
	CheckSceneFact(Scene.EditNode(Handle, Node, Revision));
	CheckSceneFacts(Scene, Handle,
	                {{"test.scene-fact", "first", Removed}, {"test.scene-fact", "test.scene-fact-other", Added}});
	Scene.Acknowledge(Scene.GetRevision());
	Node.Components.Remove("test.scene-fact-other");
	Node.Components.Add("test.scene-fact-other", "test.scene-fact-other");
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle,
	                {{"test.scene-fact", "test.scene-fact-other", Removed},
	                 {"test.scene-fact-other", "test.scene-fact-other", Added}});
	Scene.Acknowledge(Scene.GetRevision());
	Node.Components.Remove("second");
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle, {{"test.scene-fact", "second", Removed}});
}

void CheckOccurrenceFacts()
{
	for (unsigned Case = 0; Case < 4; ++Case)
	{
		FScene Scene;
		FSceneNode Node;
		Node.Id = "occurrences";
		if (Case == 1 || Case == 2)
		{
			AddSceneFactValue(Node, "value", 1);
		}
		const auto Handle = Scene.AddNode(Node);
		Scene.Acknowledge(Scene.GetRevision());
		const auto InitialRevision = Scene.GetRevision();
		if (Case == 1)
		{
			Node.Components.Remove("value");
		}
		else if (Case == 2)
		{
			SetSceneFactValue(Node, "value", 2);
		}
		else
		{
			AddSceneFactValue(Node, "value", 1);
		}
		CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
		const auto FirstRevision = Scene.GetRevision();
		const auto OwnedFirst = Scene.GetChanges();
		if (Case == 3)
		{
			SetSceneFactValue(Node, "value", 2);
			CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
		}
		if (Case == 1)
		{
			AddSceneFactValue(Node, "value", 1);
		}
		else if (Case == 2)
		{
			SetSceneFactValue(Node, "value", 1);
		}
		else
		{
			Node.Components.Remove("value");
		}
		CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
		const auto Expected = Case == 2 ? Modified : Case == 3 ? Added | Modified | Removed : Added | Removed;
		CheckSceneFacts(Scene, Handle, {{"test.scene-fact", "value", Expected}});
		CheckSceneFact(SceneFactChange(Scene, Handle).Node == Node);
		CheckSceneFact(Scene.GetRevision() == InitialRevision + (Case == 3 ? 3 : 2));
		Scene.Acknowledge(FirstRevision);
		CheckSceneFacts(Scene, Handle, {{"test.scene-fact", "value", Expected}});
		Scene.Acknowledge(Scene.GetRevision());
		CheckSceneFact(Scene.GetChanges().empty());
		CheckSceneFact(OwnedFirst[0].Revision == FirstRevision &&
		               OwnedFirst[0].ComponentChanges == std::vector<FSceneComponentChange>{{"test.scene-fact", "value",
		                                                                                     Case == 1   ? Removed
		                                                                                     : Case == 2 ? Modified
		                                                                                                 : Added}});
	}
}

void CheckRemovedAndOwnedFacts()
{
	FScene Scene;
	FSceneNode Node;
	Node.Id = "reused";
	Node.Name = Node.Id;
	AddSceneFactValue(Node, "retained-component-identity-long-enough-to-own", 7);
	const auto Old = Scene.AddNode(Node);
	const auto Owned = Scene.GetChanges();
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.RemoveSubtree(Old));
	CheckSceneFacts(Scene, Old,
	                {{"hyperion.scenetransform", "hyperion.scenetransform", Removed},
	                 {"test.scene-fact", "retained-component-identity-long-enough-to-own", Removed}});
	const auto New = Scene.AddNode(Node);
	CheckSceneFact(New.Slot == Old.Slot && New.Generation != Old.Generation && Scene.GetChanges().size() == 2);
	CheckSceneFacts(Scene, New,
	                {{"hyperion.scenetransform", "hyperion.scenetransform", Added},
	                 {"test.scene-fact", "retained-component-identity-long-enough-to-own", Added}});
	CheckSceneFact(SceneFactChange(Scene, Old).bRemoved && !SceneFactChange(Scene, Old).Node);
	Scene.Clear();
	CheckSceneFacts(Scene, New,
	                {{"hyperion.scenetransform", "hyperion.scenetransform", Added | Removed},
	                 {"test.scene-fact", "retained-component-identity-long-enough-to-own", Added | Removed}});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Owned[0].Handle == Old && Owned[0].Node == Node &&
	               Owned[0].ComponentChanges ==
	                   std::vector<FSceneComponentChange>{
	                       {"hyperion.scenetransform", "hyperion.scenetransform", Added},
	                       {"test.scene-fact", "retained-component-identity-long-enough-to-own", Added}});
}

void CheckSynchronizationFacts()
{
	for (unsigned Case = 0; Case < 4; ++Case)
	{
		FScene Scene;
		FSceneNode Node;
		Node.Id = "sync";
		AddSceneFactValue(Node, "value", 1);
		std::optional<FSceneHandle> Handle;
		if (Case < 3)
		{
			Handle = Scene.AddNode(Node);
			Scene.Acknowledge(Scene.GetRevision());
			if (Case == 1)
			{
				SetSceneFactValue(Node, "value", 2);
				Scene.EditNode(*Handle, Node, Scene.GetRevision());
			}
			if (Case == 2)
			{
				Scene.RemoveSubtree(*Handle);
			}
		}
		const auto Revision = Scene.GetRevision();
		Scene.BeginSynchronization();
		CheckSceneFact(Scene.GetRevision() == Revision);
		const auto Changes = Scene.GetChanges();
		CheckSceneFact(Changes.size() == (Handle ? 2 : 1));
		CheckSceneFact(Changes.back().Settings == Scene.GetSettings() && Changes.back().ComponentChanges.empty());
		if (Handle)
		{
			CheckSceneFacts(Scene, *Handle,
			                {{"hyperion.scenetransform", "hyperion.scenetransform", Case == 2 ? Removed : Added},
			                 {"test.scene-fact", "value",
			                  Case == 2   ? Removed
			                  : Case == 1 ? Added | Modified
			                              : Added}});
		}
		bool bRejected{};
		try
		{
			Scene.BeginSynchronization();
		}
		catch (const std::logic_error&)
		{
			bRejected = true;
		}
		CheckSceneFact(bRejected);
		Scene.EndSynchronization();
		Scene.Acknowledge(Revision);
		Scene.BeginSynchronization();
		CheckSceneFact(Scene.GetRevision() == Revision && Scene.GetChanges().size() == (Case < 2 ? 2 : 1));
		Scene.EndSynchronization();
	}
}

void CheckHierarchyFacts()
{
	FScene Scene;
	FSceneNode Parent;
	Parent.Id = "parent";
	Parent.Local() = Translation({3, 0, 0});
	FSceneNode Child;
	Child.Id = "child";
	FSceneNode Grandchild;
	Grandchild.Id = "grandchild";
	Grandchild.Parent() = "child";
	const auto Handles = Scene.LoadNodes({Parent, Child, Grandchild});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.Reparent(Handles[1], Handles[0], ESceneReparentMode::KeepLocal));
	CheckSceneFacts(Scene, Handles[1], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFacts(Scene, Handles[2], {});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.Reparent(Handles[1], std::nullopt, ESceneReparentMode::KeepWorld));
	CheckSceneFacts(Scene, Handles[1], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFact(Scene.GetChanges().size() == 1);
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.Reparent(Handles[1], Handles[0], ESceneReparentMode::KeepWorld));
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.SetLocalTransform(Handles[0], Translation({4, 0, 0})));
	CheckSceneFacts(Scene, Handles[0], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFacts(Scene, Handles[1], {});
	CheckSceneFacts(Scene, Handles[2], {});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.SetEnabled(Handles[0], false));
	for (const auto Handle : Handles)
	{
		CheckSceneFacts(Scene, Handle, {});
	}
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.RemoveNodeKeepChildren(Handles[0]));
	CheckSceneFacts(Scene, Handles[0], {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}});
	CheckSceneFacts(Scene, Handles[1], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFacts(Scene, Handles[2], {});
}

void CheckBatchAndRestoreFacts()
{
	FScene Scene;
	FSceneNode Parent;
	Parent.Id = "parent";
	FSceneNode Child;
	Child.Id = "child";
	Child.Parent() = "parent";
	AddSceneFactValue(Child, "value", 1);
	const auto Handles = Scene.AddNodes({Parent, Child});
	Scene.Acknowledge(Scene.GetRevision());
	const auto Revision = Scene.GetRevision();
	Parent.Local() = Translation({1, 0, 0});
	SetSceneFactValue(Child, "value", 2);
	CheckSceneFact(Scene.EditNodes({{Handles[0], Parent}, {Handles[1], Child}}, Revision));
	CheckSceneFact(Scene.GetRevision() == Revision + 1);
	CheckSceneFacts(Scene, Handles[0], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	CheckSceneFacts(Scene, Handles[1], {{"test.scene-fact", "value", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.RemoveNodeKeepChildren(Handles[0]));
	Scene.Acknowledge(Scene.GetRevision());
	const auto Restored = Scene.AddNodes({Parent}, {{Handles[1], Child}}).front();
	CheckSceneFact(Restored.Generation != Handles[0].Generation);
	CheckSceneFacts(Scene, Restored, {{"hyperion.scenetransform", "hyperion.scenetransform", Added}});
	CheckSceneFacts(Scene, Handles[1], {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	FSceneNode Other;
	Other.Id = "other";
	const auto OtherHandle = Scene.AddNode(Other);
	Scene.Acknowledge(Scene.GetRevision());
	const std::array Roots{Restored, OtherHandle};
	CheckSceneFact(Scene.RemoveSubtrees(Roots));
	CheckSceneFacts(Scene, Restored, {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}});
	CheckSceneFacts(
	    Scene, Handles[1],
	    {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}, {"test.scene-fact", "value", Removed}});
	CheckSceneFacts(Scene, OtherHandle, {{"hyperion.scenetransform", "hyperion.scenetransform", Removed}});
}

void CheckKnownAndOpaqueFacts()
{
	FScene Scene;
	FSceneNode Node;
	Node.Id = "mixed";
	Node.Camera();
	AddSceneFactValue(Node, "known", 1);
	const auto KnownComponents = Node.Components;
	const auto Envelope = std::make_shared<const FArchiveNode>(
	    FArchiveNode::FObject{{"type", WriteValue(std::string("unregistered.type"))}});
	Node.Components.AddOpaque("unknown", Envelope);
	const auto Handle = Scene.AddNode(Node);
	CheckSceneFacts(
	    Scene, Handle,
	    {{"hyperion.scenetransform", "hyperion.scenetransform", Added}, {"test.scene-fact", "known", Added}});
	Scene.Acknowledge(Scene.GetRevision());
	Node.Components = KnownComponents;
	Node.Components.AddOpaque("unknown", std::make_shared<const FArchiveNode>(FArchiveNode::FObject{
	                                         {"type", WriteValue(std::string("different.type"))}}));
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle, {});
	CheckSceneFact(SceneFactChange(Scene, Handle).Mask == ESceneChangeMask::Metadata);
	CheckSceneFact(Scene.FindNode(Handle)->Components.Unknown() == Node.Components.Unknown());
	bool bRejected{};
	try
	{
		WriteValue(SceneEntryFromNode(*Scene.FindNode(Handle)));
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	CheckSceneFact(bRejected);
}

void CheckBuiltinFacts()
{
	FScene Scene;
	auto Node = MakeSceneCameraNode("builtin");
	Node.Model().emplace();
	Node.PointLight().emplace();
	const auto Handle = Scene.AddNode(Node);
	for (const auto Type : {"hyperion.scenecamera", "hyperion.scenepointlight", "hyperion.staticmesh"})
	{
		Scene.Acknowledge(Scene.GetRevision());
		const auto NewId = std::string(Type) + "-instance";
		Node.Components.Rename(Type, NewId);
		CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
		CheckSceneFacts(Scene, Handle, {{Type, Type, Removed}, {Type, NewId, Added}});
		CheckSceneFact(SceneFactChange(Scene, Handle).Mask == ESceneChangeMask::Metadata);
	}
	Scene.Acknowledge(Scene.GetRevision());
	Node.Camera()->Near = .2f;
	Node.PointLight()->Intensity += 1;
	Node.Model()->bVisible = false;
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle,
	                {{"hyperion.scenecamera", "hyperion.scenecamera-instance", Modified},
	                 {"hyperion.scenepointlight", "hyperion.scenepointlight-instance", Modified},
	                 {"hyperion.staticmesh", "hyperion.staticmesh-instance", Modified}});
	CheckSceneFact(SceneFactChange(Scene, Handle).Mask == (ESceneChangeMask::Metadata | ESceneChangeMask::Camera |
	                                                       ESceneChangeMask::Light | ESceneChangeMask::Model));
	Scene.Acknowledge(Scene.GetRevision());
	Scene.SetSettings({Handle, {}});
	CheckSceneFact(Scene.GetChanges().size() == 1 && Scene.GetChanges()[0].ComponentChanges.empty());
	Scene.Acknowledge(Scene.GetRevision());
	Node.Camera().reset();
	CheckSceneFact(Scene.EditNode(Handle, Node, Scene.GetRevision()));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenecamera", "hyperion.scenecamera-instance", Removed}});
	CheckSceneFact(!Scene.GetSettings().DefaultCamera && Scene.GetChanges().back().ComponentChanges.empty());
}

void CheckConvenienceSetterFacts()
{
	FScene Scene;
	auto Node = MakeSceneCameraNode("setters");
	Node.Model().emplace();
	Node.DirectionalLight().emplace();
	Node.EnvironmentLight().emplace();
	Node.PointLight().emplace();
	Node.SpotLight().emplace();
	const auto Handle = Scene.AddNode(Node);
	Scene.Acknowledge(Scene.GetRevision());
	auto Directional = *Node.DirectionalLight();
	Directional.Intensity += 1;
	CheckSceneFact(Scene.SetDirectionalLight(Handle, Directional));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenedirectionallight", "hyperion.scenedirectionallight", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	auto Environment = *Node.EnvironmentLight();
	Environment.Intensity += 1;
	CheckSceneFact(Scene.SetEnvironmentLight(Handle, Environment));
	CheckSceneFacts(Scene, Handle, {{"hyperion.sceneenvironmentlight", "hyperion.sceneenvironmentlight", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	auto Point = *Node.PointLight();
	Point.Intensity += 1;
	CheckSceneFact(Scene.SetPointLight(Handle, Point));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenepointlight", "hyperion.scenepointlight", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	auto Spot = *Node.SpotLight();
	Spot.Intensity += 1;
	CheckSceneFact(Scene.SetSpotLight(Handle, Spot));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenespotlight", "hyperion.scenespotlight", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.SetModelVisible(Handle, false));
	CheckSceneFacts(Scene, Handle, {{"hyperion.staticmesh", "hyperion.staticmesh", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.SetWorldTransform(Handle, Translation({1, 0, 0})));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	auto Camera = *Node.Camera();
	Camera.Near = .2f;
	CheckSceneFact(Scene.SetCameraView(Handle, Translation({2, 0, 0}), Camera));
	CheckSceneFacts(Scene, Handle,
	                {{"hyperion.scenecamera", "hyperion.scenecamera", Modified},
	                 {"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	auto Model = *Scene.Find(Handle);
	Model.World = Translation({3, 0, 0});
	CheckSceneFact(Scene.Update(Handle, Model));
	CheckSceneFacts(Scene, Handle, {{"hyperion.scenetransform", "hyperion.scenetransform", Modified}});
	Scene.Acknowledge(Scene.GetRevision());
	CheckSceneFact(Scene.SetName(Handle, "renamed") && Scene.SetEnabled(Handle, false));
	CheckSceneFacts(Scene, Handle, {});
}
} // namespace

void CheckSceneComponentChanges()
{
	RegisterSceneFactTypes();
	CheckSceneFact(HasComponentChange(Added | Removed, Added) && !HasComponentChange(Added, Modified));
	CheckIdentityFacts();
	CheckOccurrenceFacts();
	CheckRemovedAndOwnedFacts();
	CheckSynchronizationFacts();
	CheckHierarchyFacts();
	CheckBatchAndRestoreFacts();
	CheckKnownAndOpaqueFacts();
	CheckBuiltinFacts();
	CheckConvenienceSetterFacts();
}
