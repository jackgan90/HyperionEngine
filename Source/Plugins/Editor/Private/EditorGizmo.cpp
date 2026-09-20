#include "EditorApplication.h"

namespace Hyperion
{
void FEditorPlugin::DrawGizmoToolbar()
{
	constexpr std::array Labels{"##TransformPosition", "##TransformRotation", "##TransformScale"};
	constexpr std::array Icons{EGuiIcon::Translate, EGuiIcon::Rotate, EGuiIcon::Scale};
	constexpr std::array Tips{"Position - move along world axes or drag the center freely",
	                          "Rotation - rotate around a local axis",
	                          "Scale - resize along a local axis or drag the center uniformly"};
	for (unsigned Index = 0; Index < Labels.size(); ++Index)
	{
		if (Index)
		{
			Gui->SameLine();
		}
		const auto Mode = static_cast<ETransformGizmoMode>(Index);
		if (Gui->IconButton(Labels[Index], Icons[Index], Tips[Index], GizmoMode == Mode))
		{
			FinishInspectorEdit();
			GizmoMode = Mode;
		}
		GizmoButtonBounds[Index] = Gui->LastItemBounds();
	}
}

void FEditorPlugin::FinishGizmo(bool bInCancel)
{
	Gizmo.End();
	if (Gui)
	{
		Gui->CaptureImagePointer(false);
	}
	if (!GizmoEdit)
	{
		return;
	}
	const auto Edit = std::exchange(GizmoEdit, {});
	try
	{
		const auto* Node = Scene->FindNode(Edit->Handle);
		if (!Node || Node->Local().Values != Edit->Preview.Values)
		{
			return;
		}
		const auto After = *Node;
		if (After.Local().Values == Edit->Initial.Values)
		{
			return;
		}
		// Only own the transform: asynchronous changes to other properties must survive this transaction.
		auto Before = After;
		Before.Local() = Edit->Initial;
		if (!Scene->EditNode(Edit->Handle, std::move(Before), Scene->GetRevision()))
		{
			throw std::runtime_error("Transform target is no longer current");
		}
		if (!bInCancel)
		{
			CommitEdit(Edit->Handle, After, Scene->GetRevision());
		}
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::DrawGizmo()
{
	FSceneNodeView View;
	if (Placement.IsActive() || bPlacementUsedMouse || !Selection || !Scene->GetNodeView(*Selection, View) ||
	    !bViewportCameraInitialized || bOpenDialog || bSaveDialog || bDiscardDialog || PreviewCamera)
	{
		FinishGizmo();
		return;
	}
	const auto Pointer = Gui->PointerState();
	if (!Pointer.bPositionValid)
	{
		// Focus loss clears the pointer to a finite sentinel; commit the last valid preview.
		FinishGizmo(Pointer.bCancel);
		return;
	}
	const auto Bounds = ViewportRegion.Bounds;
	if (GizmoEdit &&
	    (GizmoEdit->Handle != *Selection || GizmoEdit->Revision != Scene->GetRevision() || !ViewportRegion.bFocused ||
	     Pointer.bCancel || Pointer.bRightDown || GizmoEdit->Bounds.X != Bounds.X || GizmoEdit->Bounds.Y != Bounds.Y ||
	     GizmoEdit->Bounds.Z != Bounds.Z || GizmoEdit->Bounds.W != Bounds.W))
	{
		FinishGizmo(Pointer.bCancel);
		return;
	}
	FMat4 Parent = Identity();
	if (!View.Node->Parent().empty())
	{
		FSceneNodeView ParentView;
		if (!Scene->GetNodeView(Scene->FindHandle(View.Node->Parent()), ParentView))
		{
			FinishGizmo();
			return;
		}
		Parent = ParentView.World;
	}
	if (!Gizmo.Configure(ViewCamera, Bounds, View.Node->Local(), Parent, GizmoMode, Gui->ApplicationScale()))
	{
		return;
	}
	if (!GizmoEdit && Pointer.bPressed && !Pointer.bRightDown && !bCameraDragging && ViewportRegion.bHovered &&
	    ViewportRegion.bFocused)
	{
		FinishInspectorEdit();
		if (Gizmo.Begin(Pointer.Position))
		{
			GizmoEdit = FGizmoEdit{*Selection, View.Node->Local(), View.Node->Local(), Scene->GetRevision(), Bounds};
			Gui->CaptureImagePointer(true);
			Camera.Reset();
		}
	}
	UpdateGizmoDrag(Pointer);
}

void FEditorPlugin::DrawGizmoOverlay()
{
	FSceneNodeView View;
	if (!Selection || !Scene->GetNodeView(*Selection, View) || !bViewportCameraInitialized || PreviewCamera ||
	    bOpenDialog || bSaveDialog || bDiscardDialog)
	{
		return;
	}
	FMat4 Parent = Identity();
	if (!View.Node->Parent().empty())
	{
		FSceneNodeView ParentView;
		if (!Scene->GetNodeView(Scene->FindHandle(View.Node->Parent()), ParentView))
		{
			return;
		}
		Parent = ParentView.World;
	}
	const auto Bounds = ViewportRegion.Bounds;
	const auto Pointer = Gui->PointerState();
	FTransformGizmo Display;
	Display.Configure(ViewCamera, Bounds, Scene->FindNode(*Selection)->Local(), Parent, GizmoMode,
	                  Gui->ApplicationScale());
	const auto Hovered = Gizmo.IsDragging()        ? Gizmo.ActiveHandle()
	                     : ViewportRegion.bHovered ? Display.HitTest(Pointer.Position)
	                                               : ETransformGizmoHandle::None;
	// Freeze the drag basis across zero/mirrored scales. Position handles follow the moving pivot.
	const auto Strokes = Gizmo.IsDragging() && GizmoMode != ETransformGizmoMode::Position ? Gizmo.Geometry(Hovered)
	                                                                                      : Display.Geometry(Hovered);
	for (const auto& Stroke : Strokes)
	{
		Gui->DrawImageOverlay(Bounds, Stroke.Points, Stroke.Color, Stroke.Thickness, Stroke.bFilled);
	}
}

void FEditorPlugin::UpdateGizmoDrag(const FGuiPointerState& InPointer)
{
	if (GizmoEdit)
	{
		bGizmoUsedMouse = true;
		FMat4 Local;
		if (Gizmo.Drag(InPointer.Position, Local) && Local.Values != Scene->FindNode(*Selection)->Local().Values)
		{
			try
			{
				auto Candidate = *Scene->FindNode(*Selection);
				Candidate.Local() = Local;
				if (!Scene->EditNode(*Selection, std::move(Candidate), GizmoEdit->Revision))
				{
					FinishGizmo();
					return;
				}
				GizmoEdit->Preview = Local;
				GizmoEdit->Revision = Scene->GetRevision();
			}
			catch (const std::exception& Failure)
			{
				FinishGizmo(true);
				Error = Failure.what();
			}
		}
		if (InPointer.bReleased || !InPointer.bDown)
		{
			FinishGizmo();
		}
	}
}
} // namespace Hyperion
