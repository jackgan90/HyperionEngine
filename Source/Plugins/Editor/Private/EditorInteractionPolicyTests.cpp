#include "EditorInteractionPolicy.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Hyperion;

namespace
{
enum class ERoute
{
	Camera,
	Picking,
	Placement,
	Gizmo,
	Overlay,
	ReparentBegin,
	ReparentContinue,
	Document,
	AuxiliaryWindow,
	ScenePanels,
	CloseRequest,
	CloseIdle,
	Shortcut
};

struct FPolicyCase
{
	const char* Name;
	bool FEditorInteractionFacts::* Fact;
	bool bValue;
	std::vector<ERoute> Blocked;
};

FEditorInteractionFacts ReadyFacts()
{
	FEditorInteractionFacts Facts;
	Facts.bSceneReady = true;
	Facts.bSceneFocus = true;
	Facts.bCameraInitialized = true;
	Facts.bViewportVisible = true;
	Facts.bViewportFocused = true;
	Facts.bViewportHovered = true;
	Facts.bPointerPositionValid = true;
	return Facts;
}

bool Allows(const FEditorInteractionPolicy& InPolicy, ERoute InRoute)
{
	switch (InRoute)
	{
		case ERoute::Camera:
			return InPolicy.AllowsCameraNavigation();
		case ERoute::Picking:
			return InPolicy.AllowsViewportPicking();
		case ERoute::Placement:
			return InPolicy.AllowsPlacement();
		case ERoute::Gizmo:
			return InPolicy.AllowsGizmo();
		case ERoute::Overlay:
			return InPolicy.AllowsGizmoOverlay();
		case ERoute::ReparentBegin:
			return InPolicy.AllowsReparentBegin();
		case ERoute::ReparentContinue:
			return InPolicy.AllowsReparentContinue();
		case ERoute::Document:
			return !InPolicy.IsDocumentBusy();
		case ERoute::AuxiliaryWindow:
			return !InPolicy.BlocksAuxiliaryWindows();
		case ERoute::ScenePanels:
			return InPolicy.AllowsScenePanels();
		case ERoute::CloseRequest:
			return InPolicy.AllowsCloseRequest();
		case ERoute::CloseIdle:
			return !InPolicy.HasCloseInteraction();
		case ERoute::Shortcut:
			return InPolicy.Shortcuts().Allows(EEditorShortcut::SelectAll);
	}
	throw std::logic_error("Unknown test route");
}

void CheckCases(std::span<const FPolicyCase> InCases)
{
	constexpr std::array Routes{
	    ERoute::Camera,          ERoute::Picking,       ERoute::Placement,        ERoute::Gizmo,
	    ERoute::Overlay,         ERoute::ReparentBegin, ERoute::ReparentContinue, ERoute::Document,
	    ERoute::AuxiliaryWindow, ERoute::ScenePanels,   ERoute::CloseRequest,     ERoute::CloseIdle,
	    ERoute::Shortcut};
	for (const auto& Case : InCases)
	{
		auto Facts = ReadyFacts();
		Facts.*Case.Fact = Case.bValue;
		for (const auto Route : Routes)
		{
			const bool bExpected = std::find(Case.Blocked.begin(), Case.Blocked.end(), Route) == Case.Blocked.end();
			if (Allows(FEditorInteractionPolicy(Facts), Route) != bExpected)
			{
				throw std::runtime_error(std::string(Case.Name) + " route " + std::to_string(static_cast<int>(Route)));
			}
		}
		// A fresh snapshot after the state clears must immediately restore admission.
		Facts.*Case.Fact = ReadyFacts().*Case.Fact;
		for (const auto Route : Routes)
		{
			if (!Allows(FEditorInteractionPolicy(Facts), Route))
			{
				throw std::runtime_error(std::string(Case.Name) + " did not restore admission");
			}
		}
	}
}

void CheckDialogs()
{
	const std::vector SceneDialog{
	    ERoute::Camera,        ERoute::Picking,          ERoute::Placement, ERoute::Gizmo,           ERoute::Overlay,
	    ERoute::ReparentBegin, ERoute::ReparentContinue, ERoute::Document,  ERoute::AuxiliaryWindow, ERoute::Shortcut};
	auto OpenSave = SceneDialog;
	OpenSave.push_back(ERoute::CloseRequest);
	auto Root = OpenSave;
	Root.push_back(ERoute::ScenePanels);
	const std::array Cases{FPolicyCase{"open", &FEditorInteractionFacts::bOpenDialog, true, OpenSave},
	                       FPolicyCase{"save", &FEditorInteractionFacts::bSaveDialog, true, OpenSave},
	                       FPolicyCase{"decision", &FEditorInteractionFacts::bDecisionVisible, true, SceneDialog},
	                       FPolicyCase{"asset message", &FEditorInteractionFacts::bAssetMessage, true, SceneDialog},
	                       FPolicyCase{"pending root", &FEditorInteractionFacts::bPendingRoot, true, Root},
	                       FPolicyCase{"preferences",
	                                   &FEditorInteractionFacts::bPreferencesDialog,
	                                   true,
	                                   {ERoute::Camera, ERoute::Picking, ERoute::Placement, ERoute::ReparentBegin,
	                                    ERoute::ReparentContinue, ERoute::Document, ERoute::AuxiliaryWindow,
	                                    ERoute::CloseRequest, ERoute::Shortcut}},
	                       FPolicyCase{"save before close",
	                                   &FEditorInteractionFacts::bSavingClose,
	                                   true,
	                                   {ERoute::AuxiliaryWindow, ERoute::Shortcut}},
	                       FPolicyCase{"ordinary popup", &FEditorInteractionFacts::bPopup, true, {ERoute::Shortcut}},
	                       FPolicyCase{"benchmark",
	                                   &FEditorInteractionFacts::bBenchmark,
	                                   true,
	                                   {ERoute::Document, ERoute::ScenePanels, ERoute::Shortcut}},
	                       FPolicyCase{"finished",
	                                   &FEditorInteractionFacts::bFinished,
	                                   true,
	                                   {ERoute::Document, ERoute::CloseRequest, ERoute::Shortcut}}};
	CheckCases(Cases);
}

void CheckGestures()
{
	const std::array Cases{
	    FPolicyCase{"pending reparent",
	                &FEditorInteractionFacts::bReparentPending,
	                true,
	                {ERoute::Camera, ERoute::Document, ERoute::Shortcut}},
	    FPolicyCase{"dragging reparent", &FEditorInteractionFacts::bReparentDragging, true, {ERoute::Gizmo}},
	    FPolicyCase{"placement",
	                &FEditorInteractionFacts::bPlacement,
	                true,
	                {ERoute::Camera, ERoute::Picking, ERoute::Gizmo, ERoute::ReparentBegin, ERoute::Document,
	                 ERoute::CloseIdle, ERoute::Shortcut}},
	    FPolicyCase{"placement mouse",
	                &FEditorInteractionFacts::bPlacementUsedMouse,
	                true,
	                {ERoute::Camera, ERoute::Picking, ERoute::Gizmo, ERoute::Shortcut}},
	    FPolicyCase{"gizmo dragging",
	                &FEditorInteractionFacts::bGizmoDragging,
	                true,
	                {ERoute::Camera, ERoute::Picking, ERoute::Placement, ERoute::Shortcut}},
	    FPolicyCase{"gizmo mouse",
	                &FEditorInteractionFacts::bGizmoUsedMouse,
	                true,
	                {ERoute::Camera, ERoute::Picking, ERoute::Shortcut}},
	    FPolicyCase{"gizmo edit", &FEditorInteractionFacts::bGizmoEdit, true, {ERoute::Document, ERoute::CloseIdle}},
	    FPolicyCase{"inspector interaction",
	                &FEditorInteractionFacts::bInspectorInteraction,
	                true,
	                {ERoute::Document, ERoute::CloseIdle, ERoute::Shortcut}},
	    FPolicyCase{"pending inspector",
	                &FEditorInteractionFacts::bPendingInspectorEdit,
	                true,
	                {ERoute::Document, ERoute::CloseIdle}},
	    FPolicyCase{"inspector transaction", &FEditorInteractionFacts::bInspectorTransaction, true, {ERoute::Shortcut}},
	    FPolicyCase{"drag payload",
	                &FEditorInteractionFacts::bDragPayload,
	                true,
	                {ERoute::Picking, ERoute::ReparentBegin, ERoute::Document, ERoute::Shortcut}},
	    FPolicyCase{"editing text",
	                &FEditorInteractionFacts::bEditingText,
	                true,
	                {ERoute::Camera, ERoute::Picking, ERoute::Document}},
	    FPolicyCase{"text owned this frame", &FEditorInteractionFacts::bTextInputOwned, true, {ERoute::Shortcut}},
	    FPolicyCase{
	        "camera dragging", &FEditorInteractionFacts::bCameraDragging, true, {ERoute::Picking, ERoute::Shortcut}}};
	CheckCases(Cases);
}

void CheckViewportAndPointer()
{
	const std::array Cases{
	    FPolicyCase{"scene not ready",
	                &FEditorInteractionFacts::bSceneReady,
	                false,
	                {ERoute::ReparentBegin, ERoute::ReparentContinue, ERoute::Shortcut}},
	    FPolicyCase{"scene focus lost", &FEditorInteractionFacts::bSceneFocus, false, {ERoute::Shortcut}},
	    FPolicyCase{"preview camera",
	                &FEditorInteractionFacts::bPreviewCamera,
	                true,
	                {ERoute::Camera, ERoute::Gizmo, ERoute::Overlay}},
	    FPolicyCase{"camera uninitialized",
	                &FEditorInteractionFacts::bCameraInitialized,
	                false,
	                {ERoute::Camera, ERoute::Gizmo, ERoute::Overlay}},
	    FPolicyCase{"viewport hidden",
	                &FEditorInteractionFacts::bViewportVisible,
	                false,
	                {ERoute::Camera, ERoute::Picking, ERoute::Placement}},
	    FPolicyCase{"viewport focus lost",
	                &FEditorInteractionFacts::bViewportFocused,
	                false,
	                {ERoute::Camera, ERoute::Picking}},
	    FPolicyCase{"viewport not hovered", &FEditorInteractionFacts::bViewportHovered, false, {ERoute::Picking}},
	    FPolicyCase{"view options", &FEditorInteractionFacts::bViewOptionsOpen, true, {ERoute::Placement}},
	    FPolicyCase{
	        "right pointer",
	        &FEditorInteractionFacts::bPointerRightDown,
	        true,
	        {ERoute::Picking, ERoute::Placement, ERoute::ReparentBegin, ERoute::ReparentContinue, ERoute::Shortcut}},
	    FPolicyCase{"left pointer", &FEditorInteractionFacts::bPointerDown, true, {ERoute::Shortcut}},
	    FPolicyCase{"cancel",
	                &FEditorInteractionFacts::bPointerCancel,
	                true,
	                {ERoute::Picking, ERoute::Placement, ERoute::ReparentContinue, ERoute::Shortcut}},
	    FPolicyCase{"invalid pointer",
	                &FEditorInteractionFacts::bPointerPositionValid,
	                false,
	                {ERoute::Picking, ERoute::ReparentContinue}}};
	CheckCases(Cases);
}

void CheckShortcutDifferences()
{
	auto Facts = ReadyFacts();
	Facts.bSceneFocus = false;
	Facts.bDetailsFocus = true;
	Facts.bTextInputOwned = true;
	Facts.bInspectorTransaction = true;
	const auto Shortcuts = FEditorInteractionPolicy(Facts).Shortcuts();
	if (!Shortcuts.Allows(EEditorShortcut::History) || !Shortcuts.Allows(EEditorShortcut::Save) ||
	    Shortcuts.Allows(EEditorShortcut::Delete) || Shortcuts.Allows(EEditorShortcut::Clipboard))
	{
		throw std::runtime_error("Inspector history must retain command-specific ownership");
	}
	Facts.bTextInputOwned = false;
	Facts.bInspectorTransaction = false;
	const auto Details = FEditorInteractionPolicy(Facts).Shortcuts();
	if (!Details.Allows(EEditorShortcut::Clipboard) || !Details.Allows(EEditorShortcut::FrameSelection) ||
	    Details.Allows(EEditorShortcut::SelectAll))
	{
		throw std::runtime_error("Details command focus changed");
	}
}
} // namespace

int main()
{
	try
	{
		CheckDialogs();
		CheckGestures();
		CheckViewportAndPointer();
		CheckShortcutDifferences();
		std::cout << "Editor interaction policies preserve per-entry admission\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
