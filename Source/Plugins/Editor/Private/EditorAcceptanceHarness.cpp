#include "EditorAcceptanceHarness.h"

namespace Hyperion
{
EEditorAssetAcceptancePhase FEditorAcceptanceHarness::AssetPhase() const
{
	constexpr unsigned NameAndSaveFirstStep = 13;
	constexpr unsigned NameAndSaveLastStep = 20;
	constexpr unsigned PropertyFirstStep = 100;
	constexpr unsigned PropertyLastStep = 172;
	constexpr unsigned WindowLifecycleFirstStep = 190;
	constexpr unsigned MainSceneCloseStep = 200;
	constexpr unsigned MainSceneClosedStep = 201;
	if (Scenario.ExerciseStep >= NameAndSaveFirstStep && Scenario.ExerciseStep <= NameAndSaveLastStep)
	{
		return EEditorAssetAcceptancePhase::NameAndSave;
	}
	if (Scenario.ExerciseStep >= PropertyFirstStep && Scenario.ExerciseStep <= PropertyLastStep)
	{
		return EEditorAssetAcceptancePhase::AssetProperties;
	}
	if (Scenario.ExerciseStep >= WindowLifecycleFirstStep && Scenario.ExerciseStep != MainSceneCloseStep &&
	    Scenario.ExerciseStep != MainSceneClosedStep)
	{
		return EEditorAssetAcceptancePhase::AssetWindowLifecycle;
	}
	return EEditorAssetAcceptancePhase::MainWindow;
}

EEditorAcceptanceWindow FEditorAcceptanceHarness::AssetInputWindow() const
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

void FEditorAcceptanceHarness::CollectInput(std::vector<FInputEvent>& InEvents, std::vector<FInputEvent>& InAssetEvents)
{
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
	if (!Editor.Options.ExerciseCapture.empty())
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

bool FEditorAcceptanceHarness::IsComplete() const
{
	return (Editor.Options.bExercise && Scenario.ExerciseStep == 21 && Editor.ReadyFrames > 8) ||
	       Scenario.bDocumentVerified || Scenario.bViewsVerified || Scenario.bGizmoVerified ||
	       Scenario.bPickingVerified || Scenario.bPlacementVerified || Scenario.bModelPlacementVerified ||
	       Scenario.bOutlinesVerified || Scenario.bMultiSelectionVerified || Scenario.bContentVerified ||
	       Scenario.bRenderControlsVerified || Scenario.bReparentVerified || Scenario.bClipboardVerified ||
	       Scenario.bLogVerified || Scenario.bFramingVerified || Scenario.bSelectionShortcutsVerified;
}

bool FEditorAcceptanceHarness::ShouldCapture() const
{
	const bool bExerciseComplete = IsComplete();
	return !Editor.Options.Capture.empty() &&
	       (bExerciseComplete ||
	        (Editor.Options.ExerciseCapture == "toggle" && Editor.bPreferencesDialog && Scenario.ExerciseStep == 2 &&
	         Scenario.ExerciseWait == 2) ||
	        (Editor.Options.ExerciseCapture == "capture" && Scenario.ExerciseStep == 1));
}

void FEditorAcceptanceHarness::CheckCompletion() const
{
	if (Editor.Options.bExerciseSelectionShortcuts && !Scenario.bSelectionShortcutsVerified)
	{
		throw std::runtime_error("Editor selection shortcut acceptance incomplete at step " +
		                         std::to_string(Scenario.ShortcutStep));
	}
	if (Editor.Options.bExerciseFraming && !Scenario.bFramingVerified)
	{
		throw std::runtime_error("Editor framing acceptance incomplete at step " +
		                         std::to_string(Scenario.FramingStep));
	}
	if (!Editor.Options.ExerciseModelPlacement.empty() && !Scenario.bModelPlacementVerified)
	{
		throw std::runtime_error("Model placement acceptance incomplete at case " +
		                         std::to_string(Scenario.ModelPlacementCase) + " step " +
		                         std::to_string(Scenario.ModelPlacementStep) + ": " + Editor.PlacementStatus);
	}
	if (Editor.Options.bExerciseClipboard && !Scenario.bClipboardVerified)
	{
		throw std::runtime_error("Editor clipboard acceptance did not complete at step " +
		                         std::to_string(Scenario.ClipboardExerciseStep));
	}
	if (!Editor.Options.ExerciseReparent.empty() && !Scenario.bReparentVerified)
	{
		throw std::runtime_error("Reparent acceptance did not complete");
	}
	if (!Editor.Options.ExerciseImport.empty() && !Scenario.bImportVerified)
	{
		throw std::runtime_error("Import GUI acceptance incomplete at step " + std::to_string(Scenario.ExerciseStep));
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
		throw std::runtime_error("Editor Log acceptance incomplete at step " + std::to_string(Scenario.ExerciseStep));
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

void FEditorAcceptanceHarness::CheckTimeout(double InElapsed) const
{
	if ((Editor.Options.bExercise || Editor.Options.bExerciseLog || Editor.Options.bExerciseGizmo ||
	     Editor.Options.bExercisePicking || Editor.Options.bExerciseMultiSelection ||
	     Editor.Options.bExerciseClipboard || Editor.Options.bExerciseFraming ||
	     Editor.Options.bExerciseSelectionShortcuts || !Editor.Options.ExerciseDocument.empty() ||
	     !Editor.Options.ExerciseViews.empty() || !Editor.Options.ExercisePlacement.empty() ||
	     !Editor.Options.ExerciseModelPlacement.empty() || !Editor.Options.ExerciseOutlines.empty() ||
	     !Editor.Options.ExerciseCapture.empty() || !Editor.Options.ExerciseContent.empty() ||
	     !Editor.Options.ExerciseAssets.empty() || !Editor.Options.ExerciseRenderControls.empty() ||
	     !Editor.Options.ExerciseImport.empty() || !Editor.Options.ExerciseReparent.empty()) &&
	    InElapsed > 90)
	{
		throw std::runtime_error("Editor interaction acceptance timed out; model placement case " +
		                         std::to_string(Scenario.ModelPlacementCase) + " step " +
		                         std::to_string(Scenario.ModelPlacementStep) + " (" + Editor.PlacementStatus +
		                         "); step " + std::to_string(Scenario.ExerciseStep) + ": " +
		                         Editor.AssetWorkspace->ActiveStatus());
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
	if (!Editor.Options.ExerciseAssets.empty() && (Scenario.ExerciseStep == 132 || Scenario.ExerciseStep == 133) &&
	    Editor.AssetWorkspace->HasPendingEdits() && !Scenario.bPendingAssetEditChecked)
	{
		CheckPendingAssetEdit();
	}
}
} // namespace Hyperion
