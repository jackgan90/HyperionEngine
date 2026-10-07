#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
EEditorAcceptanceWindow FEditorAcceptanceHarness::AssetInputWindow() const
{
	return DescribeAssetAcceptance(Scenario.Asset.Progress.GetState()).InputWindow;
}

void FEditorAcceptanceHarness::CollectInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents)
{
	Scenario.CapturePreference.bCaptureBeforeToggle = false;
	if (!Editor.Options.ExerciseSceneLifecycle.empty())
	{
		ExerciseSceneLifecycleInput(InEvents);
	}
	if (!Editor.Options.ExerciseImport.empty())
	{
		ExerciseImportInput(InEvents);
	}
	if (Editor.Options.bExercise)
	{
		ExerciseInput(InEvents);
	}
	if (!Editor.Options.ExerciseDocument.empty())
	{
		ExerciseDocumentInput(InEvents);
	}
	if (!Editor.Options.ExerciseViews.empty())
	{
		ExerciseViewInput(InEvents);
	}
	if (!Editor.Options.ExerciseRenderControls.empty())
	{
		ExerciseRenderControlsInput(InEvents);
	}
	if (Editor.Options.ExerciseCapture)
	{
		ExerciseCaptureInput(InEvents);
	}
	if (Editor.Options.bExerciseLog)
	{
		ExerciseLogInput(InEvents);
	}
	if (!Editor.Options.ExerciseAssets.empty())
	{
		ExerciseAssetInput(AssetInputWindow() == EEditorAcceptanceWindow::Asset ? InAssetEvents : InEvents);
	}
	if (Editor.Options.bExerciseClipboard)
	{
		ExerciseClipboard(InEvents);
	}
	if (Editor.Options.bExerciseFraming)
	{
		ExerciseFraming(InEvents);
	}
	if (Editor.Options.bExerciseSelectionShortcuts)
	{
		ExerciseSelectionShortcuts(InEvents);
	}
	if (Editor.Options.bExerciseGizmo)
	{
		ExerciseGizmoInput(InEvents);
	}
	if (Editor.Options.bExercisePicking)
	{
		ExercisePickingInput(InEvents);
	}
	if (Editor.Options.bExerciseMultiSelection)
	{
		ExerciseMultiSelection(InEvents);
	}
	if (!Editor.Options.ExerciseReparent.empty())
	{
		ExerciseReparent(InEvents);
	}
	if (!Editor.Options.ExercisePlacement.empty())
	{
		ExercisePlacementInput(InEvents);
	}
	if (!Editor.Options.ExerciseModelPlacement.empty())
	{
		ExerciseModelPlacement(InEvents);
	}
	if (!Editor.Options.ExerciseOutlines.empty())
	{
		ExerciseOutlines();
	}
	if (!Editor.Options.ExerciseContent.empty())
	{
		ExerciseContentInput(InEvents);
	}
}

bool FEditorAcceptanceHarness::IsInteractionComplete() const
{
	return Editor.Options.bExercise && Scenario.Interaction.Progress.Is(EInteractionState::AwaitReopenedScene) &&
	       Editor.ReadyFrames > 8;
}

bool FEditorAcceptanceHarness::IsComplete() const
{
	return IsInteractionComplete() || Scenario.SceneLifecycle.bVerified || Scenario.bDocumentVerified ||
	       Scenario.bViewsVerified || Scenario.bGizmoVerified || Scenario.bPickingVerified ||
	       Scenario.bPlacementVerified || Scenario.bModelPlacementVerified || Scenario.bOutlinesVerified ||
	       Scenario.bMultiSelectionVerified || Scenario.bContentVerified || Scenario.bRenderControlsVerified ||
	       Scenario.bReparentVerified || Scenario.bClipboardVerified || Scenario.bLogVerified ||
	       Scenario.bFramingVerified || Scenario.bSelectionShortcutsVerified;
}

bool FEditorAcceptanceHarness::ShouldCapture() const
{
	const bool bExerciseComplete = IsComplete();
	return !Editor.Options.Capture.empty() &&
	       (bExerciseComplete ||
	        (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Toggle && Editor.bPreferencesDialog &&
	         Scenario.CapturePreference.Progress.Is(ECapturePreferenceState::TogglePreference) &&
	         Scenario.CapturePreference.bCaptureBeforeToggle) ||
	        (Editor.Options.ExerciseCapture == EEditorCaptureExercise::Capture &&
	         Scenario.Capture.Progress.Is(ECaptureState::VerifyCapture)));
}

void FEditorAcceptanceHarness::CheckCompletion() const
{
	if (!Editor.Options.ExerciseSceneLifecycle.empty() && !Scenario.SceneLifecycle.bVerified)
	{
		throw std::runtime_error("Scene lifecycle acceptance incomplete at case " +
		                         std::to_string(Scenario.SceneLifecycle.CaseIndex) + " " +
		                         Scenario.SceneLifecycle.Progress.Name());
	}
	if (Editor.Options.bExerciseSelectionShortcuts && !Scenario.bSelectionShortcutsVerified)
	{
		throw std::runtime_error("Editor selection shortcut acceptance incomplete at step " +
		                         Scenario.Shortcut.Progress.Name());
	}
	if (Editor.Options.bExerciseFraming && !Scenario.bFramingVerified)
	{
		throw std::runtime_error("Editor framing acceptance incomplete at step " + Scenario.Framing.Progress.Name());
	}
	if (!Editor.Options.ExerciseModelPlacement.empty() && !Scenario.bModelPlacementVerified)
	{
		throw std::runtime_error("Model placement acceptance incomplete at case " +
		                         std::to_string(Scenario.ModelPlacementCase) + " step " +
		                         Scenario.ModelPlacement.Progress.Name() + ": " + Editor.PlacementStatus);
	}
	if (Editor.Options.bExerciseClipboard && !Scenario.bClipboardVerified)
	{
		throw std::runtime_error("Editor clipboard acceptance did not complete at step " +
		                         Scenario.Clipboard.Progress.Name());
	}
	if (!Editor.Options.ExerciseReparent.empty() && !Scenario.bReparentVerified)
	{
		throw std::runtime_error("Reparent acceptance did not complete");
	}
	if (!Editor.Options.ExerciseImport.empty() && !Scenario.bImportVerified)
	{
		throw std::runtime_error("Import GUI acceptance incomplete at step " + Scenario.Import.Progress.Name());
	}
	if (!Editor.Options.ExerciseRenderControls.empty() && !Scenario.bRenderControlsVerified)
	{
		throw std::runtime_error("Editor render controls acceptance did not complete");
	}
	if (!Editor.Options.ExerciseAssets.empty() && !Scenario.bAssetsVerified)
	{
		throw std::runtime_error("Asset editor acceptance incomplete");
	}
	if (!Editor.Options.ExerciseContent.empty() &&
	    (!Scenario.bContentVerified || Editor.IsDirty() || !Editor.Window->ShouldClose()))
	{
		throw std::runtime_error("Content transition acceptance did not complete a clean close");
	}
	if (Editor.Options.bExerciseMultiSelection && !Scenario.bMultiSelectionVerified)
	{
		throw std::runtime_error("Editor multi-selection acceptance did not complete");
	}
	if (!Editor.Options.ExerciseOutlines.empty() && !Scenario.bOutlinesVerified)
	{
		throw std::runtime_error("Editor outline acceptance did not complete");
	}
	if (!Editor.Options.ExercisePlacement.empty() && !Scenario.bPlacementVerified)
	{
		throw std::runtime_error("Editor placement acceptance did not complete");
	}
	if (Editor.Options.bExercisePicking && !Scenario.bPickingVerified)
	{
		throw std::runtime_error("Editor picking acceptance did not complete");
	}
	if (Editor.Options.bExerciseGizmo && !Scenario.bGizmoVerified)
	{
		throw std::runtime_error("Editor gizmo acceptance did not complete");
	}
	if (Editor.Options.bExerciseLog && !Scenario.bLogVerified)
	{
		throw std::runtime_error("Editor Log acceptance incomplete at step " + Scenario.Log.Progress.Name());
	}
	if (Editor.Options.bExercise &&
	    (!Editor.CurrentPath.ends_with("/Sponza.hasset") || Editor.OpenCount < 2 || !Editor.ReadyFrames ||
	     !Scenario.bMovementVerified || !Scenario.bMovementGateVerified || !Scenario.bRightReleaseVerified ||
	     !Scenario.bLookVerified || !Scenario.bDollyVerified || !Scenario.bSpeedVerified ||
	     !Scenario.bInputIsolationVerified))
	{
		throw std::runtime_error("Editor interaction acceptance did not complete");
	}
}

std::string FEditorAcceptanceHarness::ScenarioStatus() const
{
	std::string Status;
	const auto Append = [&](bool bInEnabled, std::string_view InName, const auto& InProgress)
	{
		if (bInEnabled)
		{
			Status += " " + std::string(InName) + "=" + InProgress.Name();
		}
	};
	Append(Editor.Options.bExercise, "interaction", Scenario.Interaction.Progress);
	Append(!Editor.Options.ExerciseSceneLifecycle.empty(), "scene-lifecycle", Scenario.SceneLifecycle.Progress);
	Append(!Editor.Options.ExerciseDocument.empty(), "document", Scenario.Document.Progress);
	Append(!Editor.Options.ExerciseDocument.empty(), "transform", Scenario.Transform.Progress);
	Append(!Editor.Options.ExerciseViews.empty(), "view", Scenario.View.Progress);
	Append(!Editor.Options.ExerciseAssets.empty(), "asset", Scenario.Asset.Progress);
	Append(!Editor.Options.ExerciseImport.empty(), "import", Scenario.Import.Progress);
	Append(!Editor.Options.ExerciseContent.empty(), "content", Scenario.Content.Progress);
	Append(!Editor.Options.ExerciseRenderControls.empty(), "render-controls", Scenario.RenderControls.Progress);
	Append(Editor.Options.bExerciseLog, "log", Scenario.Log.Progress);
	Append(Editor.Options.ExerciseCapture.has_value(), "capture", Scenario.Capture.Progress);
	Append(Editor.Options.ExerciseCapture.has_value(), "capture-preference", Scenario.CapturePreference.Progress);
	Append(Editor.Options.bExerciseClipboard, "clipboard", Scenario.Clipboard.Progress);
	Append(Editor.Options.bExerciseFraming, "framing", Scenario.Framing.Progress);
	Append(Editor.Options.bExerciseSelectionShortcuts, "shortcut", Scenario.Shortcut.Progress);
	Append(Editor.Options.bExerciseGizmo, "gizmo", Scenario.Gizmo);
	Append(Editor.Options.bExercisePicking, "picking-scene", Scenario.PickingScene);
	Append(Editor.Options.bExercisePicking, "picking", Scenario.Picking);
	Append(Editor.Options.bExerciseMultiSelection, "multi-selection", Scenario.MultiSelection.Progress);
	Append(!Editor.Options.ExerciseOutlines.empty(), "outline", Scenario.Outline.Progress);
	Append(!Editor.Options.ExerciseReparent.empty(), "reparent-drag", Scenario.ReparentDrag);
	Append(!Editor.Options.ExerciseReparent.empty(), "reparent-document", Scenario.ReparentDocument);
	Append(!Editor.Options.ExercisePlacement.empty(), "placement-drag", Scenario.PlacementDrag);
	Append(!Editor.Options.ExercisePlacement.empty(), "placement-cancel", Scenario.PlacementCancel);
	Append(!Editor.Options.ExercisePlacement.empty(), "placement-document", Scenario.PlacementDocument);
	Append(!Editor.Options.ExerciseModelPlacement.empty(), "model-placement", Scenario.ModelPlacement.Progress);
	Append(!Editor.Options.ExerciseModelPlacement.empty(), "model-placement-history", Scenario.ModelPlacementHistory);
	return Status;
}

void FEditorAcceptanceHarness::CheckTimeout(double InElapsed) const
{
	if ((Editor.Options.bExercise || Editor.Options.bExerciseLog || Editor.Options.bExerciseGizmo ||
	     Editor.Options.bExercisePicking || Editor.Options.bExerciseMultiSelection ||
	     Editor.Options.bExerciseClipboard || Editor.Options.bExerciseFraming ||
	     Editor.Options.bExerciseSelectionShortcuts || !Editor.Options.ExerciseDocument.empty() ||
	     !Editor.Options.ExerciseViews.empty() || !Editor.Options.ExercisePlacement.empty() ||
	     !Editor.Options.ExerciseModelPlacement.empty() || !Editor.Options.ExerciseOutlines.empty() ||
	     Editor.Options.ExerciseCapture.has_value() || !Editor.Options.ExerciseContent.empty() ||
	     !Editor.Options.ExerciseAssets.empty() || !Editor.Options.ExerciseRenderControls.empty() ||
	     !Editor.Options.ExerciseImport.empty() || !Editor.Options.ExerciseReparent.empty() ||
	     !Editor.Options.ExerciseSceneLifecycle.empty()) &&
	    InElapsed > 90)
	{
		throw std::runtime_error("Editor interaction acceptance timed out; model placement case " +
		                         std::to_string(Scenario.ModelPlacementCase) + " step " +
		                         Scenario.ModelPlacement.Progress.Name() + " (" + Editor.PlacementStatus +
		                         "); states:" + ScenarioStatus() + ": " + Editor.AssetWorkspace->ActiveStatus());
	}
}

void FEditorAcceptanceHarness::CheckGui(const FGuiDrawData& InData)
{
	if (!Editor.Options.ExercisePlacement.empty())
	{
		CheckPlacementMarkerDraws(InData);
	}
	if (Editor.Options.bExerciseMultiSelection)
	{
		CheckMultiSelectionMarkerDraws(InData);
	}
}

void FEditorAcceptanceHarness::CheckAssetWindowFrame()
{
	if (!Editor.Options.ExerciseAssets.empty() &&
	    Scenario.Asset.Progress.IsAny({EAssetState::SelectSrgbEncoding, EAssetState::SaveEncoding}) &&
	    Editor.AssetWorkspace->HasPendingEdits() && !Scenario.bPendingAssetEditChecked)
	{
		CheckPendingAssetEdit();
	}
}
} // namespace Hyperion
