#include "EditorAcceptanceHarness.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"

namespace Hyperion
{
namespace
{
void RequireShortcut(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Selection shortcut acceptance: " + InMessage);
	}
}

FVec2 ShortcutPoint(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) / 2, (InBounds.Y + InBounds.W) / 2};
}

void ShortcutKey(std::vector<FInputEvent>& InEvents, EKey InKey, unsigned InModifiers = InputModifiers::None,
                 bool bInDown = true, bool bInRepeat = false)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InModifiers;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	InEvents.push_back(Event);
}

void ShortcutMouse(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown,
                   unsigned InButton = InputButtons::Left)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.Button = InButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::PrepareSelectionShortcuts()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		FSceneNode Node;
		Node.Name = std::string("Shortcut ") + char('A' + Index);
		if (Index == 0 || Index == 2)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = Editor.PlacementModels.at("Cube").Asset;
			Node.Local() = Translation({Index ? 2.f : -2.f, 0, 0});
		}
		if (Index == 2)
		{
			Node.Parent() = Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id;
		}
		Scenario.ShortcutObjects.push_back(Editor.Scene->AddNode(std::move(Node)));
	}
	FSceneNode Parent;
	Parent.Name = "Hidden parent";
	Parent.bEnabled = false;
	const auto ParentHandle = Editor.Scene->AddNode(std::move(Parent));
	for (unsigned Index = 0; Index < 130; ++Index)
	{
		FSceneNode Node;
		Node.Name = "Hidden child " + std::to_string(Index);
		Node.Parent() = Editor.Scene->FindNode(ParentHandle)->Id;
		Editor.Scene->AddNode(std::move(Node));
	}
	Editor.Filter = "Shortcut";
	Editor.bShowLightMarkers = false;
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 12}, {});
	Editor.Viewport.bViewportCameraInitialized = true;
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
	Scenario.ShortcutRevision = Editor.Scene->GetRevision();
	Editor.Gui->FocusWindow("Outliner");
}

void FEditorAcceptanceHarness::CheckShortcutSelection(std::initializer_list<unsigned> InIndices)
{
	RequireShortcut(Editor.Selection.All().size() == InIndices.size(),
	                "selection size at step " + Scenario.Shortcut.Progress.Name() + ": expected " +
	                    std::to_string(InIndices.size()) + ", got " + std::to_string(Editor.Selection.All().size()));
	for (const auto Index : InIndices)
	{
		RequireShortcut(Editor.Selection.Contains(Scenario.ShortcutObjects[Index]), "missing range member");
	}
	RequireShortcut(Editor.Selection.Primary() == Scenario.ShortcutObjects[*(InIndices.end() - 1)],
	                "range endpoint is not primary");
	RequireShortcut(Editor.Scene->GetRevision() == Scenario.ShortcutRevision && !Editor.IsDirty() &&
	                    Editor.History.empty(),
	                "selection mutated authored state");
}

void FEditorAcceptanceHarness::ExerciseSelectionRanges(std::vector<FInputEvent>& InEvents)
{
	const unsigned Case = DescribeShortcutRangeContext(Scenario.Shortcut.Progress.GetState()).CaseIndex;
	const auto Phase = DescribeShortcutRangeContext(Scenario.Shortcut.Progress.GetState()).Action;
	const std::array<unsigned, 7> Targets{0, 3, 2, 3, 1, 0, 0};
	const std::array<unsigned, 7> Modifiers{0, 2, 2, 1, 2, 3, 2};
	const auto Bounds =
	    Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[Targets[Case]])->Id);
	if ((Phase == EShortcutRangeAction::PressRow || Phase == EShortcutRangeAction::ReleaseRow))
	{
		ShortcutKey(InEvents, EKey::None, Phase == EShortcutRangeAction::PressRow ? Modifiers[Case] : 0);
		ShortcutMouse(InEvents, ShortcutPoint(Bounds), Phase == EShortcutRangeAction::PressRow);
	}
	else
	{
		switch (Case)
		{
			case 0:
				CheckShortcutSelection({0});
				break;
			case 1:
				CheckShortcutSelection({0, 1, 2, 3});
				break;
			case 2:
				CheckShortcutSelection({0, 1, 2});
				break;
			case 3:
				CheckShortcutSelection({0, 1, 2, 3});
				break;
			case 4:
				CheckShortcutSelection({2, 3, 1});
				break;
			case 5:
				CheckShortcutSelection({1, 2, 3, 0});
				Editor.Filter = "Shortcut A";
				break;
			case 6:
				CheckShortcutSelection({0});
				break;
		}
	}

	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::PlainPressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PlainReleaseRow);
			break;
		case EShortcutState::PlainReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PlainVerifyRange);
			break;
		case EShortcutState::PlainVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendForwardPressRow);
			break;
		case EShortcutState::ExtendForwardPressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendForwardReleaseRow);
			break;
		case EShortcutState::ExtendForwardReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendForwardVerifyRange);
			break;
		case EShortcutState::ExtendForwardVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendBackwardPressRow);
			break;
		case EShortcutState::ExtendBackwardPressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendBackwardReleaseRow);
			break;
		case EShortcutState::ExtendBackwardReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ExtendBackwardVerifyRange);
			break;
		case EShortcutState::ExtendBackwardVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddPressRow);
			break;
		case EShortcutState::AddPressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddReleaseRow);
			break;
		case EShortcutState::AddReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddVerifyRange);
			break;
		case EShortcutState::AddVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReplaceRangePressRow);
			break;
		case EShortcutState::ReplaceRangePressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReplaceRangeReleaseRow);
			break;
		case EShortcutState::ReplaceRangeReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReplaceRangeVerifyRange);
			break;
		case EShortcutState::ReplaceRangeVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddRangePressRow);
			break;
		case EShortcutState::AddRangePressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddRangeReleaseRow);
			break;
		case EShortcutState::AddRangeReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AddRangeVerifyRange);
			break;
		case EShortcutState::AddRangeVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::FilteredRangePressRow);
			break;
		case EShortcutState::FilteredRangePressRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::FilteredRangeReleaseRow);
			break;
		case EShortcutState::FilteredRangeReleaseRow:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::FilteredRangeVerifyRange);
			break;
		case EShortcutState::FilteredRangeVerifyRange:
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::FocusOutliner);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionKeys(std::vector<FInputEvent>& InEvents)
{
	// Stay inside the model face and clear of the primary object's gizmo handles.
	const auto Point =
	    ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds, {2.35f, -.35f, 0});
	RequireShortcut(bool(Point), "viewport point unavailable");
	const FVec2 Hit{Point->X, Point->Y};
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::FocusOutliner:
			Editor.Filter = "Shortcut";
			Editor.Gui->FocusWindow("Outliner");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::SelectAllOutliner);
			break;
		case EShortcutState::SelectAllOutliner:
			ShortcutKey(InEvents, EKey::A, 1);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyOutlinerSelectAll);
			break;
		case EShortcutState::VerifyOutlinerSelectAll:
		case EShortcutState::VerifyViewportSelectAll:
		{
			RequireShortcut(Editor.Selection.All().size() == Editor.Scene->GetNodes().size() &&
			                    Editor.Selection.All().size() > 128,
			                "Ctrl+A omitted hidden or search-excluded nodes");
			RequireShortcut(
			    Editor.Selection.Primary() ==
			        Scenario
			            .ShortcutObjects[Scenario.Shortcut.Progress.Is(EShortcutState::VerifyOutlinerSelectAll) ? 0
			                                                                                                    : 2],
			    "Ctrl+A moved primary");
			const auto Before = Editor.Selection;
			const auto Summary =
			    SelectAllSceneNodes(Editor.SceneDocument, {Editor.SceneDocument.Id(), Scenario.ShortcutRevision});
			RequireShortcut(Editor.Selection == Before && Summary.Count == Before.All().size(),
			                "GUI/domain all-selection mismatch");
			Editor.SelectObject(
			    Scenario
			        .ShortcutObjects[Scenario.Shortcut.Progress.Is(EShortcutState::VerifyOutlinerSelectAll) ? 2 : 0]);
			Editor.Gui->FocusWindow("Viewport");
			Scenario.Shortcut.Progress.TransitionTo(
			    Scenario.Shortcut.Progress.Is(EShortcutState::VerifyOutlinerSelectAll)
			        ? EShortcutState::SelectAllViewport
			        : EShortcutState::PressShiftViewportHit);
			break;
		}
		case EShortcutState::SelectAllViewport:
			ShortcutKey(InEvents, EKey::A, 1);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyViewportSelectAll);
			break;
		case EShortcutState::PressShiftViewportHit:
		case EShortcutState::PressShiftViewportToggle:
		case EShortcutState::PressShiftViewportMiss:
			if (Scenario.Shortcut.Progress.Is(EShortcutState::PressShiftViewportToggle))
			{
				CheckShortcutSelection({0, 2});
			}
			if (Scenario.Shortcut.Progress.Is(EShortcutState::PressShiftViewportMiss))
			{
				CheckShortcutSelection({0});
			}
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(InEvents, Scenario.Shortcut.Progress.Is(EShortcutState::PressShiftViewportMiss) ? Miss : Hit,
			              true);
			Scenario.Shortcut.Progress.TransitionTo(
			    Scenario.Shortcut.Progress.Is(EShortcutState::PressShiftViewportHit)
			        ? EShortcutState::ReleaseShiftViewportHit
			    : Scenario.Shortcut.Progress.Is(EShortcutState::PressShiftViewportToggle)
			        ? EShortcutState::ReleaseShiftViewportToggle
			        : EShortcutState::ReleaseShiftViewportMiss);
			break;
		case EShortcutState::ReleaseShiftViewportHit:
		case EShortcutState::ReleaseShiftViewportToggle:
		case EShortcutState::ReleaseShiftViewportMiss:
			ShortcutMouse(InEvents,
			              Scenario.Shortcut.Progress.Is(EShortcutState::ReleaseShiftViewportMiss) ? Miss : Hit, false);
			Scenario.Shortcut.Progress.TransitionTo(
			    Scenario.Shortcut.Progress.Is(EShortcutState::ReleaseShiftViewportHit)
			        ? EShortcutState::PressShiftViewportToggle
			    : Scenario.Shortcut.Progress.Is(EShortcutState::ReleaseShiftViewportToggle)
			        ? EShortcutState::PressShiftViewportMiss
			        : EShortcutState::VerifyViewportRange);
			break;
		case EShortcutState::VerifyViewportRange:
			CheckShortcutSelection({0});
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PressSearch);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionGuards(std::vector<FInputEvent>& InEvents)
{
	const auto Search = ShortcutPoint(Scenario.InspectionBounds.at("clipboard/search"));
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::PressSearch:
			ShortcutMouse(InEvents, Search, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseSearch);
			break;
		case EShortcutState::ReleaseSearch:
			ShortcutMouse(InEvents, Search, false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptSearchCommands);
			break;
		case EShortcutState::AttemptSearchCommands:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifySearchGuard);
			break;
		case EShortcutState::VerifySearchGuard:
			CheckShortcutSelection({0});
			Editor.Gui->FinishEditing();
			Editor.Gui->FocusWindow("Details");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptDetailsCommands);
			break;
		case EShortcutState::AttemptDetailsCommands:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyDetailsGuard);
			break;
		case EShortcutState::VerifyDetailsGuard:
			CheckShortcutSelection({0});
			Editor.Gui->FocusWindow("Viewport");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptNavigationCommands);
			break;
		case EShortcutState::AttemptNavigationCommands:
			ShortcutMouse(InEvents, Miss, true, 1);
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptReleaseBatchCommands);
			break;
		case EShortcutState::AttemptReleaseBatchCommands:
			CheckShortcutSelection({0});
			// The key belongs to navigation even if its later release shares the same event batch.
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutMouse(InEvents, Miss, false, 1);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptRepeatedCommands);
			break;
		case EShortcutState::AttemptRepeatedCommands:
			CheckShortcutSelection({0});
			ShortcutKey(InEvents, EKey::A, 1, true, true);
			ShortcutKey(InEvents, EKey::Delete, InputModifiers::None, true, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyRepeatedGuard);
			break;
		case EShortcutState::VerifyRepeatedGuard:
			CheckShortcutSelection({0});
			Editor.bRequestOpen = Editor.bOpenDialog = true;
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::AttemptModalCommands);
			break;
		case EShortcutState::AttemptModalCommands:
			ShortcutKey(InEvents, EKey::A, 1);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyModalGuard);
			break;
		case EShortcutState::VerifyModalGuard:
			CheckShortcutSelection({0});
			Editor.bOpenDialog = false;
			Editor.Gui->ClosePopups();
			Editor.Gui->FocusWindow("Outliner");
			Editor.Filter.clear();
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PressTreeToggle);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionTree(std::vector<FInputEvent>& InEvents)
{
	const FVec2 Miss{Editor.Viewport.ViewportRegion.Bounds.X + 8, Editor.Viewport.ViewportRegion.Bounds.Y + 8};
	switch (Scenario.Shortcut.Progress.GetState())
	{
		case EShortcutState::PressTreeToggle:
		case EShortcutState::ReleaseTreeToggle:
		{
			const auto Bounds = Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id);
			ShortcutMouse(InEvents, {Bounds.X + Editor.Gui->Scale(8), (Bounds.Y + Bounds.W) / 2},
			              Scenario.Shortcut.Progress.Is(EShortcutState::PressTreeToggle));
			Scenario.Shortcut.Progress.TransitionTo(Scenario.Shortcut.Progress.Is(EShortcutState::PressTreeToggle)
			                                            ? EShortcutState::ReleaseTreeToggle
			                                            : EShortcutState::VerifyFoldAndPressRange);
			break;
		}
		case EShortcutState::VerifyFoldAndPressRange:
			RequireShortcut(std::find(Editor.OutlinerRows.begin(), Editor.OutlinerRows.end(),
			                          Scenario.ShortcutObjects[2]) == Editor.OutlinerRows.end(),
			                "tree child did not fold");
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseFoldedRange);
			break;
		case EShortcutState::ReleaseFoldedRange:
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyFoldedRangeAndPressDrag);
			break;
		case EShortcutState::VerifyFoldedRangeAndPressDrag:
			CheckShortcutSelection({0, 1, 3});
			ShortcutKey(InEvents, EKey::None, 2);
			ShortcutMouse(
			    InEvents,
			    ShortcutPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Scenario.ShortcutObjects[3])->Id)),
			    true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DragRangeOutside);
			break;
		case EShortcutState::DragRangeOutside:
			ShortcutMouse(InEvents, Miss, true);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::CancelRangeDragAndDelete);
			break;
		case EShortcutState::CancelRangeDragAndDelete:
			RequireShortcut(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bDragging,
			                "Shift gesture did not enter drag arbitration");
			ShortcutKey(InEvents, EKey::Escape);
			ShortcutKey(InEvents, EKey::Delete);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::ReleaseRangeDrag);
			break;
		case EShortcutState::ReleaseRangeDrag:
			ShortcutMouse(InEvents, Miss, false);
			ShortcutKey(InEvents, EKey::Escape, 0, false);
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::VerifyRangeDragCancellation);
			break;
		case EShortcutState::VerifyRangeDragCancellation:
			CheckShortcutSelection({0, 1, 3});
			RequireShortcut(!Editor.Reparent.GetGesture() &&
			                    Editor.Scene->FindNode(Scenario.ShortcutObjects[2])->Parent() ==
			                        Editor.Scene->FindNode(Scenario.ShortcutObjects[1])->Id,
			                "cancelled Shift gesture reparented nodes");
			Editor.SelectObject(Scenario.ShortcutObjects[0]);
			Editor.Gui->FocusWindow("Content Browser");
			Scenario.Shortcut.Progress.TransitionTo(EShortcutState::DeleteInContentBrowser);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseSelectionShortcuts(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportVisible || Scenario.bSelectionShortcutsVerified)
	{
		return;
	}
	const auto InputPhase = Scenario.Shortcut.Cadence.Advance();
	if (InputPhase != EAcceptanceCadencePhase::Execute)
	{
		if (InputPhase == EAcceptanceCadencePhase::ReleaseKeys)
		{
			for (const auto Key : {EKey::A, EKey::Delete, EKey::Z, EKey::Y, EKey::S, EKey::Tab, EKey::End})
			{
				ShortcutKey(InEvents, Key, InputModifiers::None, false);
			}
		}
		return;
	}
	if (Scenario.Shortcut.Progress.Is(EShortcutState::PrepareFixtures))
	{
		PrepareSelectionShortcuts();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
		Scenario.Shortcut.Progress.TransitionTo(EShortcutState::PlainPressRow);
	}
	else if ((IsShortcutRangeState(Scenario.Shortcut.Progress.GetState()) ||
	          Scenario.Shortcut.Progress.Is(EShortcutState::PrepareFixtures)))
	{
		ExerciseSelectionRanges(InEvents);
	}
	else if (Scenario.Shortcut.Progress.IsAny(
	             {EShortcutState::FocusOutliner, EShortcutState::SelectAllOutliner,
	              EShortcutState::VerifyOutlinerSelectAll, EShortcutState::SelectAllViewport,
	              EShortcutState::VerifyViewportSelectAll, EShortcutState::PressShiftViewportHit,
	              EShortcutState::ReleaseShiftViewportHit, EShortcutState::PressShiftViewportToggle,
	              EShortcutState::ReleaseShiftViewportToggle, EShortcutState::PressShiftViewportMiss,
	              EShortcutState::ReleaseShiftViewportMiss, EShortcutState::VerifyViewportRange}))
	{
		ExerciseSelectionKeys(InEvents);
	}
	else if (Scenario.Shortcut.Progress.IsAny(
	             {EShortcutState::PressSearch, EShortcutState::ReleaseSearch, EShortcutState::AttemptSearchCommands,
	              EShortcutState::VerifySearchGuard, EShortcutState::AttemptDetailsCommands,
	              EShortcutState::VerifyDetailsGuard, EShortcutState::AttemptNavigationCommands,
	              EShortcutState::AttemptReleaseBatchCommands, EShortcutState::AttemptRepeatedCommands,
	              EShortcutState::VerifyRepeatedGuard, EShortcutState::AttemptModalCommands,
	              EShortcutState::VerifyModalGuard}))
	{
		ExerciseSelectionGuards(InEvents);
	}
	else if (Scenario.Shortcut.Progress.IsAny(
	             {EShortcutState::PressTreeToggle, EShortcutState::ReleaseTreeToggle,
	              EShortcutState::VerifyFoldAndPressRange, EShortcutState::ReleaseFoldedRange,
	              EShortcutState::VerifyFoldedRangeAndPressDrag, EShortcutState::DragRangeOutside,
	              EShortcutState::CancelRangeDragAndDelete, EShortcutState::ReleaseRangeDrag,
	              EShortcutState::VerifyRangeDragCancellation}))
	{
		ExerciseSelectionTree(InEvents);
	}
	else if (!ExerciseShortcutRouting(InEvents))
	{
		return;
	}
}
} // namespace Hyperion
