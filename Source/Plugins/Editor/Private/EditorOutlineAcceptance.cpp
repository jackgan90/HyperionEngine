#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::PrepareOutlineExercise()
{
	for (const auto Handle : Scene->GetNodes(ESceneNodeKind::Model))
	{
		Scene->SetModelVisible(Handle, false);
	}
	FSceneModel Model;
	Model.Data = PlacementModels.at("Cube").Data;
	Model.Name = "Outline left";
	Model.World = Translation({-.35f, 0, 0});
	OutlineExerciseObjects.push_back(Scene->Add(Model));
	Model.Name = "Outline right";
	Model.World = Translation({.35f, .15f, -.4f});
	OutlineExerciseObjects.push_back(Scene->Add(Model));
	Model.Name = "Outline occluder";
	Model.World = Multiply(Translation({0, 0, 1.5f}), Scale({3, 3, .2f}));
	OutlineExerciseWall = Scene->Add(Model);
	Scene->SetModelVisible(OutlineExerciseWall, false);
	auto Light = MakeSceneDirectionalLightNode("outline-key");
	Light.Local() = SceneCameraTransform({3, 4, 5}, {});
	Light.DirectionalLight()->Intensity = 2;
	Light.DirectionalLight()->bCastShadows = false;
	auto Environment = MakeSceneEnvironmentLightNode("outline-fill");
	Environment.EnvironmentLight()->Intensity = .15f;
	Environment.EnvironmentLight()->bVisible = false;
	auto Settings = Scene->GetSettings();
	Settings.MainDirectionalLight = Scene->AddNode(std::move(Light));
	Settings.EnvironmentLight = Scene->AddNode(std::move(Environment));
	Scene->SetSettings(std::move(Settings));
	ViewCamera.World = SceneCameraTransform({2, 1.5f, 5}, {});
	PreviewCamera.reset();
	bViewportCameraInitialized = true;
	bShowLightMarkers = false;
	ResetDocument();
	SelectObject(std::nullopt);
	OutlineSettings = {};
	++OutlineExerciseStep;
}

void FEditorPlugin::ExerciseOutlines()
{
	if (!bViewportVisible || !Scene->GetStatus().bReady || bOutlinesVerified)
	{
		return;
	}
	if (OutlineExerciseStep == 0)
	{
		const auto Found = PlacementModels.find("Cube");
		if (Found == PlacementModels.end() || !Found->second.Data)
		{
			return;
		}
		PrepareOutlineExercise();
	}
	if (OutlineExerciseWait++ == 0)
	{
		OutlineSettings.Overlap =
		    OutlineExerciseStep == 2 ? EOutlineOverlapMode::PerObject : EOutlineOverlapMode::Union;
		OutlineSettings.bSupersample = OutlineExerciseStep == 5;
		Scene->SetModelVisible(OutlineExerciseWall, OutlineExerciseStep >= 4);
		if (OutlineExerciseStep == 6)
		{
			OutlineExerciseObjects.clear();
			SelectObject(std::nullopt);
		}
		return;
	}
	const auto& Stats = RenderStats.SelectionOutline;
	if (OutlineExerciseWait < 4 || Stats.PendingItems || (OutlineExerciseStep < 6 && !Stats.Items))
	{
		return;
	}
	const auto ExpectedPasses = OutlineExerciseStep == 6 ? 0u : OutlineExerciseStep == 2 ? 2u : 1u;
	if (Stats.MaskPasses != ExpectedPasses || IsDirty() || !History.empty())
	{
		throw std::runtime_error("Outline modes changed document history or submitted incorrect mask passes");
	}
	const std::array Names{"Union.png", "PerObject.png", "UnionAgain.png", "Occluded.png", "Smooth.png", "Cleared.png"};
	OutlineCapture = Options.ExerciseOutlines / Names.at(OutlineExerciseStep - 1);
	bOutlinesVerified = OutlineExerciseStep == 6;
	++OutlineExerciseStep;
	OutlineExerciseWait = 0;
}
} // namespace Hyperion
