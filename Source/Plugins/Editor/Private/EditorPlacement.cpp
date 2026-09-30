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
	return PollPlacement(FPlacementService::Candidate(*Object), InRequest.Position);
}

std::optional<FSceneNodeInfo> FEditorPlugin::PlaceModel(const FSceneModelPlacementRequest& InRequest)
{
	UpdateDocumentInteraction();
	SceneDocument.RequireIdle(InRequest.Document, InRequest.Revision);
	if (!IsFinite(InRequest.Position))
	{
		throw std::invalid_argument("Placement requires a finite position");
	}
	return PollPlacement(FPlacementService::ModelCandidate(InRequest.Model, Assets), InRequest.Position);
}

std::optional<FSceneNodeInfo> FEditorPlugin::PollPlacement(const FPlacementCandidate& InCandidate, FVec3 InPosition)
{
	const auto Document = SceneDocument.Id();
	const auto Revision = Scene->GetRevision();
	Scene->Tick();
	// Publishing completed scene work can advance the revision after the request's admission check.
	SceneDocument.RequireIdle(Document, Revision);
	PlacementService.Prepare(InCandidate, *Scene);
	PollPlacementResources();
	const auto Unavailable = PlacementUnavailableReason(InCandidate);
	if (!Unavailable.empty())
	{
		if (Unavailable.starts_with("Preparing"))
		{
			return {};
		}
		throw FSceneEditError("load_failed", Unavailable);
	}
	CommitPlacement(InCandidate, InPosition);
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
		if (const auto Payload = Gui->DragPayload();
		    Payload && (Payload->Type == PlacementPayload ||
		                (Payload->Type == PlacementSourceType && Payload->Value == PlacementSourceValue)))
		{
			Gui->CancelDragDrop();
		}
	}
	PlacementCandidate.reset();
	PlacementSourceType.clear();
	PlacementSourceValue.clear();
}

void FEditorPlugin::RoutePlacement()
{
	// The image must remain the last submitted item while the generic drop target is queried.
	const auto Payload = Gui->DragPayload();
	const bool bSupported = Payload && (Payload->Type == PlacementPayload || Payload->Type == AssetPathPayloadType);
	bPlacementUsedMouse = Placement.IsActive() || bSupported;
	if (!bSupported)
	{
		CancelPlacement();
		return;
	}
	const auto Drop = Gui->DropTarget(Payload->Type.c_str());
	// A content drag belongs to placement only after entering this viewport target.
	if (!Drop && !Placement.IsActive())
	{
		return;
	}
	try
	{
		RoutePlacementPayload(*Payload, Drop);
	}
	catch (const std::exception& Failure)
	{
		PlacementStatus = Failure.what();
		Placement.SetPreview({});
		if (Drop)
		{
			Gui->DrawDropFeedback(false, PlacementStatus.c_str());
		}
		if (Payload->bDelivery || Gui->PointerState().bReleased)
		{
			CancelPlacement();
		}
	}
}

FPlacementCandidate FEditorPlugin::ResolvePlacementPayload(const FGuiDragPayload& InPayload)
{
	if (InPayload.Type == PlacementPayload)
	{
		const auto* Object = PlacementRegistry.Find(InPayload.Value);
		if (!Object)
		{
			throw std::invalid_argument("Unknown placeable object");
		}
		return FPlacementService::Candidate(*Object);
	}
	for (auto Reference : Assets.GetAssetIndex())
	{
		if (Reference.Path == InPayload.Value)
		{
			Reference.Revision.clear();
			return FPlacementService::ModelCandidate(std::move(Reference), Assets);
		}
	}
	throw std::invalid_argument("Asset is unavailable; refresh Content Browser and try again");
}

void FEditorPlugin::RoutePlacementPayload(const FGuiDragPayload& InPayload,
                                          const std::optional<FGuiDragPayload>& InDrop)
{
	ViewportClick.reset();
	Camera.Reset();
	Viewport.bCameraDragging = false;
	const auto CameraView = PickingCamera();
	const auto Pointer = Gui->PointerState();
	if (!CameraView || !Viewport.bViewportVisible || bOpenDialog || bSaveDialog || bAssetMessage ||
	    Transition.PendingRoot || Transition.bDiscardDialog || bPreferencesDialog || bViewOptionsOpen ||
	    Pointer.bCancel || Pointer.bRightDown || Gizmo.IsDragging())
	{
		CancelPlacement();
		return;
	}
	const auto Logical = Window->LogicalSize();
	const auto Pixels = Window->PixelSize();
	const FViewportPlacementContext PlacementContext{
	    DocumentEpoch,
	    Scene->GetRevision(),
	    Viewport.ViewportRegion.Bounds,
	    {float(Pixels.Width) / Logical.Width, float(Pixels.Height) / Logical.Height},
	    Gui->ApplicationScale(),
	    *CameraView};
	if (!Placement.IsActive())
	{
		FinishInspectorEdit();
		PlacementCandidate = ResolvePlacementPayload(InPayload);
		PlacementSourceType = InPayload.Type;
		PlacementSourceValue = InPayload.Value;
		Placement.Begin(PlacementCandidate->Id, PlacementContext);
	}
	if (InPayload.Type != PlacementSourceType || InPayload.Value != PlacementSourceValue ||
	    !Placement.IsCurrent(PlacementCandidate->Id, PlacementContext))
	{
		CancelPlacement();
		return;
	}
	Placement.SetPreview({});
	if (!InDrop || !Pointer.bPositionValid)
	{
		if (Pointer.bReleased || !Pointer.bDown)
		{
			CancelPlacement();
		}
		return;
	}
	PlacementService.Prepare(*PlacementCandidate, *Scene);
	PlacementStatus = PlacementUnavailableReason(*PlacementCandidate);
	if (!PlacementStatus.empty())
	{
		Gui->DrawDropFeedback(false, PlacementStatus.c_str());
		if (Pointer.bReleased)
		{
			CancelPlacement();
		}
		return;
	}
	UpdatePlacementPreview(*PlacementCandidate, *CameraView, Pointer.Position);
	Gui->DrawDropFeedback(Placement.GetPreview().has_value(), PlacementStatus.c_str());
	if (InDrop->bDelivery)
	{
		if (Placement.GetPreview())
		{
			try
			{
				CommitPlacement(*PlacementCandidate, Placement.GetPreview()->Position);
				// The selected object is now in the viewport; subsequent scene shortcuts belong there.
				Gui->FocusWindow("Viewport");
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
		}
		CancelPlacement();
	}
}

void FEditorPlugin::UpdatePlacementPreview(const FPlacementCandidate& InObject, const FSceneCameraView& InCamera,
                                           FVec2 InPointer)
{
	const FBounds Bounds = InObject.Model ? PlacementModels.at(InObject.Id).Data->Bounds : FBounds{};
	const auto& Region = Viewport.ViewportRegion.Bounds;
	const FVec2 Position{(InPointer.X - Region.X) / (Region.Z - Region.X),
	                     (InPointer.Y - Region.Y) / (Region.W - Region.Y)};
	const auto Ray = MakeViewportRay(InCamera, Position, Viewport.ViewportSize.Width, Viewport.ViewportSize.Height,
	                                 GetDepthConvention(Rendering.bReversedZ));
	if (Ray)
	{
		const auto Hit = Scene->Raycast(*Ray, MakeSceneRayOptions(MakePipelineSettings(Rendering).Pipeline));
		Placement.SetPreview(ResolveViewportPlacement(*Ray, Hit, InCamera, Bounds));
	}
	PlacementStatus =
	    Placement.GetPreview() ? "Release to place. Esc cancels." : "Scene geometry is not ready for placement.";
}

void FEditorPlugin::CommitPlacement(const FPlaceableObject& InObject, FVec3 InPosition)
{
	CommitPlacement(FPlacementService::Candidate(InObject), InPosition);
}

void FEditorPlugin::CommitPlacement(const FPlacementCandidate& InObject, FVec3 InPosition)
{
	if (!IsFinite(InPosition) || !PlacementUnavailableReason(InObject).empty())
	{
		throw std::invalid_argument("Placement requires finite coordinates and prepared resources");
	}
	const auto Preview = InObject.Model && Placement.GetType() == InObject.Id && Placement.GetPreview()
	                         ? FreezePlacementPreview()
	                         : nullptr;
	FinishInspectorEdit();
	const auto Handle = PlacementService.Commit(InObject, InPosition, SceneDocument);
	bSelectionInitialized = true;
	Error.clear();
	if (Preview)
	{
		PlacementPublication = Handle;
		PlacementPublicationLocal = Scene->FindNode(Handle)->Local();
		PlacementPublicationRevision = Scene->GetRevision();
		bPlacementPublicationSourceMaterials = InObject.bPreferModelMaterials;
		PlacementPublicationPreview = Preview;
	}
	PlacementStatus = "Placed " + InObject.Label;
}
} // namespace Hyperion
