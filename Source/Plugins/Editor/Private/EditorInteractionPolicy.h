#pragma once
#include "EditorShortcuts.h"

namespace Hyperion
{
// Main-thread observations. Capture at the admission point; never retain across GUI mutations.
struct FEditorInteractionFacts
{
	bool bOpenDialog{};
	bool bSaveDialog{};
	bool bDecisionVisible{};
	bool bAssetMessage{};
	bool bPendingRoot{};
	bool bPreferencesDialog{};
	bool bSavingClose{};
	bool bBenchmark{};
	bool bFinished{};
	bool bReparentPending{};
	bool bReparentDragging{};
	bool bPlacement{};
	bool bPlacementUsedMouse{};
	bool bGizmoDragging{};
	bool bGizmoUsedMouse{};
	bool bGizmoEdit{};
	bool bInspectorInteraction{};
	bool bPendingInspectorEdit{};
	bool bInspectorTransaction{};
	bool bDragPayload{};
	bool bEditingText{};
	bool bTextInputOwned{};
	bool bPopup{};
	bool bSceneFocus{};
	bool bDetailsFocus{};
	bool bSceneReady{};
	bool bPreviewCamera{};
	bool bCameraInitialized{};
	bool bViewportVisible{};
	bool bViewportFocused{};
	bool bViewportHovered{};
	bool bCameraDragging{};
	bool bViewOptionsOpen{};
	bool bPointerRightDown{};
	bool bPointerDown{};
	bool bPointerCancel{};
	bool bPointerPositionValid{};
};

class FEditorInteractionPolicy
{
public:
	explicit FEditorInteractionPolicy(FEditorInteractionFacts InFacts) : Facts(InFacts)
	{
	}

	bool AllowsCameraNavigation() const;
	bool AllowsViewportPicking() const;
	bool AllowsPlacement() const;
	bool AllowsGizmo() const;
	bool AllowsGizmoOverlay() const;
	bool AllowsReparentBegin() const;
	bool AllowsReparentContinue() const;
	bool IsDocumentBusy() const;
	bool BlocksAuxiliaryWindows() const;
	bool AllowsScenePanels() const;
	bool AllowsCloseRequest() const;
	bool HasCloseInteraction() const;
	FEditorShortcutInteraction Shortcuts() const;

private:
	bool HasSceneDialog() const;
	bool HasPointerDialog() const;
	FEditorInteractionFacts Facts;
};
} // namespace Hyperion
