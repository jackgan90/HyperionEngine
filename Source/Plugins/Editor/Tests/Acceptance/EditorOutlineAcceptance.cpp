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
	Scenario.Outline.Progress.TransitionTo(EOutlineState::CaptureUnion);
}

void FEditorAcceptanceHarness::ExerciseOutlines()
{
	if (!Editor.Viewport.bViewportVisible || !Editor.Scene->GetStatus().bReady || Scenario.bOutlinesVerified)
	{
		return;
	}
	if (Scenario.Outline.Progress.Is(EOutlineState::PrepareOutline))
	{
		const auto Found = Editor.PlacementModels.find("Cube");
		if (Found == Editor.PlacementModels.end() || !Found->second.Data)
		{
			return;
		}
		PrepareOutlineExercise();
	}
	if (!Scenario.Outline.bCapturePrepared)
	{
		Scenario.Outline.bCapturePrepared = true;
		Editor.OutlineSettings.Overlap = Scenario.Outline.Progress.Is(EOutlineState::CapturePerObject)
		                                     ? EOutlineOverlapMode::PerObject
		                                     : EOutlineOverlapMode::Union;
		Editor.OutlineSettings.bSupersample = Scenario.Outline.Progress.Is(EOutlineState::CaptureSmooth);
		Editor.Scene->SetModelVisible(
		    Scenario.OutlineExerciseWall,
		    Scenario.Outline.Progress.IsAny({EOutlineState::CaptureOccluded, EOutlineState::CaptureSmooth,
		                                     EOutlineState::CaptureCleared, EOutlineState::Complete}));
		if (Scenario.Outline.Progress.Is(EOutlineState::CaptureCleared))
		{
			Scenario.OutlineExerciseObjects.clear();
			Editor.SelectObject(std::nullopt);
		}
		return;
	}
	const auto& Stats = Editor.RenderStats.SelectionOutline;
	if (!Scenario.Outline.CaptureObservation.Advance() || Stats.PendingItems ||
	    (Scenario.Outline.Progress.IsAny({EOutlineState::PrepareOutline, EOutlineState::CaptureUnion,
	                                      EOutlineState::CapturePerObject, EOutlineState::CaptureUnionAgain,
	                                      EOutlineState::CaptureOccluded, EOutlineState::CaptureSmooth}) &&
	     !Stats.Items))
	{
		return;
	}
	const auto ExpectedPasses = Scenario.Outline.Progress.Is(EOutlineState::CaptureCleared)     ? 0u
	                            : Scenario.Outline.Progress.Is(EOutlineState::CapturePerObject) ? 2u
	                                                                                            : 1u;
	if (Stats.MaskPasses != ExpectedPasses || Editor.IsDirty() || !Editor.History.empty())
	{
		throw std::runtime_error("Outline modes changed document history or submitted incorrect mask passes");
	}
	switch (Scenario.Outline.Progress.GetState())
	{
		case EOutlineState::CaptureUnion:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "Union.png";
			Scenario.Outline.Progress.TransitionTo(EOutlineState::CapturePerObject);
			break;
		case EOutlineState::CapturePerObject:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "PerObject.png";
			Scenario.Outline.Progress.TransitionTo(EOutlineState::CaptureUnionAgain);
			break;
		case EOutlineState::CaptureUnionAgain:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "UnionAgain.png";
			Scenario.Outline.Progress.TransitionTo(EOutlineState::CaptureOccluded);
			break;
		case EOutlineState::CaptureOccluded:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "Occluded.png";
			Scenario.Outline.Progress.TransitionTo(EOutlineState::CaptureSmooth);
			break;
		case EOutlineState::CaptureSmooth:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "Smooth.png";
			Scenario.Outline.Progress.TransitionTo(EOutlineState::CaptureCleared);
			break;
		case EOutlineState::CaptureCleared:
			Scenario.OutlineCapture = Editor.Options.ExerciseOutlines / "Cleared.png";
			Scenario.bOutlinesVerified = true;
			Scenario.Outline.Progress.TransitionTo(EOutlineState::Complete);
			break;
		default:
			throw std::logic_error("Invalid outline capture state");
	}
	Scenario.Outline.bCapturePrepared = false;
	Scenario.Outline.CaptureObservation.Restart();
}
} // namespace Hyperion
