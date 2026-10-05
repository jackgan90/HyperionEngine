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
	Scenario.MultiSelectionStep = 1;
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
	if (Scenario.MultiSelectionStep <= 4)
	{
		if (Scenario.MultiSelectionStep == 3)
		{
			RequireMulti(Editor.Selection == A && Editor.Selection.All().size() == 1, "plain outliner click");
			Modifier(InEvents, 1);
		}
		if (Scenario.MultiSelectionStep == 4)
		{
			Modifier(InEvents, 0);
		}
		Click(InEvents, Row(Scenario.MultiSelectionStep <= 2 ? A : B), Scenario.MultiSelectionStep % 2 == 1);
	}
	else if (Scenario.MultiSelectionStep == 5)
	{
		RequireMulti(Editor.Selection == B && Editor.Selection.All().size() == 2 && !Editor.IsDirty() &&
		                 Editor.History.empty(),
		             "Ctrl-add in outliner");
	}
	else if (Scenario.MultiSelectionStep >= 6 && Scenario.MultiSelectionStep <= 8)
	{
		if (Scenario.MultiSelectionStep == 6)
		{
			Modifier(InEvents, 1);
		}
		const auto Point =
		    ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds, {-1, 0, 0});
		RequireMulti(bool(Point), "viewport point");
		if (Scenario.MultiSelectionStep == 7)
		{
			Modifier(InEvents, 0);
		}
		else
		{
			Click(InEvents, {Point->X, Point->Y}, Scenario.MultiSelectionStep == 6);
		}
	}
	else if (Scenario.MultiSelectionStep == 9)
	{
		RequireMulti(Editor.Selection == B && Editor.Selection.All().size() == 1,
		             "viewport Ctrl-toggle did not retain press modifier");
	}
	else if (Scenario.MultiSelectionStep == 10 || Scenario.MultiSelectionStep == 11)
	{
		Modifier(InEvents, 1);
		Click(InEvents, Row(A), Scenario.MultiSelectionStep == 10);
	}
	else if (Scenario.MultiSelectionStep == 12)
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
	++Scenario.MultiSelectionStep;
}

void FEditorAcceptanceHarness::ExerciseMultiDetails(std::vector<FInputEvent>& InEvents)
{
	const unsigned Pass = (Scenario.MultiSelectionStep - 14) / 8;
	const unsigned Phase = (Scenario.MultiSelectionStep - 14) % 8;
	const auto A = Scenario.MultiSelectionObjects[0];
	const auto B = Scenario.MultiSelectionObjects[1];
	if (Phase == 0 && Pass == 1)
	{
		auto Candidate = *Editor.Scene->FindNode(B);
		Candidate.Local().Values[12] = 7;
		Editor.Scene->EditNode(B, std::move(Candidate), Editor.Scene->GetRevision());
		Editor.ResetDocument();
	}
	if (Phase == 1 || Phase == 2)
	{
		Modifier(InEvents, 1);
		const auto Bounds = Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x");
		Click(InEvents, {(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2}, Phase == 1);
	}
	if (Phase >= 3 && Phase <= 6)
	{
		if (Phase == 5)
		{
			RequireMulti(Editor.Scene->FindNode(A)->Local().Values[12] == 3 &&
			                 Editor.Scene->FindNode(B)->Local().Values[12] == 3,
			             "valid same-value input must broadcast before Enter or blur");
		}
		if (Pass == 1 && Phase >= 5)
		{
			const auto Bounds = Scenario.InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/y");
			Click(InEvents, {Bounds.X - Editor.Gui->Scale(8), (Bounds.Y + Bounds.W) / 2}, Phase == 5);
		}
		else
		{
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = Phase <= 4 ? EKey::A : EKey::Enter;
			Event.Modifiers = Phase == 3 ? 1 : 0;
			Event.bDown = Phase == 3 || Phase == 5;
			InEvents.push_back(Event);
			if (Phase == 4)
			{
				Event.Type = EEventType::Text;
				Event.Text = "3";
				InEvents.push_back(Event);
			}
		}
	}
	if (Phase == 7)
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
	++Scenario.MultiSelectionStep;
}

void FEditorAcceptanceHarness::ExerciseMultiSelection(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Viewport.bViewportVisible || !Editor.Scene->GetStatus().bReady || Scenario.bMultiSelectionVerified ||
	    ++Scenario.MultiSelectionWait % 3 != 0)
	{
		return;
	}
	if (Scenario.MultiSelectionStep == 0)
	{
		const auto Found = Editor.PlacementModels.find("Cube");
		if (Found != Editor.PlacementModels.end() && Found->second.Data)
		{
			PrepareMultiSelection();
		}
	}
	else if (Scenario.MultiSelectionStep < 14)
	{
		ExerciseMultiSelectionClicks(InEvents);
	}
	else if (Scenario.MultiSelectionStep < 30)
	{
		ExerciseMultiDetails(InEvents);
	}
	else if (Scenario.MultiSelectionStep < 54)
	{
		ExerciseMultiGizmo(InEvents);
	}
	else if (Scenario.MultiSelectionStep == 54)
	{
		ExerciseMultiHistory();
		ExerciseMultiCancellation();
		++Scenario.MultiSelectionStep;
	}
	else if (Scenario.MultiSelectionStep == 55)
	{
		RequireMulti(Scenario.InspectionBounds.contains(RecordType<FSceneTransform>().Id + "/position/x") &&
		                 !Scenario.InspectionBounds.contains(RecordType<FSceneModelComponent>().Id + "/visible") &&
		                 !Scenario.InspectionBounds.contains(RecordType<FSceneCamera>().Id + "/verticalRadians"),
		             "heterogeneous selection must show only the common components");
		PrepareMultiSelectionMarkers();
		++Scenario.MultiSelectionStep;
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
	if (Scenario.MultiSelectionStep != 56)
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
