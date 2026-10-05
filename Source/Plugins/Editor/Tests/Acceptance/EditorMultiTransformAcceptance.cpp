#include "EditorAcceptanceHarness.h"
#include <cmath>
#include <limits>

namespace Hyperion
{
namespace
{
void RequireMulti(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Multi-transform acceptance: ") + InMessage);
	}
}

void Pointer(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInButton, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
	if (bInButton)
	{
		Event.Type = EEventType::MouseButton;
		Event.Button = InputButtons::Left;
		Event.bDown = bInDown;
		InEvents.push_back(Event);
	}
}
} // namespace

void FEditorAcceptanceHarness::ExerciseMultiGizmo(std::vector<FInputEvent>& InEvents)
{
	const unsigned Mode = (Scenario.MultiSelectionStep - 30) / 8;
	const unsigned Phase = (Scenario.MultiSelectionStep - 30) % 8;
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const float Size = 85 * Editor.Gui->ApplicationScale();
	const float Diagonal = Size / std::sqrt(2.f);
	const auto Start = Mode == 1 ? FVec2{Center.X + Diagonal, Center.Y - Diagonal} : Center;
	const auto End = Mode == 1 ? FVec2{Center.X - Diagonal, Center.Y - Diagonal} : FVec2{Center.X + Size, Center.Y};
	if (Phase == 0)
	{
		Editor.FinishInspectorEdit();
		Editor.GizmoMode = static_cast<ETransformGizmoMode>(Mode);
		std::vector<FSceneNodeEdit> Edits;
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			auto Node = *Editor.Scene->FindNode(Scenario.MultiSelectionObjects[Index]);
			Node.Local() = Translation({float(Index * 2), 0, 0});
			Scenario.MultiSelectionInitial[Index] = Node.Local();
			Edits.push_back({Scenario.MultiSelectionObjects[Index], std::move(Node)});
		}
		Editor.Scene->EditNodes(std::move(Edits), Editor.Scene->GetRevision());
		Editor.ResetDocument();
	}
	if (Phase == 1 || Phase == 3)
	{
		Pointer(InEvents, Phase == 1 ? Start : End, true, Phase == 1);
	}
	if (Phase == 2)
	{
		RequireMulti(Editor.Gizmo.IsDragging(), "group handle capture");
		Pointer(InEvents, End, false, false);
	}
	if (Phase == 4)
	{
		RequireMulti(Editor.HistoryCursor == 1 && !Editor.Gizmo.IsDragging(), "one group history entry");
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			Scenario.MultiSelectionFinal[Index] =
			    Editor.Scene->FindNode(Scenario.MultiSelectionObjects[Index])->Local();
		}
		const auto& A = Scenario.MultiSelectionFinal[0];
		const auto& B = Scenario.MultiSelectionFinal[1];
		if (Mode == 0)
		{
			RequireMulti(A.Values[12] > 0 && std::abs(B.Values[12] - A.Values[12] - 2) < .0001f,
			             "common world translation");
		}
		else if (Mode == 1)
		{
			// The toolbar can place the pivot between logical pixels. ImGui floors mouse positions, so the
			// submitted gesture need not be exactly 90 degrees even though the unfloored endpoints are.
			const float From = std::atan2(Center.Y - std::floor(Start.Y), std::floor(Start.X) - Center.X);
			const float To = std::atan2(Center.Y - std::floor(End.Y), std::floor(End.X) - Center.X);
			const float Angle = To - From;
			RequireMulti(std::abs(A.Values[12]) < .0001f && std::abs(B.Values[12] - 2 * std::cos(Angle)) < .0001f &&
			                 std::abs(B.Values[13] - 2 * std::sin(Angle)) < .0001f &&
			                 std::abs(B.Values[1] - std::sin(Angle)) < .0001f,
			             "rotation must orbit and rotate the secondary");
		}
		else
		{
			// GUI pointer coordinates are floored to logical pixels before the drag controller sees them.
			const float Factor = 1 + (std::floor(End.X) - std::floor(Start.X)) / Size;
			RequireMulti(std::abs(B.Values[12] - 2 * Factor) < .0001f && std::abs(B.Values[0] - Factor) < .0001f,
			             "group scale must change offset and shape");
		}
		Editor.Undo();
	}
	if (Phase == 5 || Phase == 7)
	{
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			const auto& Expected =
			    Phase == 5 ? Scenario.MultiSelectionInitial[Index] : Scenario.MultiSelectionFinal[Index];
			RequireMulti(Editor.Scene->FindNode(Scenario.MultiSelectionObjects[Index])->Local().Values ==
			                 Expected.Values,
			             "group undo/redo exact matrices");
		}
	}
	if (Phase == 6)
	{
		Editor.Redo();
	}
	++Scenario.MultiSelectionStep;
}

void FEditorAcceptanceHarness::ExerciseMultiHistory()
{
	const auto A = Scenario.MultiSelectionObjects[0];
	const auto B = Scenario.MultiSelectionObjects[1];
	const auto AId = Editor.Scene->FindNode(A)->Id;
	const auto BId = Editor.Scene->FindNode(B)->Id;
	auto Child = MakeSceneCameraNode("multi-child");
	Child.Name = "Multi Child";
	Child.Parent() = AId;
	Child.Local() = Translation({1, 0, 0});
	const auto ChildHandle = Editor.Scene->AddNode(Child);
	auto Settings = Editor.Scene->GetSettings();
	Settings.DefaultCamera = ChildHandle;
	Editor.Scene->SetSettings(Settings);
	Editor.SelectObject(A);
	Editor.ClickObject(B, true);
	Editor.ClickObject(ChildHandle, true);
	RequireMulti(Editor.SelectedRoots().size() == 2, "selected child must not be transformed twice");
	Editor.ResetDocument();
	FSceneNodeView Primary;
	FSceneNodeView Parent;
	Editor.Scene->GetNodeView(ChildHandle, Primary);
	Editor.Scene->GetNodeView(A, Parent);
	const FVec3 Pivot{Primary.World.Values[12], Primary.World.Values[13], Primary.World.Values[14]};
	Editor.Viewport.ViewCamera.World = SceneCameraTransform(Add(Pivot, {0, 0, 10}), Pivot);
	Editor.GizmoMode = ETransformGizmoMode::Position;
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	RequireMulti(Editor.Gizmo.Configure(Editor.Viewport.ViewCamera, Bounds, Primary.Node->Local(), Parent.World,
	                                    Editor.GizmoMode) &&
	                 Editor.Gizmo.Begin(Center),
	             "hierarchy primary gizmo");
	Editor.BeginGizmoEdit(Primary, Bounds);
	FMat4 Local;
	RequireMulti(Editor.Gizmo.Drag({Center.X + 40, Center.Y}, Local), "hierarchy translation input");
	const auto Expected = Multiply(Parent.World, Local);
	Editor.PreviewGizmoEdit(Local);
	Editor.FinishGizmo();
	Editor.Scene->GetNodeView(ChildHandle, Primary);
	RequireMulti(std::abs(Primary.World.Values[12] - Expected.Values[12]) < .0001f &&
	                 Primary.Node->Local().Values == Child.Local().Values,
	             "child world moved twice");
	const auto Revision = Editor.Scene->GetRevision();
	const auto Cursor = Editor.HistoryCursor;
	auto Valid = *Editor.Scene->FindNode(A);
	auto Invalid = *Editor.Scene->FindNode(B);
	Valid.Name = "Must not commit";
	Invalid.Local().Values[0] = std::numeric_limits<float>::infinity();
	bool bRejected{};
	try
	{
		Editor.CommitEdits({{A, Valid}, {B, Invalid}}, Revision);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	RequireMulti(bRejected && Editor.Scene->GetRevision() == Revision && Editor.HistoryCursor == Cursor &&
	                 Editor.Scene->FindNode(A)->Name != Valid.Name,
	             "failed batch partially committed");
	Editor.CommitDelete();
	RequireMulti(!Editor.Selection && !Editor.Scene->FindNode(A) && !Editor.Scene->FindNode(B) &&
	                 !Editor.Scene->FindNode(ChildHandle),
	             "multi-delete");
	Editor.Undo();
	const auto RestoredChild = Editor.Scene->FindHandle(Child.Id);
	RequireMulti(Editor.Selection.All().size() == 3 && Editor.Selection == RestoredChild &&
	                 RestoredChild != ChildHandle && Editor.Scene->GetSettings().DefaultCamera == RestoredChild,
	             "delete undo selection/reference remap");
	Editor.Undo();
	Editor.Redo();
	Editor.Redo();
	RequireMulti(!Editor.Selection && !Editor.Scene->FindHandle(AId).Scene && !Editor.Scene->FindHandle(BId).Scene,
	             "remapped redo deletion");
	Editor.Undo();
}

void FEditorAcceptanceHarness::ExerciseMultiCancellation()
{
	const auto A = Editor.Scene->FindHandle("multi-child");
	const auto BeforeSelection = Editor.Selection;
	Editor.ClickObject(A, true);
	RequireMulti(Editor.Selection.All().size() == 2 && Editor.Selection.Primary() == BeforeSelection.All()[1],
	             "removing the primary must choose the last remaining target");
	Editor.ClickObject(std::nullopt, true);
	RequireMulti(Editor.Selection.All().size() == 2, "Ctrl miss must preserve selection");
	Editor.SetSelection(BeforeSelection);
	Editor.Undo();
	const auto Cursor = Editor.HistoryCursor;
	const auto Count = Editor.History.size();
	const auto State = Editor.DocumentState;
	FSceneNodeView Primary;
	FSceneNodeView Parent;
	Editor.Scene->GetNodeView(A, Primary);
	Editor.Scene->GetNodeView(Editor.Scene->FindHandle(Primary.Node->Parent()), Parent);
	const FVec3 Pivot{Primary.World.Values[12], Primary.World.Values[13], Primary.World.Values[14]};
	Editor.Viewport.ViewCamera.World = SceneCameraTransform(Add(Pivot, {0, 0, 10}), Pivot);
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	RequireMulti(Editor.Gizmo.Configure(Editor.Viewport.ViewCamera, Bounds, Primary.Node->Local(), Parent.World,
	                                    ETransformGizmoMode::Position) &&
	                 Editor.Gizmo.Begin(Center),
	             "cancel group capture");
	Editor.BeginGizmoEdit(Primary, Bounds);
	const auto Targets = Editor.GizmoEdit->Targets;
	FMat4 Local;
	RequireMulti(Editor.Gizmo.Drag({Center.X + 40, Center.Y}, Local), "cancel group preview");
	Editor.PreviewGizmoEdit(Local);
	Editor.FinishGizmo(true);
	for (const auto& Target : Targets)
	{
		RequireMulti(Editor.Scene->FindNode(Target.Handle)->Local().Values == Target.Initial.Values,
		             "cancel exact matrices");
	}
	RequireMulti(Editor.HistoryCursor == Cursor && Editor.History.size() == Count && Editor.DocumentState == State &&
	                 Editor.Selection == BeforeSelection,
	             "cancel must preserve redo branch and selection");
	Editor.Redo();
}
} // namespace Hyperion
