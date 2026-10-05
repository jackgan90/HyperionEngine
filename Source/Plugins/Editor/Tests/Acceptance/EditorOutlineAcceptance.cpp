#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
void FEditorAcceptanceHarness::PrepareOutlineExercise()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	FSceneModel Model;
	Model.Data = Editor.PlacementModels.at("Cube").Data;
	Model.Name = "Outline left";
	Model.World = Translation({-.35f, 0, 0});
	Scenario.OutlineExerciseObjects.push_back(Editor.Scene->Add(Model));
	Model.Name = "Outline right";
	Model.World = Translation({.35f, .15f, -.4f});
	Scenario.OutlineExerciseObjects.push_back(Editor.Scene->Add(Model));
	Model.Name = "Outline occluder";
	Model.World = Multiply(Translation({0, 0, 1.5f}), Scale({3, 3, .2f}));
	Scenario.OutlineExerciseWall = Editor.Scene->Add(Model);
	Editor.Scene->SetModelVisible(Scenario.OutlineExerciseWall, false);
	auto Light = MakeSceneDirectionalLightNode("outline-key");
	Light.Local() = SceneCameraTransform({3, 4, 5}, {});
	Light.DirectionalLight()->Intensity = 2;
	Light.DirectionalLight()->bCastShadows = false;
	auto Environment = MakeSceneEnvironmentLightNode("outline-fill");
	Environment.EnvironmentLight()->Intensity = .15f;
	Environment.EnvironmentLight()->bVisible = false;
	Environment.EnvironmentLight()->Priority = 10;
	Editor.Scene->AddNode(std::move(Light));
	Editor.Scene->AddNode(std::move(Environment));
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({2, 1.5f, 5}, {});
	Editor.Viewport.PreviewCamera.reset();
	Editor.Viewport.bViewportCameraInitialized = true;
	Editor.bShowLightMarkers = false;
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
	Editor.OutlineSettings = {};
	++Scenario.OutlineExerciseStep;
}

void FEditorAcceptanceHarness::ExerciseOutlines()
{
	if (!Editor.Viewport.bViewportVisible || !Editor.Scene->GetStatus().bReady || Scenario.bOutlinesVerified)
	{
		return;
	}
	if (Scenario.OutlineExerciseStep == 0)
	{
		const auto Found = Editor.PlacementModels.find("Cube");
		if (Found == Editor.PlacementModels.end() || !Found->second.Data)
		{
			return;
		}
		PrepareOutlineExercise();
	}
	if (Scenario.OutlineExerciseWait++ == 0)
	{
		Editor.OutlineSettings.Overlap =
		    Scenario.OutlineExerciseStep == 2 ? EOutlineOverlapMode::PerObject : EOutlineOverlapMode::Union;
		Editor.OutlineSettings.bSupersample = Scenario.OutlineExerciseStep == 5;
		Editor.Scene->SetModelVisible(Scenario.OutlineExerciseWall, Scenario.OutlineExerciseStep >= 4);
		if (Scenario.OutlineExerciseStep == 6)
		{
			Scenario.OutlineExerciseObjects.clear();
			Editor.SelectObject(std::nullopt);
		}
		return;
	}
	const auto& Stats = Editor.RenderStats.SelectionOutline;
	if (Scenario.OutlineExerciseWait < 4 || Stats.PendingItems || (Scenario.OutlineExerciseStep < 6 && !Stats.Items))
	{
		return;
	}
	const auto ExpectedPasses = Scenario.OutlineExerciseStep == 6 ? 0u : Scenario.OutlineExerciseStep == 2 ? 2u : 1u;
	if (Stats.MaskPasses != ExpectedPasses || Editor.IsDirty() || !Editor.History.empty())
	{
		throw std::runtime_error("Outline modes changed document history or submitted incorrect mask passes");
	}
	const std::array Names{"Union.png", "PerObject.png", "UnionAgain.png", "Occluded.png", "Smooth.png", "Cleared.png"};
	Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / Names.at(Scenario.OutlineExerciseStep - 1);
	Scenario.bOutlinesVerified = Scenario.OutlineExerciseStep == 6;
	++Scenario.OutlineExerciseStep;
	Scenario.OutlineExerciseWait = 0;
}
} // namespace Hyperion
