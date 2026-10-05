#include "../../Private/EditorAcceptanceReport.h"
#include "../../Private/EditorHostOptions.h"
#include "EditorAcceptanceHarness.h"
#include "Hyperion/Assets/Assets.h"
#include <utility>

namespace Hyperion
{
std::unique_ptr<FEditorAcceptanceHarness> FEditorAcceptanceHarness::Create(FEditorPlugin& InEditor)
{
	return HasEditorAcceptanceRequest(InEditor.Options) ? std::make_unique<FEditorAcceptanceHarness>(InEditor)
	                                                    : nullptr;
}

FEditorAcceptanceHarness::FEditorAcceptanceHarness(FEditorPlugin& InEditor) : Editor(InEditor)
{
	const auto& Options = Editor.Options;
	Scenario.bInitialCapturePreference = Options.Preferences.bRenderDocCapture;
	Scenario.bInitialCaptureHudPreference = Options.Preferences.bRenderDocHud;
	if (!Options.ExerciseAssets.empty() || !Options.ExerciseReparent.empty() || !Options.ExerciseModelPlacement.empty())
	{
		// Hidden swapchains must not merge separate synthetic double-click sequences.
		ExecutionPolicy.GuiDelta = 1.f / 60;
	}
	if (Options.bExercise)
	{
		ExecutionPolicy.CameraDelta = 1.f / 60;
		ExecutionPolicy.bUseVsync = false;
		ExecutionPolicy.bUseFrameLimit = false;
	}
	ExecutionPolicy.bAllowAssetWindowWait = Options.ExerciseAssets.empty();
	ExecutionPolicy.bPersistAssetLayout = Options.ExerciseAssets.empty() && Options.ExerciseContent.empty();
}

const FEditorAcceptancePolicy& FEditorAcceptanceHarness::Policy() const
{
	return ExecutionPolicy;
}

void FEditorAcceptanceHarness::ObservePlacement(const std::shared_ptr<const FTransientGeometry>& InPreview) const
{
	if (!Editor.Options.ExerciseModelPlacement.empty() && Editor.PlacementPublication && InPreview)
	{
		const auto Primitives = Editor.Scene->ResolveRenderPrimitives(*Editor.PlacementPublication);
		if (Primitives.empty() || InPreview->ReplacedPrimitives != Primitives)
		{
			throw std::runtime_error("Model placement publication retained duplicate formal geometry");
		}
	}
}

std::span<const FSceneHandle> FEditorAcceptanceHarness::OutlineSelection(
    std::span<const FSceneHandle> InSelection) const
{
	return Scenario.OutlineExerciseObjects.empty() ? InSelection
	                                               : std::span<const FSceneHandle>(Scenario.OutlineExerciseObjects);
}

std::filesystem::path FEditorAcceptanceHarness::MainCapture() const
{
	return Scenario.OutlineCapture.empty() ? Scenario.PlacementCapture : Scenario.OutlineCapture;
}

std::filesystem::path FEditorAcceptanceHarness::TakeAssetCapture()
{
	return !Editor.Options.ExerciseAssets.empty() ? std::exchange(Scenario.PlacementCapture, {})
	                                              : std::filesystem::path{};
}

void FEditorAcceptanceHarness::CompleteCapture(const FImage& InImage, const std::filesystem::path& InPath)
{
	if (!InPath.empty())
	{
		std::filesystem::create_directories(InPath.parent_path());
		SaveImage(InPath, InImage);
		Scenario.PlacementCapture.clear();
		Scenario.OutlineCapture.clear();
	}
}

bool FEditorAcceptanceHarness::RevealContent(std::string_view InPath)
{
	if (Scenario.ContentRevealPath != InPath)
	{
		return false;
	}
	Scenario.ContentRevealPath.clear();
	return true;
}

void FEditorAcceptanceHarness::WriteReport(std::ostream& InStream) const
{
	WriteEditorAcceptanceReport(InStream, {.bContent = Scenario.bContentVerified,
	                                       .bDocument = Scenario.bDocumentVerified,
	                                       .bViews = Scenario.bViewsVerified,
	                                       .bRenderControls = Scenario.bRenderControlsVerified,
	                                       .bGizmo = Scenario.bGizmoVerified,
	                                       .bMultiSelection = Scenario.bMultiSelectionVerified,
	                                       .bSelectionShortcuts = Scenario.bSelectionShortcutsVerified,
	                                       .bClipboard = Scenario.bClipboardVerified,
	                                       .bFraming = Scenario.bFramingVerified,
	                                       .bReparent = Scenario.bReparentVerified,
	                                       .bPicking = Scenario.bPickingVerified,
	                                       .bOutlines = Scenario.bOutlinesVerified,
	                                       .bPlacement = Scenario.bPlacementVerified,
	                                       .bModelPlacement = Scenario.bModelPlacementVerified,
	                                       .Step = Scenario.ExerciseStep,
	                                       .bMovement = Scenario.bMovementVerified,
	                                       .bMovementGate = Scenario.bMovementGateVerified,
	                                       .bRightRelease = Scenario.bRightReleaseVerified,
	                                       .bLook = Scenario.bLookVerified,
	                                       .bDolly = Scenario.bDollyVerified,
	                                       .bSpeed = Scenario.bSpeedVerified,
	                                       .bInputIsolation = Scenario.bInputIsolationVerified});
}
} // namespace Hyperion
