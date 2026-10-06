#include "EditorAcceptanceHarness.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void RequireMulti(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Multi-selection acceptance: ") + InMessage);
	}
}

void Click(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.Button = InputButtons::Left;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void Modifier(std::vector<FInputEvent>& InEvents, unsigned InModifiers)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Modifiers = InModifiers;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::PrepareMultiSelection()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	FSceneModel Model;
	Model.Data = Editor.PlacementModels.at("Cube").Data;
	Model.Name = "Multi A";
	Model.World = Translation({-1, 0, 0});
	Scenario.MultiSelectionObjects.push_back(Editor.Scene->Add(Model));
	Model.Name = "Multi B";
	Model.World = Translation({1, 1, 0});
	Scenario.MultiSelectionObjects.push_back(Editor.Scene->Add(Model));
	Editor.Filter = "Multi";
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	Editor.Viewport.bViewportCameraInitialized = true;
	Editor.bShowLightMarkers = false;
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
	Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PressFirstRow);
}

void FEditorAcceptanceHarness::ExerciseMultiSelectionClicks(std::vector<FInputEvent>& InEvents)
{
	const auto A = Scenario.MultiSelectionObjects[0];
	const auto B = Scenario.MultiSelectionObjects[1];
	const auto Row = [&](FSceneHandle InHandle)
	{
		const auto Bounds = Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(InHandle)->Id);
		return FVec2{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	};
	if (Scenario.MultiSelection.Progress.IsAny(
	        {EMultiSelectionState::PrepareFixtures, EMultiSelectionState::PressFirstRow,
	         EMultiSelectionState::ReleaseFirstRow, EMultiSelectionState::PressSecondRow,
	         EMultiSelectionState::ReleaseSecondRow}))
	{
		if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PressSecondRow))
		{
			RequireMulti(Editor.Selection == A && Editor.Selection.All().size() == 1, "plain outliner click");
			Modifier(InEvents, 1);
		}
		if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::ReleaseSecondRow))
		{
			Modifier(InEvents, 0);
		}
		Click(InEvents,
		      Row(Scenario.MultiSelection.Progress.IsAny({EMultiSelectionState::PrepareFixtures,
		                                                  EMultiSelectionState::PressFirstRow,
		                                                  EMultiSelectionState::ReleaseFirstRow})
		              ? A
		              : B),
		      Scenario.MultiSelection.Progress.IsAny(
		          {EMultiSelectionState::PressFirstRow, EMultiSelectionState::PressSecondRow}));
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::VerifyOutlinerSelection))
	{
		RequireMulti(Editor.Selection == B && Editor.Selection.All().size() == 2 && !Editor.IsDirty() &&
		                 Editor.History.empty(),
		             "Ctrl-add in outliner");
	}
	else if (Scenario.MultiSelection.Progress.IsAny({EMultiSelectionState::PressViewportToggle,
	                                                 EMultiSelectionState::ReleaseViewportModifier,
	                                                 EMultiSelectionState::ReleaseViewportToggle}))
	{
		if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PressViewportToggle))
		{
			Modifier(InEvents, 1);
		}
		const auto Point =
		    ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds, {-1, 0, 0});
		RequireMulti(bool(Point), "viewport point");
		if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::ReleaseViewportModifier))
		{
			Modifier(InEvents, 0);
		}
		else
		{
			Click(InEvents, {Point->X, Point->Y},
			      Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PressViewportToggle));
		}
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::VerifyViewportToggle))
	{
		RequireMulti(Editor.Selection == B && Editor.Selection.All().size() == 1,
		             "viewport Ctrl-toggle did not retain press modifier");
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PressFirstRowAgain) ||
	         Scenario.MultiSelection.Progress.Is(EMultiSelectionState::ReleaseFirstRowAgain))
	{
		Modifier(InEvents, 1);
		Click(InEvents, Row(A), Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PressFirstRowAgain));
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::VerifyPrimary))
	{
		Modifier(InEvents, 0);
		RequireMulti(Editor.Selection == A && Editor.Selection.All().size() == 2, "primary after re-add");
	}
	else
	{
		if (Editor.RenderStats.SelectionOutline.PendingItems)
		{
			return;
		}
		RequireMulti(Editor.RenderStats.SelectionOutline.Items >= 2, "both selected models need outlines");
	}
	switch (Scenario.MultiSelection.Progress.GetState())
	{
		case EMultiSelectionState::PrepareFixtures:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PressFirstRow);
			break;
		case EMultiSelectionState::PressFirstRow:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::ReleaseFirstRow);
			break;
		case EMultiSelectionState::ReleaseFirstRow:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PressSecondRow);
			break;
		case EMultiSelectionState::PressSecondRow:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::ReleaseSecondRow);
			break;
		case EMultiSelectionState::ReleaseSecondRow:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::VerifyOutlinerSelection);
			break;
		case EMultiSelectionState::VerifyOutlinerSelection:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PressViewportToggle);
			break;
		case EMultiSelectionState::PressViewportToggle:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::ReleaseViewportModifier);
			break;
		case EMultiSelectionState::ReleaseViewportModifier:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::ReleaseViewportToggle);
			break;
		case EMultiSelectionState::ReleaseViewportToggle:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::VerifyViewportToggle);
			break;
		case EMultiSelectionState::VerifyViewportToggle:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PressFirstRowAgain);
			break;
		case EMultiSelectionState::PressFirstRowAgain:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::ReleaseFirstRowAgain);
			break;
		case EMultiSelectionState::ReleaseFirstRowAgain:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::VerifyPrimary);
			break;
		case EMultiSelectionState::VerifyPrimary:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::VerifyOutlines);
			break;
		case EMultiSelectionState::VerifyOutlines:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialPreparePass);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseMultiDetails(std::vector<FInputEvent>& InEvents)
{
	const unsigned Pass = DescribeMultiDetailsContext(Scenario.MultiSelection.Progress.GetState()).CaseIndex;
	const auto Phase = DescribeMultiDetailsContext(Scenario.MultiSelection.Progress.GetState()).Action;
	const auto A = Scenario.MultiSelectionObjects[0];
	const auto B = Scenario.MultiSelectionObjects[1];
	if (Phase == EMultiDetailsAction::PreparePass && Pass == 1)
	{
		auto Candidate = *Editor.Scene->FindNode(B);
		Candidate.Local().Values[12] = 7;
		Editor.Scene->EditNode(B, std::move(Candidate), Editor.Scene->GetRevision());
		Editor.ResetDocument();
	}
	if (Phase == EMultiDetailsAction::PressField || Phase == EMultiDetailsAction::ReleaseField)
	{
		Modifier(InEvents, 1);
		const auto Bounds = Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x");
		Click(InEvents, {(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2},
		      Phase == EMultiDetailsAction::PressField);
	}
	if ((Phase == EMultiDetailsAction::PressSelectAll || Phase == EMultiDetailsAction::ReleaseSelectAllAndType ||
	     Phase == EMultiDetailsAction::SubmitValue || Phase == EMultiDetailsAction::FinishSubmission))
	{
		if (Phase == EMultiDetailsAction::SubmitValue)
		{
			RequireMulti(Editor.Scene->FindNode(A)->Local().Values[12] == 3 &&
			                 Editor.Scene->FindNode(B)->Local().Values[12] == 3,
			             "valid same-value input must broadcast before Enter or blur");
		}
		if (Pass == 1 && (Phase == EMultiDetailsAction::SubmitValue || Phase == EMultiDetailsAction::FinishSubmission ||
		                  Phase == EMultiDetailsAction::VerifyHistory))
		{
			const auto Bounds = Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/y");
			Click(InEvents, {Bounds.X - Editor.Gui->Scale(8), (Bounds.Y + Bounds.W) / 2},
			      Phase == EMultiDetailsAction::SubmitValue);
		}
		else
		{
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = (Phase == EMultiDetailsAction::PreparePass || Phase == EMultiDetailsAction::PressField ||
			             Phase == EMultiDetailsAction::ReleaseField || Phase == EMultiDetailsAction::PressSelectAll ||
			             Phase == EMultiDetailsAction::ReleaseSelectAllAndType)
			                ? EKey::A
			                : EKey::Enter;
			Event.Modifiers = Phase == EMultiDetailsAction::PressSelectAll ? 1 : 0;
			Event.bDown = Phase == EMultiDetailsAction::PressSelectAll || Phase == EMultiDetailsAction::SubmitValue;
			InEvents.push_back(Event);
			if (Phase == EMultiDetailsAction::ReleaseSelectAllAndType)
			{
				Event.Type = EEventType::Text;
				Event.Text = "3";
				InEvents.push_back(Event);
			}
		}
	}
	if (Phase == EMultiDetailsAction::VerifyHistory)
	{
		RequireMulti(Editor.Scene->FindNode(A)->Local().Values[12] == 3 &&
		                 Editor.Scene->FindNode(B)->Local().Values[12] == 3,
		             Pass ? "submitting unchanged primary must broadcast" : "Details absolute local assignment");
		RequireMulti(Editor.Scene->FindNode(A)->Local().Values[13] == 0 &&
		                 Editor.Scene->FindNode(B)->Local().Values[13] == 1,
		             "unmodified vector axes were overwritten");
		RequireMulti(Editor.HistoryCursor == 1, "numeric interaction must be one batch history item");
		Editor.Undo();
		RequireMulti(Editor.Scene->FindNode(A)->Local().Values[12] == (Pass ? 3 : -1) &&
		                 Editor.Scene->FindNode(B)->Local().Values[12] == (Pass ? 7 : 1),
		             "batch undo original values");
		Editor.Redo();
		RequireMulti(Editor.Scene->FindNode(B)->Local().Values[12] == 3, "batch redo values");
	}
	switch (Scenario.MultiSelection.Progress.GetState())
	{
		case EMultiSelectionState::InitialPreparePass:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialPressField);
			break;
		case EMultiSelectionState::InitialPressField:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialReleaseField);
			break;
		case EMultiSelectionState::InitialReleaseField:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialPressSelectAll);
			break;
		case EMultiSelectionState::InitialPressSelectAll:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialReleaseSelectAllAndType);
			break;
		case EMultiSelectionState::InitialReleaseSelectAllAndType:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialSubmitValue);
			break;
		case EMultiSelectionState::InitialSubmitValue:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialFinishSubmission);
			break;
		case EMultiSelectionState::InitialFinishSubmission:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::InitialVerifyHistory);
			break;
		case EMultiSelectionState::InitialVerifyHistory:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryPreparePass);
			break;
		case EMultiSelectionState::UnchangedPrimaryPreparePass:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryPressField);
			break;
		case EMultiSelectionState::UnchangedPrimaryPressField:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryReleaseField);
			break;
		case EMultiSelectionState::UnchangedPrimaryReleaseField:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryPressSelectAll);
			break;
		case EMultiSelectionState::UnchangedPrimaryPressSelectAll:
			Scenario.MultiSelection.Progress.TransitionTo(
			    EMultiSelectionState::UnchangedPrimaryReleaseSelectAllAndType);
			break;
		case EMultiSelectionState::UnchangedPrimaryReleaseSelectAllAndType:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimarySubmitValue);
			break;
		case EMultiSelectionState::UnchangedPrimarySubmitValue:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryFinishSubmission);
			break;
		case EMultiSelectionState::UnchangedPrimaryFinishSubmission:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::UnchangedPrimaryVerifyHistory);
			break;
		case EMultiSelectionState::UnchangedPrimaryVerifyHistory:
			Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PositionPrepareModels);
			break;

		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseMultiSelection(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Viewport.bViewportVisible || !Editor.Scene->GetStatus().bReady || Scenario.bMultiSelectionVerified ||
	    Scenario.MultiSelection.Cadence.Advance() != EAcceptanceCadencePhase::Execute)
	{
		return;
	}
	if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PrepareFixtures))
	{
		const auto Found = Editor.PlacementModels.find("Cube");
		if (Found != Editor.PlacementModels.end() && Found->second.Data)
		{
			PrepareMultiSelection();
		}
	}
	else if (Scenario.MultiSelection.Progress.IsAny(
	             {EMultiSelectionState::PrepareFixtures, EMultiSelectionState::PressFirstRow,
	              EMultiSelectionState::ReleaseFirstRow, EMultiSelectionState::PressSecondRow,
	              EMultiSelectionState::ReleaseSecondRow, EMultiSelectionState::VerifyOutlinerSelection,
	              EMultiSelectionState::PressViewportToggle, EMultiSelectionState::ReleaseViewportModifier,
	              EMultiSelectionState::ReleaseViewportToggle, EMultiSelectionState::VerifyViewportToggle,
	              EMultiSelectionState::PressFirstRowAgain, EMultiSelectionState::ReleaseFirstRowAgain,
	              EMultiSelectionState::VerifyPrimary, EMultiSelectionState::VerifyOutlines}))
	{
		ExerciseMultiSelectionClicks(InEvents);
	}
	else if ((IsMultiDetailsState(Scenario.MultiSelection.Progress.GetState()) ||
	          Scenario.MultiSelection.Progress.IsAny(
	              {EMultiSelectionState::PrepareFixtures, EMultiSelectionState::PressFirstRow,
	               EMultiSelectionState::ReleaseFirstRow, EMultiSelectionState::PressSecondRow,
	               EMultiSelectionState::ReleaseSecondRow, EMultiSelectionState::VerifyOutlinerSelection,
	               EMultiSelectionState::PressViewportToggle, EMultiSelectionState::ReleaseViewportModifier,
	               EMultiSelectionState::ReleaseViewportToggle, EMultiSelectionState::VerifyViewportToggle,
	               EMultiSelectionState::PressFirstRowAgain, EMultiSelectionState::ReleaseFirstRowAgain,
	               EMultiSelectionState::VerifyPrimary, EMultiSelectionState::VerifyOutlines})))
	{
		ExerciseMultiDetails(InEvents);
	}
	else if ((IsMultiDetailsState(Scenario.MultiSelection.Progress.GetState()) ||
	          IsMultiGizmoState(Scenario.MultiSelection.Progress.GetState()) ||
	          Scenario.MultiSelection.Progress.IsAny(
	              {EMultiSelectionState::PrepareFixtures, EMultiSelectionState::PressFirstRow,
	               EMultiSelectionState::ReleaseFirstRow, EMultiSelectionState::PressSecondRow,
	               EMultiSelectionState::ReleaseSecondRow, EMultiSelectionState::VerifyOutlinerSelection,
	               EMultiSelectionState::PressViewportToggle, EMultiSelectionState::ReleaseViewportModifier,
	               EMultiSelectionState::ReleaseViewportToggle, EMultiSelectionState::VerifyViewportToggle,
	               EMultiSelectionState::PressFirstRowAgain, EMultiSelectionState::ReleaseFirstRowAgain,
	               EMultiSelectionState::VerifyPrimary, EMultiSelectionState::VerifyOutlines})))
	{
		ExerciseMultiGizmo(InEvents);
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::VerifyHistoryAndCancellation))
	{
		ExerciseMultiHistory();
		ExerciseMultiCancellation();
		Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::PrepareMarkers);
	}
	else if (Scenario.MultiSelection.Progress.Is(EMultiSelectionState::PrepareMarkers))
	{
		RequireMulti(Scenario.InspectionBounds.contains(RecordType<FSceneTransform>().Id + "/position/x") &&
		                 !Scenario.InspectionBounds.contains(RecordType<FSceneModelComponent>().Id + "/visible") &&
		                 !Scenario.InspectionBounds.contains(RecordType<FSceneCamera>().Id + "/verticalRadians"),
		             "heterogeneous selection must show only the common components");
		PrepareMultiSelectionMarkers();
		Scenario.MultiSelection.Progress.TransitionTo(EMultiSelectionState::VerifyMarkers);
	}
}

void FEditorAcceptanceHarness::PrepareMultiSelectionMarkers()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	Scenario.MultiSelectionObjects.clear();
	FSceneModel Model;
	Model.Data = Editor.PlacementModels.at("Cube").Data;
	Model.Name = "Multi Hidden Sections";
	Model.World = Translation({-1, 0, 0});
	for (const auto& Instance : SceneModelInstances(Model))
	{
		Model.Sections.push_back({ModelPrimitiveId(*Model.Data->Asset, Instance.Primitive), false});
	}
	Scenario.MultiSelectionObjects.push_back(Editor.Scene->Add(Model));
	Model.Name = "Multi Visible";
	Model.World = Translation({1, 0, 0});
	Model.Sections.clear();
	Scenario.MultiSelectionObjects.push_back(Editor.Scene->Add(Model));
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	Editor.GizmoMode = ETransformGizmoMode::Position;
	Editor.SelectObject(Scenario.MultiSelectionObjects[0]);
	Editor.ClickObject(Scenario.MultiSelectionObjects[1], true);
}

void FEditorAcceptanceHarness::CheckMultiSelectionMarkerDraws(const FGuiDrawData& InData)
{
	if (!Scenario.MultiSelection.Progress.Is(EMultiSelectionState::VerifyMarkers))
	{
		return;
	}
	RequireMulti(Editor.Selection.All().size() == 2 && Editor.Selection == Scenario.MultiSelectionObjects[1],
	             "marker non-primary fixture");
	const auto Point =
	    ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds, {-1, 0, 0});
	RequireMulti(bool(Point), "marker origin must be in view");
	const float Radius = Editor.Gui->Scale(6);
	const std::array<FVec2, 4> Corners{{{Point->X, Point->Y - Radius},
	                                    {Point->X + Radius, Point->Y},
	                                    {Point->X, Point->Y + Radius},
	                                    {Point->X - Radius, Point->Y}}};
	std::array<bool, 4> Drawn{};
	for (const auto& Command : InData.Commands)
	{
		if (Point->X < Command.Clip.X || Point->X >= Command.Clip.Z || Point->Y < Command.Clip.Y ||
		    Point->Y >= Command.Clip.W)
		{
			continue;
		}
		for (std::size_t Index = Command.FirstIndex; Index < Command.FirstIndex + Command.IndexCount; ++Index)
		{
			const auto& Vertex = InData.Vertices.at(InData.Indices.at(Index) + Command.VertexOffset);
			if ((Vertex.Color & 0xff) < 240 || ((Vertex.Color >> 8) & 0xff) < 160 ||
			    ((Vertex.Color >> 8) & 0xff) > 200 || ((Vertex.Color >> 16) & 0xff) > 60 || (Vertex.Color >> 24) < 128)
			{
				continue;
			}
			for (unsigned Corner = 0; Corner < Corners.size(); ++Corner)
			{
				// The antialiased polyline has outer vertices beyond each diamond corner.
				Drawn[Corner] |= std::abs(Vertex.Position.X - Corners[Corner].X) < 3 &&
				                 std::abs(Vertex.Position.Y - Corners[Corner].Y) < 3;
			}
		}
	}
	RequireMulti(std::all_of(Drawn.begin(), Drawn.end(),
	                         [](bool bInDrawn)
	                         {
		                         return bInDrawn;
	                         }),
	             "all hidden sections require a visible selection marker");
	Scenario.bMultiSelectionVerified = true;
}
} // namespace Hyperion
