#include "EditorApplication.h"

namespace Hyperion
{
namespace
{
constexpr const char* PlacementPayload = "Hyperion.PlaceableObject.v1";
}

void FEditorPlugin::DrawPlacementPanel()
{
	if (!bShowPlacement)
	{
		return;
	}
	if (Gui->BeginWindow("Place Object", bShowPlacement))
	{
		if (bFocusPlacement)
		{
			Gui->FocusWindow("Place Object");
			bFocusPlacement = false;
		}
		Gui->Text("Search objects");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##SearchPlaceable", PlacementFilter, false);
		Gui->Tooltip("Search objects");
		std::vector<std::string> Categories{"All"};
		Categories.insert(Categories.end(), PlacementRegistry.GetCategories().begin(),
		                  PlacementRegistry.GetCategories().end());
		for (const auto& Category : Categories)
		{
			if (Gui->Button(Category.c_str()))
			{
				PlacementCategory = Category;
			}
			if (Category != Categories.back())
			{
				Gui->SameLineIfFits(Categories.back().c_str());
			}
		}
		Gui->Separator();
		Gui->Text(PlacementCategory);
		for (const auto* Object : PlacementRegistry.Search(PlacementCategory, PlacementFilter))
		{
			const auto Unavailable = PlacementUnavailableReason(*Object);
			Gui->BeginDisabled(!Unavailable.empty());
			if (const auto It = PlacementIcons.find(Object->Icon);
			    It != PlacementIcons.end() && It->second.Source.Texture)
			{
				Gui->Image(It->second.Texture, {24, 24});
				Gui->DragSource(PlacementPayload, Object->Id, Object->Label.c_str());
				Gui->SameLine();
			}
			Gui->Selectable((Object->Label + "##Place" + Object->Id).c_str(), false);
			InspectionBounds["placement/" + Object->Id] = Gui->LastItemBounds();
			Gui->DragSource(PlacementPayload, Object->Id, Object->Label.c_str());
			Gui->EndDisabled();
			Gui->Tooltip(Unavailable.empty() ? "Drag into the viewport to place" : Unavailable.c_str());
			if (!Unavailable.empty())
			{
				Gui->TextWrapped(Unavailable);
			}
		}
		Gui->Separator();
		Gui->TextWrapped(PlacementStatus.empty() ? "Drag an object into the viewport. Esc cancels." : PlacementStatus);
	}
	Gui->EndWindow();
}

void FEditorPlugin::CancelPlacement()
{
	Placement.Cancel();
	if (Gui)
	{
		Gui->CancelDragDrop();
	}
}

void FEditorPlugin::RoutePlacement()
{
	// The image must remain the last submitted item while the generic drop target is queried.
	const auto Drop = Gui->DropTarget(PlacementPayload);
	const auto Payload = Drop ? Drop : Gui->DragPayload();
	bPlacementUsedMouse = Placement.IsActive() || (Payload && Payload->Type == PlacementPayload);
	if (!Payload || Payload->Type != PlacementPayload)
	{
		Placement.Cancel();
		return;
	}
	ViewportClick.reset();
	Camera.Reset();
	bCameraDragging = false;
	const auto CameraView = PickingCamera();
	const auto Pointer = Gui->PointerState();
	const auto* Object = PlacementRegistry.Find(Payload->Value);
	if (!Object || !CameraView || !bViewportVisible || bOpenDialog || bSaveDialog || bAssetMessage || PendingRoot ||
	    bDiscardDialog || bViewOptionsOpen || Pointer.bCancel || Pointer.bRightDown || Gizmo.IsDragging())
	{
		CancelPlacement();
		return;
	}
	const auto Logical = Window->LogicalSize();
	const auto Pixels = Window->PixelSize();
	const FViewportPlacementContext PlacementContext{
	    DocumentEpoch,           Scene->GetRevision(),
	    ViewportRegion.Bounds,   {float(Pixels.Width) / Logical.Width, float(Pixels.Height) / Logical.Height},
	    Gui->ApplicationScale(), *CameraView};
	if (!Placement.IsActive())
	{
		FinishInspectorEdit();
		Placement.Begin(Object->Id, PlacementContext);
	}
	if (!Placement.IsCurrent(Object->Id, PlacementContext))
	{
		CancelPlacement();
		return;
	}
	Placement.SetPreview({});
	if (!Drop || !Pointer.bPositionValid)
	{
		if (Pointer.bReleased || !Pointer.bDown)
		{
			CancelPlacement();
		}
		return;
	}
	PlacementStatus = PlacementUnavailableReason(*Object);
	if (!PlacementStatus.empty())
	{
		if (Pointer.bReleased)
		{
			CancelPlacement();
		}
		return;
	}
	UpdatePlacementPreview(*Object, *CameraView, Pointer.Position);
	if (Drop->bDelivery)
	{
		if (Placement.GetPreview())
		{
			try
			{
				CommitPlacement(*Object, Placement.GetPreview()->Position);
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
		}
		CancelPlacement();
	}
}

void FEditorPlugin::UpdatePlacementPreview(const FPlaceableObject& InObject, const FSceneCameraView& InCamera,
                                           FVec2 InPointer)
{
	const FBounds Bounds = InObject.Model ? PlacementModels.at(InObject.Id).Data->Bounds : FBounds{};
	const auto& Region = ViewportRegion.Bounds;
	const FVec2 Position{(InPointer.X - Region.X) / (Region.Z - Region.X),
	                     (InPointer.Y - Region.Y) / (Region.W - Region.Y)};
	const auto Ray =
	    MakeViewportRay(InCamera, Position, ViewportSize.Width, ViewportSize.Height, EDepthConvention::Reversed);
	if (Ray)
	{
		const auto Hit = Scene->Raycast(*Ray, MakeSceneRayOptions(ESceneRenderPipeline::Deferred));
		Placement.SetPreview(ResolveViewportPlacement(*Ray, Hit, InCamera, Bounds));
	}
	PlacementStatus =
	    Placement.GetPreview() ? "Release to place. Esc cancels." : "Scene geometry is not ready for placement.";
}

void FEditorPlugin::CommitPlacement(const FPlaceableObject& InObject, FVec3 InPosition)
{
	auto Node = InObject.Create();
	Node.Name = InObject.Label;
	Node.Local().Values[12] = InPosition.X;
	Node.Local().Values[13] = InPosition.Y;
	Node.Local().Values[14] = InPosition.Z;
	if (InObject.Model)
	{
		Node.Model() = FSceneModelComponent{};
		Node.Model()->Asset = PlacementModels.at(InObject.Id).Asset;
	}
	const auto Preview = InObject.Model ? FreezePlacementPreview() : nullptr;
	const auto Local = Node.Local();
	const auto Handle = CommitCreate(std::move(Node));
	if (Preview)
	{
		PlacementPublication = Handle;
		PlacementPublicationLocal = Local;
		PlacementPublicationPreview = Preview;
	}
	PlacementStatus = "Placed " + InObject.Label;
}
} // namespace Hyperion
