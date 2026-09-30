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
	Acceptance.OutlineExerciseObjects.push_back(Scene->Add(Model));
	Model.Name = "Outline right";
	Model.World = Translation({.35f, .15f, -.4f});
	Acceptance.OutlineExerciseObjects.push_back(Scene->Add(Model));
	Model.Name = "Outline occluder";
	Model.World = Multiply(Translation({0, 0, 1.5f}), Scale({3, 3, .2f}));
	Acceptance.OutlineExerciseWall = Scene->Add(Model);
	Scene->SetModelVisible(Acceptance.OutlineExerciseWall, false);
	auto Light = MakeSceneDirectionalLightNode("outline-key");
	Light.Local() = SceneCameraTransform({3, 4, 5}, {});
	Light.DirectionalLight()->Intensity = 2;
	Light.DirectionalLight()->bCastShadows = false;
	auto Environment = MakeSceneEnvironmentLightNode("outline-fill");
	Environment.EnvironmentLight()->Intensity = .15f;
	Environment.EnvironmentLight()->bVisible = false;
	Environment.EnvironmentLight()->Priority = 10;
	Scene->AddNode(std::move(Light));
	Scene->AddNode(std::move(Environment));
	Viewport.ViewCamera.World = SceneCameraTransform({2, 1.5f, 5}, {});
	Viewport.PreviewCamera.reset();
	Viewport.bViewportCameraInitialized = true;
	bShowLightMarkers = false;
	ResetDocument();
	SelectObject(std::nullopt);
	OutlineSettings = {};
	++Acceptance.OutlineExerciseStep;
}

void FEditorPlugin::ExerciseOutlines()
{
	if (!Viewport.bViewportVisible || !Scene->GetStatus().bReady || Acceptance.bOutlinesVerified)
	{
		return;
	}
	if (Acceptance.OutlineExerciseStep == 0)
	{
		const auto Found = PlacementModels.find("Cube");
		if (Found == PlacementModels.end() || !Found->second.Data)
		{
			return;
		}
		PrepareOutlineExercise();
	}
	if (Acceptance.OutlineExerciseWait++ == 0)
	{
		OutlineSettings.Overlap =
		    Acceptance.OutlineExerciseStep == 2 ? EOutlineOverlapMode::PerObject : EOutlineOverlapMode::Union;
		OutlineSettings.bSupersample = Acceptance.OutlineExerciseStep == 5;
		Scene->SetModelVisible(Acceptance.OutlineExerciseWall, Acceptance.OutlineExerciseStep >= 4);
		if (Acceptance.OutlineExerciseStep == 6)
		{
			Acceptance.OutlineExerciseObjects.clear();
			SelectObject(std::nullopt);
		}
		return;
	}
	const auto& Stats = RenderStats.SelectionOutline;
	if (Acceptance.OutlineExerciseWait < 4 || Stats.PendingItems ||
	    (Acceptance.OutlineExerciseStep < 6 && !Stats.Items))
	{
		return;
	}
	const auto ExpectedPasses = Acceptance.OutlineExerciseStep == 6   ? 0u
	                            : Acceptance.OutlineExerciseStep == 2 ? 2u
	                                                                  : 1u;
	if (Stats.MaskPasses != ExpectedPasses || IsDirty() || !History.empty())
	{
		throw std::runtime_error("Outline modes changed document history or submitted incorrect mask passes");
	}
	const std::array Names{"Union.png", "PerObject.png", "UnionAgain.png", "Occluded.png", "Smooth.png", "Cleared.png"};
	Acceptance.OutlineCapture = Options.ExerciseOutlines / Names.at(Acceptance.OutlineExerciseStep - 1);
	Acceptance.bOutlinesVerified = Acceptance.OutlineExerciseStep == 6;
	++Acceptance.OutlineExerciseStep;
	Acceptance.OutlineExerciseWait = 0;
}
} // namespace Hyperion
