#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <array>
#include <cmath>

namespace Hyperion
{
namespace
{
constexpr std::array CloseNames{"CustomMaterial", "Texture", "Sky", "Radiance", "Material", "Model"};

void CheckWorkspace(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void MovePointer(std::vector<FInputEvent>& InEvents, FVec2 InPoint)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
}

void PointerButton(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void WorkspaceKey(std::vector<FInputEvent>& InEvents, EKey InKey, bool bInDown, unsigned InModifiers = 0)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.Modifiers = InModifiers;
	InEvents.push_back(Event);
}
} // namespace

bool FEditorAcceptanceHarness::ExerciseAssetTabClose(std::vector<FInputEvent>& InEvents, const std::string& InPath)
{
	auto Bounds = Editor.AssetWorkspace->ObservedBounds("tab/" + InPath);
	CheckWorkspace(Bounds.Z > Bounds.X, "Asset tab missing before close");
	Bounds.X = Bounds.Z - Editor.Gui->Scale(28);
	MovePointer(InEvents, {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f});
	if (Scenario.Asset.TabClose.Hover.ConsumeFrame())
	{
		return false;
	}
	if (!ExerciseClick(InEvents, Bounds, Scenario.Asset.TabClose.Click, EAcceptanceClickDelay::Immediate))
	{
		return false;
	}
	Scenario.Asset.TabClose.Hover.Restart();
	return true;
}

void FEditorAcceptanceHarness::ExerciseAssetWorkspaceInput(std::vector<FInputEvent>& InEvents)
{
	const auto Stage = DescribeAssetAcceptance(Scenario.Asset.Progress.GetState()).Stage;
	if ((Stage == EAssetAcceptanceStage::FloatingPanel))
	{
		ExerciseAssetPanelInput(InEvents);
		return;
	}
	if ((Stage == EAssetAcceptanceStage::DiscardTab))
	{
		ExerciseAssetDiscardInput(InEvents);
		return;
	}
	const auto Path = "/Game/" + std::string(CloseNames.at(Scenario.AssetExerciseIndex)) + ".hasset";
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::OpenCleanTab:
			Editor.AssetWorkspace->Open(Path);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitCleanTab);
			break;
		case EAssetState::AwaitCleanTab:
			if (Document && Document->Loaded().Path == Path && Editor.AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty(), "Clean close fixture is dirty");
				Scenario.Asset.Progress.TransitionTo(EAssetState::CloseCleanTab);
			}
			break;
		case EAssetState::CloseCleanTab:
			if (ExerciseAssetTabClose(InEvents, Path))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::VerifyCleanTabClose);
			}
			break;
		case EAssetState::VerifyCleanTabClose:
			CheckWorkspace(Editor.AssetWorkspace->ObservedBounds("tab/" + Path).Z == 0 && !Editor.Window->ShouldClose(),
			               "Closing clean asset did not remove only its tab");
			CheckWorkspace(Editor.IsDirty() && Editor.History.size() == 1, "Closing asset changed scene history");
			Log(ELogLevel::Info, "Clean asset tab close passed: " + Path);
			Scenario.Asset.Progress.TransitionTo(++Scenario.AssetExerciseIndex < CloseNames.size()
			                                         ? EAssetState::OpenCleanTab
			                                         : EAssetState::OpenDiscardMaterial);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetDiscardInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::OpenDiscardMaterial:
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitDiscardMaterial);
			break;
		case EAssetState::AwaitDiscardMaterial:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Scenario.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
				Editor.AssetWorkspace->RevealProperty("field/name");
				Scenario.Asset.Progress.TransitionTo(EAssetState::FocusDiscardName);
			}
			break;
		case EAssetState::FocusDiscardName:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::TypeDiscardName);
			}
			break;
		case EAssetState::TypeDiscardName:
			if (ExerciseTextInput(InEvents, Scenario.Asset.DiscardNameInput, "Discard this draft"))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::RequestDirtyTabClose);
			}
			break;
		case EAssetState::RequestDirtyTabClose:
			CheckWorkspace(Document && Document->IsDirty(), "Discard fixture was not edited");
			if (ExerciseAssetTabClose(InEvents, "/Game/Material.hasset"))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::DiscardDirtyTab);
			}
			break;
		case EAssetState::DiscardDirtyTab:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/discard"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::ReopenDiscardedTab);
			}
			break;
		case EAssetState::ReopenDiscardedTab:
			CheckWorkspace(Editor.AssetWorkspace->ObservedBounds("tab//Game/Material.hasset").Z == 0 &&
			                   !Editor.Window->ShouldClose(),
			               "Discard did not remove only its asset tab");
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::VerifyDiscardedTab);
			break;
		case EAssetState::VerifyDiscardedTab:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty() &&
				                   ReadValue<std::string>(Document->Get("name")) == Scenario.AssetExerciseOriginalName,
				               "Discard persisted an unsaved edit");
				Log(ELogLevel::Info, "Dirty asset discard and reopen passed");
				Editor.AssetWorkspace->Open("/Game/Texture.hasset");
				Editor.AssetWorkspace->Open("/Game/Sky.hasset");
				Scenario.Asset.Progress.TransitionTo(EAssetState::PrepareFloatingPanel);
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetPanelInput(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::PrepareFloatingPanel:
			if (const auto* Document = Editor.AssetWorkspace->ActiveDocument();
			    Document && Document->Loaded().Path == "/Game/Sky.hasset" && Editor.AssetWorkspace->IsPreviewReady())
			{
				// Reproduce the user's floating panel, retaining the rest of the workspace layout.
				auto Layout = Editor.Gui->SaveLayout();
				const auto Start = Layout.find("[Window][Place Object]");
				const auto End = Layout.find("\n\n", Start);
				CheckWorkspace(Start != std::string::npos && End != std::string::npos,
				               "Place Object layout was not saved");
				Layout.replace(Start, End - Start, "[Window][Place Object]\nPos=400,200\nSize=300,500\nCollapsed=0");
				Editor.Gui->LoadLayout(Layout);
				Editor.bFocusPlacement = true;
				Scenario.Asset.PanelDrag.TitleHover.Restart();
				Scenario.Asset.Progress.TransitionTo(EAssetState::PressPanelTitle);
			}
			break;
		case EAssetState::PressPanelTitle:
		{
			const auto Bounds = Scenario.InspectionBounds.at("placement/title");
			Scenario.AssetExercisePointer = {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f};
			MovePointer(InEvents, Scenario.AssetExercisePointer);
			if (Scenario.Asset.PanelDrag.TitleHover.Advance())
			{
				PointerButton(InEvents, Scenario.AssetExercisePointer, true);
				Scenario.Asset.Progress.TransitionTo(EAssetState::DragPanelTitle);
			}
			break;
		}
		case EAssetState::DragPanelTitle:
			// The baseline is sampled before the sixth held-frame move; release follows eight observations.
			if (Scenario.Asset.PanelDrag.Progress.Is(EAssetPanelDragPhase::AwaitBaseline))
			{
				if (Scenario.Asset.PanelDrag.BaselineObservation.Advance())
				{
					Scenario.AssetExercisePanelStart = Scenario.InspectionBounds.at("placement/title");
					Scenario.Asset.PanelDrag.Progress.TransitionTo(EAssetPanelDragPhase::ObserveHeldDrag);
				}
			}
			else if (Scenario.Asset.PanelDrag.HeldDragObservation.Advance())
			{
				const auto Bounds = Scenario.InspectionBounds.at("placement/title");
				CheckWorkspace(std::abs(Bounds.X - Scenario.AssetExercisePanelStart.X - 80) < 2 &&
				                   std::abs(Bounds.Y - Scenario.AssetExercisePanelStart.Y - 48) < 2,
				               "Place Object window drag was interrupted while an asset tab was active");
				PointerButton(InEvents, Scenario.AssetExercisePointer, false);
				Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitPanelRelease);
				break;
			}
			Scenario.AssetExercisePointer.X += 10;
			Scenario.AssetExercisePointer.Y += 6;
			MovePointer(InEvents, Scenario.AssetExercisePointer);
			break;
		case EAssetState::AwaitPanelRelease:
			// A docking request from release is applied at the next frame boundary.
			if (!Scenario.Asset.PanelDrag.ReleaseObservation.Advance())
			{
				break;
			}
			CheckWorkspace(!Editor.Gui->PointerState().bDown, "Panel drag mouse release was not consumed");
			Scenario.AssetExercisePanelStart = Scenario.InspectionBounds.at("placement/title");
			MovePointer(InEvents, {Scenario.AssetExercisePointer.X + 50, Scenario.AssetExercisePointer.Y + 50});
			Scenario.Asset.Progress.TransitionTo(EAssetState::VerifyPanelRelease);
			break;
		case EAssetState::VerifyPanelRelease:
		{
			const auto Bounds = Scenario.InspectionBounds.at("placement/title");
			CheckWorkspace(Bounds.X == Scenario.AssetExercisePanelStart.X &&
			                   Bounds.Y == Scenario.AssetExercisePanelStart.Y,
			               "Place Object window kept moving after mouse release");
			CheckWorkspace(!Editor.Placement.IsActive() && Editor.IsDirty() && Editor.History.size() == 1,
			               "Panel drag changed the scene");
			Log(ELogLevel::Info, "Place Object panel held-button drag and release passed with three asset tabs");
			Scenario.AssetExerciseIndex = 4;
			Editor.AssetWorkspace->RevealProperty("field/name");
			Scenario.Asset.Progress.TransitionTo(EAssetState::FocusWindowHistoryName);
			break;
		}
	}
}
} // namespace Hyperion
