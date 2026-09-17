#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace
{
using namespace Hyperion;

FSceneNode Group(std::string InId, std::string InParent = {}, FMat4 InLocal = Identity())
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.Name = Node.Id;
	Node.Parent() = std::move(InParent);
	Node.Local() = InLocal;
	return Node;
}

FMat4 World(const FScene& InScene, FSceneHandle InHandle)
{
	FMat4 Result;
	HYP_CHECK(InScene.GetWorld(InHandle, Result));
	return Result;
}

bool Near(const FMat4& InA, const FMat4& InB, float InEpsilon = 1e-4f)
{
	for (std::size_t Index = 0; Index < 16; ++Index)
	{
		if (!std::isfinite(InA.Values[Index]) || std::abs(InA.Values[Index] - InB.Values[Index]) > InEpsilon)
		{
			return false;
		}
	}
	return true;
}

FMat4 ProductOracle(const FMat4& InA, const FMat4& InB)
{
	FMat4 Result;
	for (std::size_t Column = 0; Column < 4; ++Column)
	{
		for (std::size_t Row = 0; Row < 4; ++Row)
		{
			double Value{};
			for (std::size_t K = 0; K < 4; ++K)
			{
				Value += double(InA.Values[K * 4 + Row]) * InB.Values[Column * 4 + K];
			}
			Result.Values[Column * 4 + Row] = static_cast<float>(Value);
		}
	}
	return Result;
}

struct FCapturedNode
{
	FSceneHandle Handle;
	FSceneNode Node;
	std::array<float, 16> World;
	bool bEnabled{};
	std::vector<FSceneHandle> Children;
	bool operator==(const FCapturedNode&) const = default;
};

std::vector<FCapturedNode> Capture(const FScene& InScene)
{
	std::vector<FCapturedNode> Result;
	for (const auto Handle : InScene.GetNodes())
	{
		Result.push_back({Handle, *InScene.FindNode(Handle), World(InScene, Handle).Values,
		                  InScene.IsEffectivelyEnabled(Handle), InScene.GetChildren(Handle)});
	}
	return Result;
}

void RejectUnchanged(FScene& InScene, const std::function<void()>& InOperation)
{
	const auto Before = Capture(InScene);
	const auto Roots = InScene.GetRoots();
	const auto Revision = InScene.GetRevision();
	const auto Settings = InScene.GetSettings();
	const auto Changes = InScene.GetChanges();
	bool bRejected{};
	try
	{
		InOperation();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected && Capture(InScene) == Before && InScene.GetRoots() == Roots);
	HYP_CHECK(InScene.GetRevision() == Revision && InScene.GetSettings() == Settings);
	const auto After = InScene.GetChanges();
	HYP_CHECK(Changes.size() == After.size());
	for (std::size_t Index = 0; Index < After.size(); ++Index)
	{
		const auto& A = Changes[Index];
		const auto& B = After[Index];
		HYP_CHECK(A.Handle == B.Handle && A.Revision == B.Revision && A.Mask == B.Mask && A.Kind == B.Kind &&
		          A.Node == B.Node && A.World.Values == B.World.Values && A.bEffectiveEnabled == B.bEffectiveEnabled &&
		          A.bRemoved == B.bRemoved && A.Settings == B.Settings);
	}
}

void CheckNodeIdentityAndChanges()
{
	FScene Scene;
	const auto Parent = Scene.AddNode(Group("parent", {}, Translation({5, 0, 0})));
	auto ModelNode = Group("model", "parent", Translation({2, 0, 0}));
	ModelNode.Model() = FSceneModelComponent{};
	const auto Model = Scene.AddNode(ModelNode);
	HYP_CHECK(World(Scene, Model).Values[12] == 7 && Scene.Find(Model)->World.Values[12] == 7);
	const auto Camera = Scene.AddNode(MakeSceneCameraNode("camera"));
	const auto Light = Scene.AddNode(MakeSceneDirectionalLightNode("light"));
	const auto Ambient = Scene.AddNode(MakeSceneEnvironmentLightNode("ambient"));
	HYP_CHECK(Scene.GetNodes().size() == 5 && Scene.GetHandles() == std::vector<FSceneHandle>{Model});
	for (const auto Kind : {ESceneNodeKind::Group, ESceneNodeKind::Model, ESceneNodeKind::Camera,
	                        ESceneNodeKind::DirectionalLight, ESceneNodeKind::EnvironmentLight})
	{
		HYP_CHECK(Scene.CountNodes(Kind) == 1 && Scene.GetNodes(Kind).size() == 1);
	}
	Scene.Acknowledge(Scene.GetRevision());
	auto CameraValue = *Scene.FindNode(Camera)->Camera();
	CameraValue.Far = 600;
	CameraValue.FocusDistance = 20;
	HYP_CHECK(Scene.SetCamera(Camera, CameraValue));
	HYP_CHECK(Scene.SetLocalTransform(Camera, Translation({0, 2, 8})));
	HYP_CHECK(Scene.SetName(Camera, "changed"));
	const auto Changes = Scene.GetChanges();
	HYP_CHECK(Changes.size() == 1 && Changes[0].Node->Camera() == CameraValue && Changes[0].Node->Name == "changed");
	HYP_CHECK(HasChange(Changes[0].Mask, ESceneChangeMask::Camera) &&
	          HasChange(Changes[0].Mask, ESceneChangeMask::Transform) &&
	          HasChange(Changes[0].Mask, ESceneChangeMask::Metadata));
	const auto Revision = Scene.GetRevision();
	HYP_CHECK(Scene.SetCamera(Camera, CameraValue) && Scene.SetWorldTransform(Camera, World(Scene, Camera)));
	HYP_CHECK(Scene.SetModelComponent(Model, *Scene.FindNode(Model)->Model()) && Scene.SetEnabled(Parent, true));
	HYP_CHECK(Scene.Reparent(Model, Parent, ESceneReparentMode::KeepWorld) && Scene.GetRevision() == Revision);
	Scene.SetSettings({Camera, Light, Ambient});
	Scene.Acknowledge(Scene.GetRevision());
	Scene.RemoveSubtree(Light);
	HYP_CHECK(!Scene.GetSettings().MainDirectionalLight && Scene.GetSettings().DefaultCamera == Camera);
	const auto Removed = Scene.GetChanges();
	HYP_CHECK(Removed.size() == 2 && Removed[0].bRemoved && Removed[0].Kind == ESceneNodeKind::DirectionalLight);
	HYP_CHECK(Removed[1].Settings && !Removed[1].Settings->MainDirectionalLight);
	const auto Reused = Scene.AddNode(Group("new"));
	HYP_CHECK(Reused.Slot == Light.Slot && Reused.Generation != Light.Generation && !Scene.FindNode(Light));
	HYP_CHECK(Scene.GetNodes().back() == Reused && !Scene.SetName(Light, "stale"));
	FScene Foreign;
	const auto ForeignHandle = Foreign.AddNode(Group("foreign"));
	HYP_CHECK(!Scene.SetEnabled(ForeignHandle, false) && !Scene.RemoveSubtree(ForeignHandle));
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.AddNode(Group("camera"));
	                });
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.SetCamera(Model, {});
	                });
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.SetSettings({ForeignHandle, {}, {}});
	                });
	const auto Generated = Scene.AddNode(Group(""));
	HYP_CHECK(!Scene.FindNode(Generated)->Id.empty());
	Scene.Acknowledge(Scene.GetRevision());
	Scene.BeginSynchronization();
	HYP_CHECK(Scene.GetChanges().size() == Scene.GetNodes().size() + 1);
	Scene.EndSynchronization();
}

void CheckInheritedEnabledAndRemoval()
{
	FScene Scene;
	const auto Root = Scene.AddNode(Group("root", {}, Translation({3, 0, 0})));
	const auto Other = Scene.AddNode(Group("other"));
	auto ModelNode = Group("model", "root", Translation({2, 0, 0}));
	ModelNode.Model() = FSceneModelComponent{};
	const auto Model = Scene.AddNode(ModelNode);
	auto CameraNode = MakeSceneCameraNode("camera");
	CameraNode.Parent() = "model";
	const auto Camera = Scene.AddNode(CameraNode);
	Scene.SetModelVisible(Model, false);
	HYP_CHECK(!Scene.Find(Model)->bVisible && Scene.IsEffectivelyEnabled(Camera));
	Scene.SetEnabled(Root, false);
	Scene.SetLocalTransform(Model, Translation({4, 0, 0}));
	HYP_CHECK(!Scene.IsEffectivelyEnabled(Model) && !Scene.IsEffectivelyEnabled(Camera));
	Scene.SetEnabled(Model, false);
	Scene.SetEnabled(Model, true);
	HYP_CHECK(!Scene.IsEffectivelyEnabled(Camera));
	Scene.Reparent(Camera, Other, ESceneReparentMode::KeepLocal);
	HYP_CHECK(Scene.IsEffectivelyEnabled(Camera));
	Scene.Reparent(Camera, Model, ESceneReparentMode::KeepWorld);
	HYP_CHECK(!Scene.IsEffectivelyEnabled(Camera));
	const auto Before = World(Scene, Camera);
	Scene.RemoveNodeKeepChildren(Model);
	HYP_CHECK(Scene.FindNode(Camera)->Parent() == "root" && Near(World(Scene, Camera), Before));
	HYP_CHECK(!Scene.IsEffectivelyEnabled(Camera));
	Scene.RemoveNodeKeepChildren(Root);
	HYP_CHECK(Scene.FindNode(Camera)->Parent().empty() && Near(World(Scene, Camera), Before) &&
	          Scene.IsEffectivelyEnabled(Camera));
	HYP_CHECK(Scene.GetNodes().size() == 2 && Scene.CountNodes(ESceneNodeKind::Model) == 0);
	Scene.SetSettings({Camera, {}, {}});
	Scene.RemoveSubtree(Camera);
	HYP_CHECK(!Scene.GetSettings().DefaultCamera && Scene.GetNodes() == std::vector<FSceneHandle>{Other});
}

void CheckBulkClear(FScene& InScene)
{
	const auto Handles = InScene.GetNodes();
	const auto Revision = InScene.GetRevision();
	InScene.Acknowledge(Revision);
	InScene.Clear();
	HYP_CHECK(InScene.GetRevision() == Revision + 1);
	HYP_CHECK(InScene.GetNodes().empty() && InScene.GetRoots().empty() && InScene.CountNodes() == 0);
	HYP_CHECK(InScene.CountNodes(ESceneNodeKind::Camera) == 0 && !InScene.GetSettings().DefaultCamera);
	const auto Changes = InScene.GetChanges();
	const auto RemovedCount = std::count_if(Changes.begin(), Changes.end(),
	                                        [](const FSceneChange& InChange)
	                                        {
		                                        return InChange.bRemoved;
	                                        });
	HYP_CHECK(static_cast<std::size_t>(RemovedCount) == Handles.size());
	for (const auto Handle : Handles)
	{
		HYP_CHECK(!InScene.FindNode(Handle));
	}
	InScene.Clear();
	HYP_CHECK(InScene.GetRevision() == Revision + 1);
	const auto Reused = InScene.AddNode(Group("reused-after-clear"));
	HYP_CHECK(Reused.Slot == Handles.back().Slot && Reused.Generation != Handles.back().Generation);
	HYP_CHECK(InScene.FindNode(Reused) && !InScene.FindNode(Handles.back()));
}

void CheckWideKeepChildren()
{
	constexpr std::size_t Width = 128;
	FScene Scene;
	std::vector<FSceneNode> Nodes{Group("root", {}, Translation({3, 0, 0})), Group("other"), Group("before", "root"),
	                              Group("removed", "root", Translation({2, 0, 0})), Group("after", "root")};
	Nodes[3].bEnabled = false;
	for (std::size_t Index = 0; Index < Width; ++Index)
	{
		auto Node = Group("wide-" + std::to_string(Index), "removed", Translation({static_cast<float>(Index), 1, 0}));
		if (Index == 0)
		{
			Node.Camera() = FSceneCamera{};
		}
		Nodes.push_back(std::move(Node));
	}
	Nodes.push_back(Group("grandchild", "wide-" + std::to_string(Width - 1), Translation({0, 0, 2})));
	Scene.LoadNodes(std::move(Nodes));
	const auto Root = Scene.FindHandle("root");
	const auto Removed = Scene.FindHandle("removed");
	const auto Grandchild = Scene.FindHandle("grandchild");
	const auto GrandWorld = World(Scene, Grandchild);
	const auto Children = Scene.GetChildren(Removed);
	std::vector<FMat4> Worlds;
	for (const auto Child : Children)
	{
		Worlds.push_back(World(Scene, Child));
		HYP_CHECK(!Scene.IsEffectivelyEnabled(Child));
	}
	Scene.SetSettings({Children.front(), {}, {}});
	auto ExpectedOrder = Scene.GetNodes();
	std::erase(ExpectedOrder, Removed);
	std::vector<FSceneHandle> ExpectedChildren{Scene.FindHandle("before"), Scene.FindHandle("after")};
	ExpectedChildren.insert(ExpectedChildren.end(), Children.begin(), Children.end());
	const auto Roots = Scene.GetRoots();
	const auto Revision = Scene.GetRevision();
	Scene.Acknowledge(Revision);
	HYP_CHECK(Scene.RemoveNodeKeepChildren(Removed));
	HYP_CHECK(Scene.GetRevision() == Revision + 1 && !Scene.FindNode(Removed));
	HYP_CHECK(Scene.GetNodes() == ExpectedOrder && Scene.GetRoots() == Roots);
	HYP_CHECK(Scene.GetChildren(Root) == ExpectedChildren && Scene.GetSettings().DefaultCamera == Children.front());
	for (std::size_t Index = 0; Index < Children.size(); ++Index)
	{
		const auto Child = Children[Index];
		HYP_CHECK(Scene.FindNode(Child)->Parent() == "root");
		HYP_CHECK(Scene.FindHandle("wide-" + std::to_string(Index)) == Child);
		HYP_CHECK(Near(World(Scene, Child), Worlds[Index]) && Scene.IsEffectivelyEnabled(Child));
	}
	HYP_CHECK(Near(World(Scene, Grandchild), GrandWorld) && Scene.IsEffectivelyEnabled(Grandchild));
	const auto Changes = Scene.GetChanges();
	const auto RemovedChange = std::find_if(Changes.begin(), Changes.end(),
	                                        [&](const FSceneChange& InChange)
	                                        {
		                                        return InChange.Handle == Removed;
	                                        });
	HYP_CHECK(RemovedChange != Changes.end() && RemovedChange->bRemoved);
	CheckBulkClear(Scene);
}

void CheckCompatibilityVisibility()
{
	FScene Scene;
	const auto Parent = Scene.AddNode(Group("parent", {}, ComposeTRS({3, 2, 1}, {0, .6f, 0, .8f}, {2, 3, 4})));
	auto Node = Group("model", "parent", Translation({2, 0, 0}));
	Node.Model() = FSceneModelComponent{};
	const auto Model = Scene.AddNode(Node);
	for (const auto Disabled : {Parent, Model})
	{
		Scene.SetEnabled(Disabled, false);
		const auto Revision = Scene.GetRevision();
		auto Copy = *Scene.Find(Model);
		HYP_CHECK(!Copy.bVisible && Scene.FindModelComponent(Model)->bVisible);
		HYP_CHECK(Scene.Update(Model, Copy) && Scene.GetRevision() == Revision);
		Copy.Name = "renamed";
		HYP_CHECK(Scene.Update(Model, Copy));
		Copy.World = Translation({7, 8, 9});
		HYP_CHECK(Scene.Update(Model, Copy));
		HYP_CHECK(Scene.FindModelComponent(Model)->bVisible && !Scene.Find(Model)->bVisible);
		Scene.SetEnabled(Disabled, true);
		HYP_CHECK(Scene.Find(Model)->bVisible && Near(World(Scene, Model), Copy.World));
	}
	auto Copy = *Scene.Find(Model);
	Copy.bVisible = false;
	Scene.Update(Model, Copy);
	HYP_CHECK(!Scene.FindModelComponent(Model)->bVisible);
	Scene.SetEnabled(Parent, false);
	Copy = *Scene.Find(Model);
	Copy.Name = "hidden edit";
	Scene.Update(Model, Copy);
	Scene.SetEnabled(Parent, true);
	HYP_CHECK(!Scene.Find(Model)->bVisible && !Scene.FindModelComponent(Model)->bVisible);
	Scene.SetEnabled(Parent, false);
	Scene.SetModelVisible(Model, true);
	Scene.SetModelVisible(Model, false);
	Scene.SetEnabled(Parent, true);
	HYP_CHECK(!Scene.Find(Model)->bVisible);
}

void CheckHierarchyOracleAndTransactions()
{
	FScene Scene;
	const auto ParentWorld = ComposeTRS({4, 1, -2}, {0, std::sin(.2f), 0, std::cos(.2f)}, {-2, .5f, 3});
	const auto Parent = Scene.AddNode(Group("parent", {}, ParentWorld));
	const auto Child = Scene.AddNode(Group("child", "parent", Translation({1, 2, 3})));
	const auto Grand = Scene.AddNode(Group("grand", "child", Scale({.5f, 3, 2})));
	HYP_CHECK(Near(World(Scene, Child), ProductOracle(ParentWorld, Scene.FindNode(Child)->Local())));
	HYP_CHECK(Near(World(Scene, Grand), ProductOracle(ProductOracle(ParentWorld, Scene.FindNode(Child)->Local()),
	                                                  Scene.FindNode(Grand)->Local())));
	const auto Other = Scene.AddNode(Group("other", {}, Translation({-5, 2, 1})));
	const auto OldWorld = World(Scene, Grand);
	Scene.Reparent(Grand, Other, ESceneReparentMode::KeepWorld);
	HYP_CHECK(Near(World(Scene, Grand), OldWorld));
	const auto Local = Scene.FindNode(Grand)->Local();
	Scene.Reparent(Grand, Child, ESceneReparentMode::KeepLocal);
	HYP_CHECK(Scene.FindNode(Grand)->Local().Values == Local.Values &&
	          Near(World(Scene, Grand), ProductOracle(World(Scene, Child), Local)));
	Scene.SetWorldTransform(Grand, Translation({7, 8, 9}));
	HYP_CHECK(Near(World(Scene, Grand), Translation({7, 8, 9})));
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.Reparent(Parent, Grand, ESceneReparentMode::KeepLocal);
	                });
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.Reparent(Child, Child, ESceneReparentMode::KeepLocal);
	                });
	const auto Singular = Scene.AddNode(Group("singular", {}, Scale({1, 0, 1})));
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.Reparent(Grand, Singular, ESceneReparentMode::KeepWorld);
	                });
	auto CameraNode = MakeSceneCameraNode("camera");
	CameraNode.Parent() = "child";
	Scene.AddNode(CameraNode);
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.SetLocalTransform(Parent, Scale({1, 0, 1}));
	                });
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.AddNode(Group("overflow", "parent", Scale({std::numeric_limits<float>::max(), 1, 1})));
	                });
	const auto Intermediate = Scene.AddNode(Group("intermediate", "singular"));
	Scene.AddNode(Group("leaf", "intermediate"));
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.RemoveNodeKeepChildren(Intermediate);
	                });
}

void CheckDocumentInstallation()
{
	FScene Scene;
	for (const auto Nodes :
	     {std::vector<FSceneNode>{Group("self", "self")}, std::vector<FSceneNode>{Group("a", "b"), Group("b", "a")},
	      std::vector<FSceneNode>{Group("a"), Group("a")}, std::vector<FSceneNode>{Group("a", "missing")}})
	{
		RejectUnchanged(Scene,
		                [&]
		                {
			                Scene.LoadNodes(Nodes);
		                });
	}
	auto Camera = MakeSceneCameraNode("camera");
	Camera.Parent() = "flat";
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.LoadNodes({Camera, Group("flat", {}, Scale({1, 0, 1}))});
	                });
	auto Bad = Group("");
	Bad.Camera() = FSceneCamera{};
	Bad.Local() = Scale({1, 0, 1});
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.AddNode(Bad);
	                });
	const auto First = Scene.AddNode(Group(""));
	HYP_CHECK(Scene.FindNode(First)->Id == "node-1");
	Scene.Clear();
	constexpr std::size_t Depth = 4096;
	std::vector<FSceneNode> Nodes;
	for (std::size_t Index = Depth; Index > 0; --Index)
	{
		Nodes.push_back(Group("deep-" + std::to_string(Index),
		                      Index == 1 ? std::string{} : "deep-" + std::to_string(Index - 1),
		                      Translation({1, 0, 0})));
	}
	const auto Handles = Scene.LoadNodes(std::move(Nodes));
	HYP_CHECK(Handles.size() == Depth && Scene.GetRoots() == std::vector<FSceneHandle>{Handles.back()});
	HYP_CHECK(World(Scene, Handles.front()).Values[12] == Depth);
	Scene.SetLocalTransform(Handles.back(), Translation({2, 0, 0}));
	HYP_CHECK(World(Scene, Handles.front()).Values[12] == Depth + 1);
	Scene.SetEnabled(Handles.back(), false);
	Scene.SetLocalTransform(Handles[20], Translation({3, 0, 0}));
	HYP_CHECK(!Scene.IsEffectivelyEnabled(Handles.front()));
	Scene.Clear();
	HYP_CHECK(Scene.GetNodes().empty() && Scene.GetRoots().empty());
}

void CheckCameraAndLightValues()
{
	FScene Scene;
	auto CameraNode = Group("camera");
	CameraNode.Camera() = FSceneCamera{};
	const auto Camera = Scene.AddNode(CameraNode);
	FSceneCameraPose Pose;
	HYP_CHECK(Scene.GetCameraPose(Camera, Pose));
	HYP_CHECK(Pose.Eye.X == 0 && Pose.Eye.Y == 0 && Pose.Eye.Z == 0 && Pose.Forward.Z == -1 && Pose.Up.Y == 1 &&
	          Pose.Right.X == 1);
	constexpr float Angle = .4f;
	const auto Parent = Scene.AddNode(
	    Group("parent", {}, ComposeTRS({2, 5, 3}, {0, std::sin(Angle / 2), 0, std::cos(Angle / 2)}, {-2, .5f, 3})));
	Scene.Reparent(Camera, Parent, ESceneReparentMode::KeepLocal);
	HYP_CHECK(Scene.GetCameraPose(Camera, Pose));
	HYP_CHECK(std::abs(Pose.Forward.X + std::sin(Angle)) < 1e-5f && std::abs(Pose.Forward.Z + std::cos(Angle)) < 1e-5f);
	HYP_CHECK(std::abs(Pose.Up.Y - 1) < 1e-5f && std::abs(Dot(Pose.Forward, Pose.Right)) < 1e-5f && Pose.Eye.Y == 5);
	for (unsigned Case = 0; Case < 7; ++Case)
	{
		auto Lens = *Scene.FindNode(Camera)->Camera();
		if (Case == 0)
		{
			Lens.VerticalRadians = .01f;
		}
		if (Case == 1)
		{
			Lens.VerticalRadians = 3.f;
		}
		if (Case == 2)
		{
			Lens.Near = 0;
		}
		if (Case == 3)
		{
			Lens.Far = Lens.Near;
		}
		if (Case == 4)
		{
			Lens.FocusDistance = 0;
		}
		if (Case == 5)
		{
			Lens.VerticalRadians = std::numeric_limits<float>::quiet_NaN();
		}
		if (Case == 6)
		{
			Lens.Far = std::numeric_limits<float>::infinity();
		}
		RejectUnchanged(Scene,
		                [&]
		                {
			                Scene.SetCamera(Camera, Lens);
		                });
	}
	auto Lens = *Scene.FindNode(Camera)->Camera();
	Lens.FocusDistance = 50;
	Scene.SetCamera(Camera, Lens);
	FSceneCameraPose After;
	HYP_CHECK(Scene.GetCameraPose(Camera, After));
	HYP_CHECK(After.Eye.X == Pose.Eye.X && After.Forward.X == Pose.Forward.X);
}

void CheckDefaultLightValues()
{
	FSceneCameraPose Pose;
	FScene Defaults;
	const auto Handles = AddDefaultSceneContent(Defaults);
	HYP_CHECK(Defaults.GetNodes().size() == 3 && Defaults.GetHandles().empty());
	HYP_CHECK(Defaults.GetCameraPose(Handles[0], Pose) && std::abs(Pose.Eye.Y - 3) < 1e-5f &&
	          std::abs(Pose.Eye.Z - 12) < 1e-5f);
	const auto LightPose = ExtractScenePose(World(Defaults, Handles[1]));
	const auto LengthOracle = std::sqrt(.45f * .45f + .8f * .8f + .65f * .65f);
	HYP_CHECK(std::abs(-LightPose.Forward.X - (-.45f / LengthOracle)) < 1e-5f);
	HYP_CHECK(std::abs(-LightPose.Forward.Y - .8f / LengthOracle) < 1e-5f);
	auto Light = *Defaults.FindNode(Handles[1])->DirectionalLight();
	for (unsigned Case = 0; Case < 4; ++Case)
	{
		auto Bad = Light;
		if (Case == 0)
		{
			Bad.Color.X = -1;
		}
		if (Case == 1)
		{
			Bad.Intensity = std::numeric_limits<float>::infinity();
		}
		if (Case == 2)
		{
			Bad.Intensity = std::numeric_limits<float>::max();
		}
		if (Case == 3)
		{
			Bad.Color.Y = std::numeric_limits<float>::quiet_NaN();
		}
		RejectUnchanged(Defaults,
		                [&]
		                {
			                Defaults.SetDirectionalLight(Handles[1], Bad);
		                });
	}
	Defaults.Acknowledge(Defaults.GetRevision());
	Light.Intensity = 0;
	Light.bCastShadows = false;
	Defaults.SetDirectionalLight(Handles[1], Light);
	HYP_CHECK(Defaults.GetChanges()[0].Node->DirectionalLight() == Light &&
	          SceneLightRadiance(Light.Color, Light.Intensity).X == 0);
	auto Ambient = *Defaults.FindNode(Handles[2])->EnvironmentLight();
	Ambient.Intensity = 2;
	Defaults.SetEnvironmentLight(Handles[2], Ambient);
	HYP_CHECK(Defaults.GetChanges().back().Node->EnvironmentLight() == Ambient);
	const auto BeforeAmbient = *Defaults.FindNode(Handles[2])->EnvironmentLight();
	Defaults.SetLocalTransform(Handles[2], Scale({0, 0, 0}));
	HYP_CHECK(Defaults.FindNode(Handles[2])->EnvironmentLight() == BeforeAmbient);
}

void CheckLocalLightNodes()
{
	FScene Scene;
	const auto Parent = Scene.AddNode(Group("local-rig", {}, Translation({3, 2, 1})));
	auto Point = MakeScenePointLightNode("point");
	Point.Parent() = "local-rig";
	Point.PointLight() = FScenePointLight{{1, .5f, .2f}, 8, 4};
	const auto A = Scene.AddNode(Point);
	auto Spot = MakeSceneSpotLightNode("spot");
	Spot.Parent() = "local-rig";
	Spot.SpotLight() = FSceneSpotLight{{.2f, .5f, 1}, 12, 6, .2f, .7f};
	const auto B = Scene.AddNode(Spot);
	HYP_CHECK(Scene.CountNodes(ESceneNodeKind::PointLight) == 1 && Scene.CountNodes(ESceneNodeKind::SpotLight) == 1);
	HYP_CHECK(World(Scene, A).Values[12] == 3 && Scene.FindSpotLight(B)->Range == 6);
	Scene.SetLocalTransform(Parent, Multiply(Translation({4, 3, 2}), Scale({2, 3, 4})));
	HYP_CHECK(Scene.FindPointLight(A)->Range == 4 && Scene.FindSpotLight(B)->Range == 6);
	Scene.SetEnabled(Parent, false);
	HYP_CHECK(!Scene.IsEffectivelyEnabled(A) && !Scene.IsEffectivelyEnabled(B));
	Scene.SetEnabled(Parent, true);
	Scene.Acknowledge(Scene.GetRevision());
	auto Light = *Scene.FindPointLight(A);
	Light.Intensity = 9;
	Scene.SetPointLight(A, Light);
	HYP_CHECK(HasChange(Scene.GetChanges().front().Mask, ESceneChangeMask::Light));
	for (const float Range : {0.f, -1.f, std::numeric_limits<float>::infinity()})
	{
		Light.Range = Range;
		RejectUnchanged(Scene,
		                [&]
		                {
			                Scene.SetPointLight(A, Light);
		                });
	}
	auto Cone = *Scene.FindSpotLight(B);
	Cone.InnerRadians = Cone.OuterRadians;
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.SetSpotLight(B, Cone);
	                });
	Cone = *Scene.FindSpotLight(B);
	Cone.Color.X = -1;
	RejectUnchanged(Scene,
	                [&]
	                {
		                Scene.SetSpotLight(B, Cone);
	                });
	Scene.Reparent(B, {}, ESceneReparentMode::KeepWorld);
	HYP_CHECK(World(Scene, B).Values[12] == 4);
	Scene.RemoveSubtree(Parent);
	HYP_CHECK(!Scene.FindPointLight(A) && Scene.FindSpotLight(B));
}
} // namespace

void CheckSceneNodes()
{
	CheckNodeIdentityAndChanges();
	CheckInheritedEnabledAndRemoval();
	CheckWideKeepChildren();
	CheckCompatibilityVisibility();
	CheckHierarchyOracleAndTransactions();
	CheckDocumentInstallation();
	CheckCameraAndLightValues();
	CheckDefaultLightValues();
	CheckLocalLightNodes();
}
