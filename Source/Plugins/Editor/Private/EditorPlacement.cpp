#include "EditorApplication.h"

namespace Hyperion
{
FPlacementCatalog FEditorPlugin::PlacementCatalog() const
{
	FPlacementCatalog Result;
	for (const auto* Object : PlacementRegistry.Search("All", {}))
	{
		Result.Items.push_back({Object->Id, Object->Label, Object->Categories, PlacementUnavailableReason(*Object)});
	}
	return Result;
}

std::optional<FSceneNodeInfo> FEditorPlugin::PlaceObject(const FScenePlacementRequest& InRequest)
{
	UpdateDocumentInteraction();
	SceneDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	const auto* Object = PlacementRegistry.Find(InRequest.Object);
	if (!Object || !IsFinite(InRequest.Position))
	{
		throw std::invalid_argument("A registered placeable ID and finite position are required");
	}
	if (Object->Model && !PlacementModels.contains(Object->Id))
	{
		PlacementModels[Object->Id].Asset = Scene->RegisterModelAsset(*Object->Model);
	}
	Scene->Tick();
	PollPlacementResources();
	const auto Unavailable = PlacementUnavailableReason(*Object);
	if (!Unavailable.empty())
	{
		if (Unavailable.starts_with("Preparing"))
		{
			return {};
		}
		throw FSceneEditError("load_failed", Unavailable);
	}
	CommitPlacement(*Object, InRequest.Position);
	return DescribeSceneNode(SceneDocument, {SceneDocument.Id(), *SceneDocument.Selection().Primary()});
}

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
		InspectionBounds["placement/title"] = Gui->LastItemBounds();
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
		// Window docking also uses GUI drag payloads; cancel only the gesture owned by placement.
		if (const auto Payload = Gui->DragPayload(); Payload && Payload->Type == PlacementPayload)
		{
			Gui->CancelDragDrop();
		}
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
	if (!IsFinite(InPosition) || !PlacementUnavailableReason(InObject).empty())
	{
		throw std::invalid_argument("Placement requires finite coordinates and prepared resources");
	}
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
