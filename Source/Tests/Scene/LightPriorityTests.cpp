#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <limits>

using namespace Hyperion;

namespace
{
void CheckStablePriority()
{
	std::vector<FLightPriorityCandidate> Candidates{{{1, 0, 1}, "z", -3}, {{1, 1, 1}, "a", -3}, {{1, 2, 1}, "b", -9}};
	HYP_CHECK(!ResolveLightPriority({}).Handle);
	const auto Winner = Candidates[1].Handle;
	HYP_CHECK(ResolveLightPriority(Candidates).Handle == Winner && ResolveLightPriority(Candidates).bTied);
	std::reverse(Candidates.begin(), Candidates.end());
	HYP_CHECK(ResolveLightPriority(Candidates).Handle == Winner);
	Candidates.back().Priority = std::numeric_limits<std::int32_t>::max();
	HYP_CHECK(ResolveLightPriority(Candidates).Handle == Candidates.back().Handle &&
	          !ResolveLightPriority(Candidates).bTied);
	Candidates.back().Priority = std::numeric_limits<std::int32_t>::min();
	HYP_CHECK(ResolveLightPriority(Candidates).Handle == Winner && !ResolveLightPriority(Candidates).bTied);
}

void CheckDirectionalEligibility()
{
	FScene Scene;
	const auto A = Scene.AddNode(MakeSceneDirectionalLightNode("a"));
	const auto B = Scene.AddNode(MakeSceneDirectionalLightNode("b"));
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A && Scene.GetLightingSelection().Directional.bTied);
	auto Light = *Scene.FindNode(B)->DirectionalLight();
	Light.Priority = 20;
	Scene.SetDirectionalLight(B, Light);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == B);
	Light.bCastShadows = false;
	Scene.SetDirectionalLight(B, Light);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A);
	Light.bCastShadows = true;
	Light.Intensity = 0;
	Scene.SetDirectionalLight(B, Light);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A);
	Light.Intensity = 1;
	Light.Color = {};
	Scene.SetDirectionalLight(B, Light);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A);
	Light.Color = {1, 1, 1};
	Scene.SetDirectionalLight(B, Light);
	FSceneNode Parent;
	Parent.Id = "parent";
	Parent.bEnabled = false;
	const auto Group = Scene.AddNode(Parent);
	Scene.Reparent(B, Group, ESceneReparentMode::KeepLocal);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A);
	Scene.SetEnabled(Group, true);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == B);
	Scene.RemoveSubtree(B);
	HYP_CHECK(Scene.GetLightingSelection().Directional.Handle == A);
	Scene.SetEnabled(A, false);
	HYP_CHECK(!Scene.GetLightingSelection().Directional.Handle);
}

void CheckSkyPriorityPersistence()
{
	FScene Scene;
	const auto A = Scene.AddNode(MakeSceneEnvironmentLightNode("a"));
	const auto B = Scene.AddNode(MakeSceneEnvironmentLightNode("b"));
	auto Light = *Scene.FindNode(B)->EnvironmentLight();
	Light.Priority = 7;
	Light.Intensity = 0;
	Light.bVisible = false;
	Light.Source = ESceneEnvironmentSource::SkyAsset;
	Light.Sky.Path = "/Game/Missing.hasset";
	Scene.SetEnvironmentLight(B, Light);
	HYP_CHECK(Scene.GetLightingSelection().Environment.Handle == B);
	HYP_CHECK(ReadValue<FSceneEnvironmentLight>(WriteValue(Light)) == Light);
	FSceneManifest Manifest;
	for (const auto Handle : Scene.GetNodes())
	{
		Manifest.Nodes.push_back(SceneEntryFromNode(*Scene.FindNode(Handle)));
	}
	const auto Saved = WriteValue(Manifest);
	const auto Copy = ReadValue<FSceneManifest>(Saved);
	Scene.Clear();
	Scene.LoadNodes(NodesFromSceneManifest(Copy));
	HYP_CHECK(Scene.GetLightingSelection().Environment.Handle == Scene.FindHandle("b"));
	Scene.SetEnabled(Scene.FindHandle("b"), false);
	HYP_CHECK(Scene.GetLightingSelection().Environment.Handle == Scene.FindHandle("a"));
	HYP_CHECK(Scene.FindHandle("a") != A);
}
} // namespace

void CheckLightPriorities()
{
	CheckStablePriority();
	CheckDirectionalEligibility();
	CheckSkyPriorityPersistence();
}
