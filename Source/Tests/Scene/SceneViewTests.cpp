#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <limits>

void CheckInitialViews()
{
	using namespace Hyperion;
	FSceneManifest Manifest;
	Manifest.InitialView =
	    FSceneCameraView{FSceneCamera{.9f, .2f, 4000, 30}, SceneCameraTransform({9, 8, 7}, {1, 2, 3})};
	const auto Archive = WriteValue(Manifest);
	const auto Copy = ReadValue<FSceneManifest>(Archive);
	HYP_CHECK(Copy.InitialView == Manifest.InitialView && Copy.Nodes.empty() && Copy.DefaultCamera.empty());
	FScene Scene;
	Scene.SetSettings(ResolveSceneSettings(Copy, Scene));
	HYP_CHECK(Scene.GetSettings().InitialView == Manifest.InitialView && Scene.GetNodes().empty());
	const auto Revision = Scene.GetRevision();
	Scene.SetSettings(Scene.GetSettings());
	HYP_CHECK(Scene.GetRevision() == Revision);
	for (unsigned Index = 0; Index < 3; ++Index)
	{
		auto Invalid = Scene.GetSettings();
		if (Index == 0)
		{
			Invalid.InitialView->Lens.Far = Invalid.InitialView->Lens.Near;
		}
		else if (Index == 1)
		{
			Invalid.InitialView->World.Values[12] = std::numeric_limits<float>::infinity();
		}
		else
		{
			Invalid.InitialView->World = {};
		}
		bool bRejected{};
		try
		{
			Scene.SetSettings(Invalid);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && Scene.GetRevision() == Revision &&
		          Scene.GetSettings().InitialView == Manifest.InitialView);
	}
	// A version-six file still owns exactly its authored camera; reading it performs no topology migration.
	FSceneManifest Legacy;
	Legacy.DefaultCamera = "camera";
	Legacy.Nodes.push_back(SceneEntryFromNode(MakeSceneCameraNode("camera", {4, 5, 6}, {})));
	auto Old = WriteValue(Legacy);
	auto& Envelope = std::get<FArchiveNode::FObject>(Old.Value);
	Envelope.at("version") = WriteValue(std::uint32_t{6});
	std::get<FArchiveNode::FObject>(Envelope.at("fields").Value).erase("initialView");
	const auto Restored = ReadValue<FSceneManifest>(Old);
	HYP_CHECK(!Restored.InitialView && Restored.DefaultCamera == "camera" && Restored.Nodes.size() == 1);
	HYP_CHECK(Restored.Nodes.front().Transform.Values == Legacy.Nodes.front().Transform.Values);
}
