#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/Serialization/Archive.h"
#include "Support/TestSupport.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace
{
using namespace Hyperion;

void CheckBoundsEqual(const FBounds& InActual, const FBounds& InExpected)
{
	HYP_CHECK(IsUsable(InActual) && IsUsable(InExpected));
	HYP_CHECK(Length(Subtract(InActual.Minimum, InExpected.Minimum)) < .001f);
	HYP_CHECK(Length(Subtract(InActual.Maximum, InExpected.Maximum)) < .001f);
}

void CheckFramingFit(const FBounds& InBounds)
{
	for (const float Aspect : {.25f, 1.f, 4.f})
	{
		FSceneCameraView Camera;
		Camera.World = SceneCameraTransform({12, 5, 20}, {3, 2, -1});
		const auto Before = ExtractScenePose(Camera.World);
		const float FieldOfView = Camera.Lens.VerticalRadians;
		FitSceneCamera(Camera, InBounds, Aspect, true);
		ValidateSceneCameraView(Camera);
		const auto After = ExtractScenePose(Camera.World);
		const auto Center = ScaleVector(Add(InBounds.Minimum, InBounds.Maximum), .5f);
		const auto Target = Add(After.Eye, ScaleVector(After.Forward, Camera.Lens.FocusDistance));
		HYP_CHECK(Length(Subtract(Target, Center)) < .002f);
		HYP_CHECK(Length(Subtract(After.Forward, Before.Forward)) < .00001f);
		HYP_CHECK(Length(Subtract(After.Up, Before.Up)) < .00001f);
		HYP_CHECK(Camera.Lens.VerticalRadians == FieldOfView);
		for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
		{
			const auto Matrix = SceneCameraViewProjection(After, Camera.Lens, Aspect, Convention);
			for (unsigned Index = 0; Index < 8; ++Index)
			{
				const auto Point = BoundsCorner(InBounds, Index);
				const auto Clip = Transform(Matrix, {Point.X, Point.Y, Point.Z, 1});
				HYP_CHECK(Clip.W > 0 && std::abs(Clip.X) < Clip.W && std::abs(Clip.Y) < Clip.W);
				HYP_CHECK(Clip.Z >= 0 && Clip.Z <= Clip.W);
			}
		}
	}
}

void CheckInvalidFraming()
{
	FSceneCameraView Camera;
	Camera.World = SceneCameraTransform({2, 1, 6}, {});
	const auto Before = Camera;
	const FBounds Valid{{-1, -1, -1}, {1, 1, 1}, true};
	for (const float Aspect : {0.f, -1.f, std::numeric_limits<float>::quiet_NaN()})
	{
		bool bRejected{};
		try
		{
			FitSceneCamera(Camera, Valid, Aspect, true);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && Camera == Before);
	}
	for (const FBounds Bounds : {FBounds{}, FBounds{{2, 0, 0}, {1, 1, 1}, true},
	                             FBounds{{}, {std::numeric_limits<float>::infinity(), 1, 1}, true},
	                             FBounds{{-1e30f, -1e30f, -1e30f}, {1e30f, 1e30f, 1e30f}, true}})
	{
		bool bRejected{};
		try
		{
			FitSceneCamera(Camera, Bounds, 1, true);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && Camera == Before);
	}
	CheckFramingFit({{1, 2, 3}, {1, 2, 3}, true});
}

void CheckPrimitiveFraming(FSceneInstance& InScene, FSceneHandle InHandle)
{
	auto Model = *InScene.FindNode(InHandle)->Model();
	const auto& Asset = *Model.Data->Asset;
	for (std::size_t Index = 0; Index < Asset.Nodes.size(); ++Index)
	{
		if (Asset.Nodes[Index].Primitives.empty())
		{
			continue;
		}
		const auto Primitive = Asset.Nodes[Index].Primitives.front();
		Model.SourceNode = ModelNodeId(Asset, Index);
		Model.SourcePrimitive = ModelPrimitiveId(Asset, Primitive);
		Model.bVisible = false;
		HYP_CHECK(InScene.SetModelComponent(InHandle, Model));
		HYP_CHECK(InScene.SetEnabled(InHandle, false));
		FSceneNodeView View;
		HYP_CHECK(InScene.GetNodeView(InHandle, View));
		const std::array Handles{InHandle};
		CheckBoundsEqual(SceneSelectionBounds(InScene, Handles),
		                 TransformBounds(Model.Data->PrimitiveBounds[Primitive], View.World));
		return;
	}
	throw std::runtime_error("Framing fixture has no model primitive");
}

void CheckFallbackFraming(FSceneInstance& InScene)
{
	FSceneNode Empty;
	Empty.Id = "framing-empty";
	Empty.Local() = Translation({4, 5, 6});
	auto Light = MakeScenePointLightNode("framing-light");
	Light.Local() = Empty.Local();
	Light.PointLight()->Range = 10000;
	auto Camera = MakeSceneCameraNode("framing-camera", {4, 5, 6}, {});
	const auto Nodes = InScene.AddNodes({Empty, Light, Camera});
	const FBounds Expected{{3.5f, 4.5f, 5.5f}, {4.5f, 5.5f, 6.5f}, true};
	for (const auto Handle : Nodes)
	{
		const std::array Handles{Handle};
		CheckBoundsEqual(SceneSelectionBounds(InScene, Handles), Expected);
	}
	CheckFramingFit(Expected);
	HYP_CHECK(!IsUsable(SceneSelectionBounds(InScene, {})));
	const auto Stale = Nodes.front();
	HYP_CHECK(InScene.RemoveSubtree(Stale));
	bool bRejected{};
	try
	{
		const std::array Handles{Nodes[1], Stale};
		SceneSelectionBounds(InScene, Handles);
	}
	catch (const FSceneEditError& Error)
	{
		bRejected = Error.Code == SceneEditErrors::StaleHandle;
	}
	HYP_CHECK(bRejected);
	InScene.RemoveSubtree(Nodes[1]);
	InScene.RemoveSubtree(Nodes[2]);
}

void CheckPreparingFraming(FSceneInstance& InScene)
{
	FSceneNode Node;
	Node.Id = "framing-loading";
	Node.Model() = FSceneModelComponent{};
	Node.Local() = Translation({7, 8, 9});
	const auto Loading = InScene.AddNode(Node);
	const std::array Selected{Loading};
	const auto Revision = InScene.GetRevision();
	bool bRejected{};
	try
	{
		SceneSelectionBounds(InScene, Selected);
	}
	catch (const FSceneEditError& Error)
	{
		bRejected = Error.Code == SceneEditErrors::Busy;
	}
	HYP_CHECK(bRejected && InScene.GetRevision() == Revision);
	HYP_CHECK(InScene.RemoveSubtree(Loading));

	Node.Id = "framing-failed";
	const FAssetRef Missing{"", "FramingMissingModel.hasset", RecordType<FModelAsset>().Id, ""};
	Node.Model()->Asset = InScene.RegisterModelAsset(Missing);
	const auto Failed = InScene.AddNode(Node);
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (InScene.GetError(Failed).empty() && std::chrono::steady_clock::now() < Deadline)
	{
		InScene.Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	HYP_CHECK(!InScene.GetError(Failed).empty() && !InScene.FindNode(Failed)->Model()->Data);
	const std::array FailedSelection{Failed};
	const auto Snapshot = Serialize(InScene.Snapshot("FailedFraming.hasset"));
	const auto FailedRevision = InScene.GetRevision();
	CheckBoundsEqual(SceneSelectionBounds(InScene, FailedSelection), {{6.5f, 7.5f, 8.5f}, {7.5f, 8.5f, 9.5f}, true});
	HYP_CHECK(InScene.GetRevision() == FailedRevision);
	HYP_CHECK(Serialize(InScene.Snapshot("FailedFraming.hasset")) == Snapshot);
	HYP_CHECK(InScene.RemoveSubtree(Failed));
	InScene.Tick();
}
} // namespace

void CheckSelectionFraming(Hyperion::FSceneInstance& InScene)
{
	const auto Original = *InScene.FindNode(InScene.GetNodes(ESceneNodeKind::Model).front());
	FSceneNode Rig;
	Rig.Id = "framing-rig";
	Rig.Local() = ComposeTRS({100, 7, -5}, {0, .38268343f, 0, .92387953f}, {2, 3, -.5f});
	auto First = Original;
	First.Id = "framing-first";
	First.Parent() = Rig.Id;
	First.Local() = Multiply(Translation({-20, 2, 5}), Scale({.7f, 1.2f, -1}));
	auto Second = Original;
	Second.Id = "framing-second";
	Second.Parent() = Rig.Id;
	Second.Local() = Translation({15, -3, 2});
	const auto Handles = InScene.AddNodes({Rig, First, Second});
	const auto Local = SceneModelBounds(SceneModelTransfer(Original, Identity(), true));
	const auto A = TransformBounds(Local, Multiply(Rig.Local(), First.Local()));
	const auto B = TransformBounds(Local, Multiply(Rig.Local(), Second.Local()));
	const auto Expected = UnionBounds(A, B);
	const std::array Selected{Handles[1], Handles[2]};
	const std::array Reversed{Handles[2], Handles[1]};
	const std::array Overlapping{Handles[0], Handles[1], Handles[2], Handles[1]};
	const auto Snapshot = Serialize(InScene.Snapshot("FramingSnapshot.hasset"));
	const auto Revision = InScene.GetRevision();
	CheckBoundsEqual(SceneSelectionBounds(InScene, Selected), Expected);
	CheckBoundsEqual(SceneSelectionBounds(InScene, Reversed), Expected);
	CheckBoundsEqual(SceneSelectionBounds(InScene, Overlapping), Expected);
	CheckFramingFit(Expected);
	HYP_CHECK(InScene.GetRevision() == Revision);
	HYP_CHECK(Serialize(InScene.Snapshot("FramingSnapshot.hasset")) == Snapshot);
	CheckPrimitiveFraming(InScene, Handles[1]);
	HYP_CHECK(InScene.RemoveSubtree(Handles[0]));
	CheckFallbackFraming(InScene);
	CheckPreparingFraming(InScene);
	CheckInvalidFraming();
}
