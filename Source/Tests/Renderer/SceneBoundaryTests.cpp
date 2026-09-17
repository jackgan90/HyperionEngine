#include "Hyperion/Renderer/LocalLights.h"
#include "Hyperion/Renderer/SceneCameraController.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Scene/Model.h"
#include "Hyperion/Scene/Scene.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>

using namespace Hyperion;

namespace
{
void CheckCombinedLights()
{
	FScene Scene;
	FSceneNode Node;
	Node.PointLight() = FScenePointLight{};
	Node.SpotLight() = FSceneSpotLight{};
	const auto Handle = Scene.AddNode(std::move(Node));
	FSceneMetadata Metadata;
	Metadata.Token = {Scene.GetIdentity(), 1, 1, Scene.GetRevision()};
	Metadata.LocalLightRevision = 1;
	Metadata.PointLights.emplace(Handle, FPublishedPointLight{*Scene.FindPointLight(Handle), {}, true});
	Metadata.SpotLights.emplace(Handle,
	                            FPublishedSpotLight{*Scene.FindSpotLight(Handle), ExtractScenePose(Identity()), true});
	FLocalLightIndex Index;
	for (const bool bHierarchy : {false, true})
	{
		FLocalLightStatistics Stats;
		const auto Lights = Index.Query(Metadata, nullptr, bHierarchy, Stats);
		HYP_CHECK(Lights.size() == 2 && Stats.VisiblePoints == 1 && Stats.VisibleSpots == 1);
	}
	Metadata.PointLights.clear();
	++Metadata.LocalLightRevision;
	FLocalLightStatistics Stats;
	const auto Lights = Index.Query(Metadata, nullptr, true, Stats);
	HYP_CHECK(Lights.size() == 1 && Lights[0].bSpot && Lights[0].Handle == Handle);
}

void CheckLargeCamera()
{
	FSceneCameraView Camera;
	Camera.World = Translation({1e8f, 1e8f, 1e8f});
	RotateSceneCamera(Camera, .1f, .1f);
	ValidateSceneCameraView(Camera);
	const auto Rotated = ExtractScenePose(Camera.World);
	HYP_CHECK(Rotated.Eye.X == 1e8f && std::abs(Rotated.Forward.X) > .05f);
	PanSceneCamera(Camera, {1, 0, 1});
	OrbitSceneCamera(Camera, .1f, .1f);
	DollySceneCamera(Camera, .85f);
	ValidateSceneCameraView(Camera);
	FSceneCameraController Controller;
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = EKey::W;
	Key.bDown = true;
	Controller.Input(Camera, std::span(&Key, 1), false, false);
	Controller.Advance(Camera, .016f);
	ValidateSceneCameraView(Camera);
}

void CheckModelAffine()
{
	FModelNode Node;
	Node.Local = Identity();
	Node.Local.Values[4] = .75f;
	ValidateNodeHierarchy(std::span(&Node, 1)); // Affine shear remains supported.
	Node.Local.Values[15] = .5f;
	bool bRejected = false;
	try
	{
		ValidateNodeHierarchy(std::span(&Node, 1));
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}
} // namespace

int main()
{
	try
	{
		CheckCombinedLights();
		CheckLargeCamera();
		CheckModelAffine();
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
