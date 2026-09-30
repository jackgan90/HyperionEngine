#include "EditorApplication.h"

namespace Hyperion
{
EEditorAssetAcceptancePhase FEditorAcceptanceDriver::AssetPhase() const
{
	constexpr unsigned NameAndSaveFirstStep = 13;
	constexpr unsigned NameAndSaveLastStep = 20;
	constexpr unsigned PropertyFirstStep = 100;
	constexpr unsigned PropertyLastStep = 172;
	constexpr unsigned WindowLifecycleFirstStep = 190;
	constexpr unsigned MainSceneCloseStep = 200;
	constexpr unsigned MainSceneClosedStep = 201;
	if (ExerciseStep >= NameAndSaveFirstStep && ExerciseStep <= NameAndSaveLastStep)
	{
		return EEditorAssetAcceptancePhase::NameAndSave;
	}
	if (ExerciseStep >= PropertyFirstStep && ExerciseStep <= PropertyLastStep)
	{
		return EEditorAssetAcceptancePhase::AssetProperties;
	}
	if (ExerciseStep >= WindowLifecycleFirstStep && ExerciseStep != MainSceneCloseStep &&
	    ExerciseStep != MainSceneClosedStep)
	{
		return EEditorAssetAcceptancePhase::AssetWindowLifecycle;
	}
	return EEditorAssetAcceptancePhase::MainWindow;
}

EEditorAcceptanceWindow FEditorAcceptanceDriver::AssetInputWindow() const
{
	switch (AssetPhase())
	{
		case EEditorAssetAcceptancePhase::NameAndSave:
		case EEditorAssetAcceptancePhase::AssetProperties:
		case EEditorAssetAcceptancePhase::AssetWindowLifecycle:
			return EEditorAcceptanceWindow::Asset;
		case EEditorAssetAcceptancePhase::MainWindow:
			return EEditorAcceptanceWindow::Main;
	}
	throw std::logic_error("Unknown asset acceptance phase");
}

void FEditorAcceptanceDriver::CollectInput(FEditorPlugin& InEditor, std::vector<FInputEvent>& InEvents,
                                           std::vector<FInputEvent>& InAssetEvents)
{
	if (!InEditor.Options.ExerciseImport.empty())
	{
		InEditor.ExerciseImportInput(InEvents);
	}
	if (InEditor.Options.bExercise)
	{
		InEditor.ExerciseInput(InEvents);
	}
	if (!InEditor.Options.ExerciseDocument.empty())
	{
		InEditor.ExerciseDocumentInput(InEvents);
	}
	if (!InEditor.Options.ExerciseViews.empty())
	{
		InEditor.ExerciseViewInput(InEvents);
	}
	if (!InEditor.Options.ExerciseRenderControls.empty())
	{
		InEditor.ExerciseRenderControlsInput(InEvents);
	}
	if (!InEditor.Options.ExerciseCapture.empty())
	{
		InEditor.ExerciseCaptureInput(InEvents);
	}
	if (InEditor.Options.bExerciseLog)
	{
		InEditor.ExerciseLogInput(InEvents);
	}
	if (!InEditor.Options.ExerciseAssets.empty())
	{
		InEditor.ExerciseAssetInput(AssetInputWindow() == EEditorAcceptanceWindow::Asset ? InAssetEvents : InEvents);
	}
	if (InEditor.Options.bExerciseClipboard)
	{
		InEditor.ExerciseClipboard(InEvents);
	}
	if (InEditor.Options.bExerciseFraming)
	{
		InEditor.ExerciseFraming(InEvents);
	}
	if (InEditor.Options.bExerciseSelectionShortcuts)
	{
		InEditor.ExerciseSelectionShortcuts(InEvents);
	}
	if (InEditor.Options.bExerciseGizmo)
	{
		InEditor.ExerciseGizmoInput(InEvents);
	}
	if (InEditor.Options.bExercisePicking)
	{
		InEditor.ExercisePickingInput(InEvents);
	}
	if (InEditor.Options.bExerciseMultiSelection)
	{
		InEditor.ExerciseMultiSelection(InEvents);
	}
	if (!InEditor.Options.ExerciseReparent.empty())
	{
		InEditor.ExerciseReparent(InEvents);
	}
	if (!InEditor.Options.ExercisePlacement.empty())
	{
		InEditor.ExercisePlacementInput(InEvents);
	}
	if (!InEditor.Options.ExerciseModelPlacement.empty())
	{
		InEditor.ExerciseModelPlacement(InEvents);
	}
	if (!InEditor.Options.ExerciseOutlines.empty())
	{
		InEditor.ExerciseOutlines();
	}
	if (!InEditor.Options.ExerciseContent.empty())
	{
		InEditor.ExerciseContentInput(InEvents);
	}
}

bool FEditorAcceptanceDriver::IsComplete(const FEditorPlugin& InEditor) const
{
	return (InEditor.Options.bExercise && InEditor.Acceptance.ExerciseStep == 21 && InEditor.ReadyFrames > 8) ||
	       InEditor.Acceptance.bDocumentVerified || InEditor.Acceptance.bViewsVerified ||
	       InEditor.Acceptance.bGizmoVerified || InEditor.Acceptance.bPickingVerified ||
	       InEditor.Acceptance.bPlacementVerified || InEditor.Acceptance.bModelPlacementVerified ||
	       InEditor.Acceptance.bOutlinesVerified || InEditor.Acceptance.bMultiSelectionVerified ||
	       InEditor.Acceptance.bContentVerified || InEditor.Acceptance.bRenderControlsVerified ||
	       InEditor.Acceptance.bReparentVerified || InEditor.Acceptance.bClipboardVerified ||
	       InEditor.Acceptance.bLogVerified || InEditor.Acceptance.bFramingVerified ||
	       InEditor.Acceptance.bSelectionShortcutsVerified;
}

bool FEditorAcceptanceDriver::ShouldCapture(const FEditorPlugin& InEditor) const
{
	const bool bExerciseComplete = IsComplete(InEditor);
	return !InEditor.Options.Capture.empty() &&
	       (bExerciseComplete ||
	        (!InEditor.Options.bExercise && InEditor.Options.Frames &&
	         InEditor.FrameCount + 1 == InEditor.Options.Frames) ||
	        (!InEditor.Options.Benchmark.empty() && InEditor.bBenchmarkTiming &&
	         InEditor.BenchmarkSamples.size() + 1 == InEditor.Options.BenchmarkSamples) ||
	        (InEditor.Options.ExerciseCapture == "toggle" && InEditor.bPreferencesDialog &&
	         InEditor.Acceptance.ExerciseStep == 2 && InEditor.Acceptance.ExerciseWait == 2) ||
	        (InEditor.Options.ExerciseCapture == "capture" && InEditor.Acceptance.ExerciseStep == 1));
}

void FEditorAcceptanceDriver::CheckCompletion(FEditorPlugin& InEditor) const
{
	if (InEditor.Options.bExerciseSelectionShortcuts && !InEditor.Acceptance.bSelectionShortcutsVerified)
	{
		throw std::runtime_error("Editor selection shortcut acceptance incomplete at step " +
		                         std::to_string(InEditor.Acceptance.ShortcutStep));
	}
	if (InEditor.Options.bExerciseFraming && !InEditor.Acceptance.bFramingVerified)
	{
		throw std::runtime_error("Editor framing acceptance incomplete at step " +
		                         std::to_string(InEditor.Acceptance.FramingStep));
	}
	if (!InEditor.Options.ExerciseModelPlacement.empty() && !InEditor.Acceptance.bModelPlacementVerified)
	{
		throw std::runtime_error(
		    "Model placement acceptance incomplete at case " + std::to_string(InEditor.Acceptance.ModelPlacementCase) +
		    " step " + std::to_string(InEditor.Acceptance.ModelPlacementStep) + ": " + InEditor.PlacementStatus);
	}
	if (InEditor.Options.bExerciseClipboard && !InEditor.Acceptance.bClipboardVerified)
	{
		throw std::runtime_error("Editor clipboard acceptance did not complete at step " +
		                         std::to_string(InEditor.Acceptance.ClipboardExerciseStep));
	}
	if (!InEditor.Options.ExerciseReparent.empty() && !InEditor.Acceptance.bReparentVerified)
	{
		throw std::runtime_error("Reparent acceptance did not complete");
	}
	if (!InEditor.Options.ExerciseImport.empty() && !InEditor.Acceptance.bImportVerified)
	{
		throw std::runtime_error("Import GUI acceptance incomplete at step " +
		                         std::to_string(InEditor.Acceptance.ExerciseStep));
	}
	if (!InEditor.Options.ExerciseRenderControls.empty() && !InEditor.Acceptance.bRenderControlsVerified)
	{
		throw std::runtime_error("Editor render controls acceptance did not complete");
	}
	if (!InEditor.Options.ExerciseAssets.empty() && !InEditor.Acceptance.bAssetsVerified)
	{
		throw std::runtime_error("Asset editor acceptance incomplete");
	}
	if (!InEditor.Options.ExerciseContent.empty() &&
	    (!InEditor.Acceptance.bContentVerified || InEditor.IsDirty() || !InEditor.Window->ShouldClose()))
	{
		throw std::runtime_error("Content transition acceptance did not complete a clean close");
	}
	if (InEditor.Options.bExerciseMultiSelection && !InEditor.Acceptance.bMultiSelectionVerified)
	{
		throw std::runtime_error("Editor multi-selection acceptance did not complete");
	}
	if (!InEditor.Options.ExerciseOutlines.empty() && !InEditor.Acceptance.bOutlinesVerified)
	{
		throw std::runtime_error("Editor outline acceptance did not complete");
	}
	if (!InEditor.Options.ExercisePlacement.empty() && !InEditor.Acceptance.bPlacementVerified)
	{
		throw std::runtime_error("Editor placement acceptance did not complete");
	}
	if (InEditor.Options.bExercisePicking && !InEditor.Acceptance.bPickingVerified)
	{
		throw std::runtime_error("Editor picking acceptance did not complete");
	}
	if (InEditor.Options.bExerciseGizmo && !InEditor.Acceptance.bGizmoVerified)
	{
		throw std::runtime_error("Editor gizmo acceptance did not complete");
	}
	if (InEditor.Options.bExerciseLog && !InEditor.Acceptance.bLogVerified)
	{
		throw std::runtime_error("Editor Log acceptance incomplete at step " +
		                         std::to_string(InEditor.Acceptance.ExerciseStep));
	}
	if (InEditor.Options.bExercise &&
	    (!InEditor.CurrentPath.ends_with("/Sponza.hasset") || InEditor.OpenCount < 2 || !InEditor.ReadyFrames ||
	     !InEditor.Acceptance.bMovementVerified || !InEditor.Acceptance.bMovementGateVerified ||
	     !InEditor.Acceptance.bRightReleaseVerified || !InEditor.Acceptance.bLookVerified ||
	     !InEditor.Acceptance.bDollyVerified || !InEditor.Acceptance.bSpeedVerified ||
	     !InEditor.Acceptance.bInputIsolationVerified))
	{
		throw std::runtime_error("Editor interaction acceptance did not complete");
	}
}

void FEditorAcceptanceDriver::CheckTimeout(const FEditorPlugin& InEditor, double InElapsed) const
{
	if ((InEditor.Options.bExercise || InEditor.Options.bExerciseLog || InEditor.Options.bExerciseGizmo ||
	     InEditor.Options.bExercisePicking || InEditor.Options.bExerciseMultiSelection ||
	     InEditor.Options.bExerciseClipboard || InEditor.Options.bExerciseFraming ||
	     InEditor.Options.bExerciseSelectionShortcuts || !InEditor.Options.ExerciseDocument.empty() ||
	     !InEditor.Options.ExerciseViews.empty() || !InEditor.Options.ExercisePlacement.empty() ||
	     !InEditor.Options.ExerciseModelPlacement.empty() || !InEditor.Options.ExerciseOutlines.empty() ||
	     !InEditor.Options.ExerciseCapture.empty() || !InEditor.Options.ExerciseContent.empty() ||
	     !InEditor.Options.ExerciseAssets.empty() || !InEditor.Options.ExerciseRenderControls.empty() ||
	     !InEditor.Options.ExerciseImport.empty() || !InEditor.Options.ExerciseReparent.empty()) &&
	    InElapsed > 90)
	{
		throw std::runtime_error(
		    "Editor interaction acceptance timed out; model placement case " +
		    std::to_string(InEditor.Acceptance.ModelPlacementCase) + " step " +
		    std::to_string(InEditor.Acceptance.ModelPlacementStep) + " (" + InEditor.PlacementStatus + "); step " +
		    std::to_string(InEditor.Acceptance.ExerciseStep) + ": " + InEditor.AssetWorkspace->ActiveStatus());
	}
}

void FEditorAcceptanceDriver::CheckGui(FEditorPlugin& InEditor, const FGuiDrawData& InData) const
{
	if (!InEditor.Options.ExercisePlacement.empty())
	{
		InEditor.CheckPlacementMarkerDraws(InData);
	}
	if (InEditor.Options.bExerciseMultiSelection)
	{
		InEditor.CheckMultiSelectionMarkerDraws(InData);
	}
}

void FEditorAcceptanceDriver::CheckAssetWindow(FEditorPlugin& InEditor)
{
	if (!InEditor.Options.ExerciseAssets.empty() && (ExerciseStep == 132 || ExerciseStep == 133) &&
	    InEditor.AssetWorkspace->HasPendingEdits() && !bPendingAssetEditChecked)
	{
		InEditor.CheckPendingAssetEdit();
	}
}
} // namespace Hyperion
