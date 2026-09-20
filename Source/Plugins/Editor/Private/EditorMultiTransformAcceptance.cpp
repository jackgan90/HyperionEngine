#include "EditorApplication.h"
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
		Event.Button = 0;
		Event.bDown = bInDown;
		InEvents.push_back(Event);
	}
}
} // namespace

void FEditorPlugin::ExerciseMultiGizmo(std::vector<FInputEvent>& InEvents)
{
	const unsigned Mode = (MultiSelectionStep - 30) / 8;
	const unsigned Phase = (MultiSelectionStep - 30) % 8;
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const float Size = 85 * Gui->ApplicationScale();
	const float Diagonal = Size / std::sqrt(2.f);
	const auto Start = Mode == 1 ? FVec2{Center.X + Diagonal, Center.Y - Diagonal} : Center;
	const auto End = Mode == 1 ? FVec2{Center.X - Diagonal, Center.Y - Diagonal} : FVec2{Center.X + Size, Center.Y};
	if (Phase == 0)
	{
		FinishInspectorEdit();
		GizmoMode = static_cast<ETransformGizmoMode>(Mode);
		std::vector<FSceneNodeEdit> Edits;
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			auto Node = *Scene->FindNode(MultiSelectionObjects[Index]);
			Node.Local() = Translation({float(Index * 2), 0, 0});
			MultiSelectionInitial[Index] = Node.Local();
			Edits.push_back({MultiSelectionObjects[Index], std::move(Node)});
		}
		Scene->EditNodes(std::move(Edits), Scene->GetRevision());
		ResetDocument();
	}
	if (Phase == 1 || Phase == 3)
	{
		Pointer(InEvents, Phase == 1 ? Start : End, true, Phase == 1);
	}
	if (Phase == 2)
	{
		RequireMulti(Gizmo.IsDragging(), "group handle capture");
		Pointer(InEvents, End, false, false);
	}
	if (Phase == 4)
	{
		RequireMulti(HistoryCursor == 1 && !Gizmo.IsDragging(), "one group history entry");
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			MultiSelectionFinal[Index] = Scene->FindNode(MultiSelectionObjects[Index])->Local();
		}
		const auto& A = MultiSelectionFinal[0];
		const auto& B = MultiSelectionFinal[1];
		if (Mode == 0)
		{
			RequireMulti(A.Values[12] > 0 && std::abs(B.Values[12] - A.Values[12] - 2) < .0001f,
			             "common world translation");
		}
		else if (Mode == 1)
		{
			RequireMulti(std::abs(A.Values[12]) < .0001f && std::abs(B.Values[12]) < .0001f &&
			                 std::abs(B.Values[13] - 2) < .0001f && std::abs(B.Values[1] - 1) < .0001f,
			             "rotation must orbit and rotate the secondary");
		}
		else
		{
			// GUI pointer coordinates are floored to logical pixels before the drag controller sees them.
			const float Factor = 1 + (std::floor(End.X) - std::floor(Start.X)) / Size;
			RequireMulti(std::abs(B.Values[12] - 2 * Factor) < .0001f && std::abs(B.Values[0] - Factor) < .0001f,
			             "group scale must change offset and shape");
		}
		Undo();
	}
	if (Phase == 5 || Phase == 7)
	{
		for (unsigned Index = 0; Index < 2; ++Index)
		{
			const auto& Expected = Phase == 5 ? MultiSelectionInitial[Index] : MultiSelectionFinal[Index];
			RequireMulti(Scene->FindNode(MultiSelectionObjects[Index])->Local().Values == Expected.Values,
			             "group undo/redo exact matrices");
		}
	}
	if (Phase == 6)
	{
		Redo();
	}
	++MultiSelectionStep;
}

void FEditorPlugin::ExerciseMultiHistory()
{
	const auto A = MultiSelectionObjects[0];
	const auto B = MultiSelectionObjects[1];
	const auto AId = Scene->FindNode(A)->Id;
	const auto BId = Scene->FindNode(B)->Id;
	auto Child = MakeSceneCameraNode("multi-child");
	Child.Name = "Multi Child";
	Child.Parent() = AId;
	Child.Local() = Translation({1, 0, 0});
	const auto ChildHandle = Scene->AddNode(Child);
	auto Settings = Scene->GetSettings();
	Settings.DefaultCamera = ChildHandle;
	Scene->SetSettings(Settings);
	SelectObject(A);
	ClickObject(B, true);
	ClickObject(ChildHandle, true);
	RequireMulti(SelectedRoots().size() == 2, "selected child must not be transformed twice");
	ResetDocument();
	FSceneNodeView Primary;
	FSceneNodeView Parent;
	Scene->GetNodeView(ChildHandle, Primary);
	Scene->GetNodeView(A, Parent);
	const FVec3 Pivot{Primary.World.Values[12], Primary.World.Values[13], Primary.World.Values[14]};
	ViewCamera.World = SceneCameraTransform(Add(Pivot, {0, 0, 10}), Pivot);
	GizmoMode = ETransformGizmoMode::Position;
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	RequireMulti(Gizmo.Configure(ViewCamera, Bounds, Primary.Node->Local(), Parent.World, GizmoMode) &&
	                 Gizmo.Begin(Center),
	             "hierarchy primary gizmo");
	BeginGizmoEdit(Primary, Bounds);
	FMat4 Local;
	RequireMulti(Gizmo.Drag({Center.X + 40, Center.Y}, Local), "hierarchy translation input");
	const auto Expected = Multiply(Parent.World, Local);
	PreviewGizmoEdit(Local);
	FinishGizmo();
	Scene->GetNodeView(ChildHandle, Primary);
	RequireMulti(std::abs(Primary.World.Values[12] - Expected.Values[12]) < .0001f &&
	                 Primary.Node->Local().Values == Child.Local().Values,
	             "child world moved twice");
	const auto Revision = Scene->GetRevision();
	const auto Cursor = HistoryCursor;
	auto Valid = *Scene->FindNode(A);
	auto Invalid = *Scene->FindNode(B);
	Valid.Name = "Must not commit";
	Invalid.Local().Values[0] = std::numeric_limits<float>::infinity();
	bool bRejected{};
	try
	{
		CommitEdits({{A, Valid}, {B, Invalid}}, Revision);
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	RequireMulti(bRejected && Scene->GetRevision() == Revision && HistoryCursor == Cursor &&
	                 Scene->FindNode(A)->Name != Valid.Name,
	             "failed batch partially committed");
	CommitDelete();
	RequireMulti(!Selection && !Scene->FindNode(A) && !Scene->FindNode(B) && !Scene->FindNode(ChildHandle),
	             "multi-delete");
	Undo();
	const auto RestoredChild = Scene->FindHandle(Child.Id);
	RequireMulti(Selection.All().size() == 3 && Selection == RestoredChild && RestoredChild != ChildHandle &&
	                 Scene->GetSettings().DefaultCamera == RestoredChild,
	             "delete undo selection/reference remap");
	Undo();
	Redo();
	Redo();
	RequireMulti(!Selection && !Scene->FindHandle(AId).Scene && !Scene->FindHandle(BId).Scene,
	             "remapped redo deletion");
	Undo();
}

void FEditorPlugin::ExerciseMultiCancellation()
{
	const auto A = Scene->FindHandle("multi-child");
	const auto BeforeSelection = Selection;
	ClickObject(A, true);
	RequireMulti(Selection.All().size() == 2 && Selection.Primary() == BeforeSelection.All()[1],
	             "removing the primary must choose the last remaining target");
	ClickObject(std::nullopt, true);
	RequireMulti(Selection.All().size() == 2, "Ctrl miss must preserve selection");
	SetSelection(BeforeSelection);
	Undo();
	const auto Cursor = HistoryCursor;
	const auto Count = History.size();
	const auto State = DocumentState;
	FSceneNodeView Primary;
	FSceneNodeView Parent;
	Scene->GetNodeView(A, Primary);
	Scene->GetNodeView(Scene->FindHandle(Primary.Node->Parent()), Parent);
	const FVec3 Pivot{Primary.World.Values[12], Primary.World.Values[13], Primary.World.Values[14]};
	ViewCamera.World = SceneCameraTransform(Add(Pivot, {0, 0, 10}), Pivot);
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	RequireMulti(
	    Gizmo.Configure(ViewCamera, Bounds, Primary.Node->Local(), Parent.World, ETransformGizmoMode::Position) &&
	        Gizmo.Begin(Center),
	    "cancel group capture");
	BeginGizmoEdit(Primary, Bounds);
	const auto Targets = GizmoEdit->Targets;
	FMat4 Local;
	RequireMulti(Gizmo.Drag({Center.X + 40, Center.Y}, Local), "cancel group preview");
	PreviewGizmoEdit(Local);
	FinishGizmo(true);
	for (const auto& Target : Targets)
	{
		RequireMulti(Scene->FindNode(Target.Handle)->Local().Values == Target.Initial.Values, "cancel exact matrices");
	}
	RequireMulti(HistoryCursor == Cursor && History.size() == Count && DocumentState == State &&
	                 Selection == BeforeSelection,
	             "cancel must preserve redo branch and selection");
	Redo();
}
} // namespace Hyperion
