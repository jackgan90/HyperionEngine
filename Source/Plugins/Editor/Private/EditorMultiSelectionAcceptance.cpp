#include "EditorApplication.h"
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
	Event.Button = 0;
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

void FEditorPlugin::PrepareMultiSelection()
{
	for (const auto Handle : Scene->GetNodes(ESceneNodeKind::Model))
	{
		Scene->SetModelVisible(Handle, false);
	}
	FSceneModel Model;
	Model.Data = PlacementModels.at("Cube").Data;
	Model.Name = "Multi A";
	Model.World = Translation({-1, 0, 0});
	MultiSelectionObjects.push_back(Scene->Add(Model));
	Model.Name = "Multi B";
	Model.World = Translation({1, 1, 0});
	MultiSelectionObjects.push_back(Scene->Add(Model));
	Filter = "Multi";
	ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	bViewportCameraInitialized = true;
	bShowLightMarkers = false;
	ResetDocument();
	SelectObject(std::nullopt);
	MultiSelectionStep = 1;
}

void FEditorPlugin::ExerciseMultiSelectionClicks(std::vector<FInputEvent>& InEvents)
{
	const auto A = MultiSelectionObjects[0];
	const auto B = MultiSelectionObjects[1];
	const auto Row = [&](FSceneHandle InHandle)
	{
		const auto Bounds = MultiSelectionRows.at(Scene->FindNode(InHandle)->Id);
		return FVec2{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	};
	if (MultiSelectionStep <= 4)
	{
		if (MultiSelectionStep == 3)
		{
			RequireMulti(Selection == A && Selection.All().size() == 1, "plain outliner click");
			Modifier(InEvents, 1);
		}
		if (MultiSelectionStep == 4)
		{
			Modifier(InEvents, 0);
		}
		Click(InEvents, Row(MultiSelectionStep <= 2 ? A : B), MultiSelectionStep % 2 == 1);
	}
	else if (MultiSelectionStep == 5)
	{
		RequireMulti(Selection == B && Selection.All().size() == 2 && !IsDirty() && History.empty(),
		             "Ctrl-add in outliner");
	}
	else if (MultiSelectionStep >= 6 && MultiSelectionStep <= 8)
	{
		if (MultiSelectionStep == 6)
		{
			Modifier(InEvents, 1);
		}
		const auto Point = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds, {-1, 0, 0});
		RequireMulti(bool(Point), "viewport point");
		if (MultiSelectionStep == 7)
		{
			Modifier(InEvents, 0);
		}
		else
		{
			Click(InEvents, {Point->X, Point->Y}, MultiSelectionStep == 6);
		}
	}
	else if (MultiSelectionStep == 9)
	{
		RequireMulti(Selection == B && Selection.All().size() == 1,
		             "viewport Ctrl-toggle did not retain press modifier");
	}
	else if (MultiSelectionStep == 10 || MultiSelectionStep == 11)
	{
		Modifier(InEvents, 1);
		Click(InEvents, Row(A), MultiSelectionStep == 10);
	}
	else if (MultiSelectionStep == 12)
	{
		Modifier(InEvents, 0);
		RequireMulti(Selection == A && Selection.All().size() == 2, "primary after re-add");
	}
	else
	{
		if (RenderStats.SelectionOutline.PendingItems)
		{
			return;
		}
		RequireMulti(RenderStats.SelectionOutline.Items >= 2, "both selected models need outlines");
	}
	++MultiSelectionStep;
}

void FEditorPlugin::ExerciseMultiDetails(std::vector<FInputEvent>& InEvents)
{
	const unsigned Pass = (MultiSelectionStep - 14) / 8;
	const unsigned Phase = (MultiSelectionStep - 14) % 8;
	const auto A = MultiSelectionObjects[0];
	const auto B = MultiSelectionObjects[1];
	if (Phase == 0 && Pass == 1)
	{
		auto Candidate = *Scene->FindNode(B);
		Candidate.Local().Values[12] = 7;
		Scene->EditNode(B, std::move(Candidate), Scene->GetRevision());
		ResetDocument();
	}
	if (Phase == 1 || Phase == 2)
	{
		Modifier(InEvents, 1);
		const auto Bounds = InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/x");
		Click(InEvents, {(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2}, Phase == 1);
	}
	if (Phase >= 3 && Phase <= 6)
	{
		if (Phase == 5)
		{
			RequireMulti(Scene->FindNode(A)->Local().Values[12] == 3 && Scene->FindNode(B)->Local().Values[12] == 3,
			             "valid same-value input must broadcast before Enter or blur");
		}
		if (Pass == 1 && Phase >= 5)
		{
			const auto Bounds = InspectionBounds.at(RecordType<FSceneTransform>().Id + "/position/y");
			Click(InEvents, {Bounds.X - Gui->Scale(8), (Bounds.Y + Bounds.W) / 2}, Phase == 5);
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
		RequireMulti(Scene->FindNode(A)->Local().Values[12] == 3 && Scene->FindNode(B)->Local().Values[12] == 3,
		             Pass ? "submitting unchanged primary must broadcast" : "Details absolute local assignment");
		RequireMulti(Scene->FindNode(A)->Local().Values[13] == 0 && Scene->FindNode(B)->Local().Values[13] == 1,
		             "unmodified vector axes were overwritten");
		RequireMulti(HistoryCursor == 1, "numeric interaction must be one batch history item");
		Undo();
		RequireMulti(Scene->FindNode(A)->Local().Values[12] == (Pass ? 3 : -1) &&
		                 Scene->FindNode(B)->Local().Values[12] == (Pass ? 7 : 1),
		             "batch undo original values");
		Redo();
		RequireMulti(Scene->FindNode(B)->Local().Values[12] == 3, "batch redo values");
	}
	++MultiSelectionStep;
}

void FEditorPlugin::ExerciseMultiSelection(std::vector<FInputEvent>& InEvents)
{
	if (!bViewportVisible || !Scene->GetStatus().bReady || bMultiSelectionVerified || ++MultiSelectionWait % 3 != 0)
	{
		return;
	}
	if (MultiSelectionStep == 0)
	{
		const auto Found = PlacementModels.find("Cube");
		if (Found != PlacementModels.end() && Found->second.Data)
		{
			PrepareMultiSelection();
		}
	}
	else if (MultiSelectionStep < 14)
	{
		ExerciseMultiSelectionClicks(InEvents);
	}
	else if (MultiSelectionStep < 30)
	{
		ExerciseMultiDetails(InEvents);
	}
	else if (MultiSelectionStep < 54)
	{
		ExerciseMultiGizmo(InEvents);
	}
	else if (MultiSelectionStep == 54)
	{
		ExerciseMultiHistory();
		ExerciseMultiCancellation();
		++MultiSelectionStep;
	}
	else if (MultiSelectionStep == 55)
	{
		RequireMulti(InspectionBounds.contains(RecordType<FSceneTransform>().Id + "/position/x") &&
		                 !InspectionBounds.contains(RecordType<FSceneModelComponent>().Id + "/visible") &&
		                 !InspectionBounds.contains(RecordType<FSceneCamera>().Id + "/verticalRadians"),
		             "heterogeneous selection must show only the common components");
		PrepareMultiSelectionMarkers();
		++MultiSelectionStep;
	}
}

void FEditorPlugin::PrepareMultiSelectionMarkers()
{
	for (const auto Handle : Scene->GetNodes(ESceneNodeKind::Model))
	{
		Scene->SetModelVisible(Handle, false);
	}
	MultiSelectionObjects.clear();
	FSceneModel Model;
	Model.Data = PlacementModels.at("Cube").Data;
	Model.Name = "Multi Hidden Sections";
	Model.World = Translation({-1, 0, 0});
	for (const auto& Instance : SceneModelInstances(Model))
	{
		Model.Sections.push_back({ModelPrimitiveId(*Model.Data->Asset, Instance.Primitive), false});
	}
	MultiSelectionObjects.push_back(Scene->Add(Model));
	Model.Name = "Multi Visible";
	Model.World = Translation({1, 0, 0});
	Model.Sections.clear();
	MultiSelectionObjects.push_back(Scene->Add(Model));
	ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	GizmoMode = ETransformGizmoMode::Position;
	SelectObject(MultiSelectionObjects[0]);
	ClickObject(MultiSelectionObjects[1], true);
}

void FEditorPlugin::CheckMultiSelectionMarkerDraws(const FGuiDrawData& InData)
{
	if (MultiSelectionStep != 56)
	{
		return;
	}
	RequireMulti(Selection.All().size() == 2 && Selection == MultiSelectionObjects[1], "marker non-primary fixture");
	const auto Point = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds, {-1, 0, 0});
	RequireMulti(bool(Point), "marker origin must be in view");
	const float Radius = Gui->Scale(6);
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
	bMultiSelectionVerified = true;
}
} // namespace Hyperion
