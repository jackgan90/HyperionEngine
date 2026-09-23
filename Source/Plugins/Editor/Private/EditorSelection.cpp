#include "EditorApplication.h"

namespace Hyperion
{
namespace
{
bool HasVisibleGeometry(const FSceneModel* InModel)
{
	if (!InModel || !InModel->bVisible || !InModel->Data)
	{
		return false;
	}
	for (const auto& Instance : SceneModelInstances(*InModel))
	{
		const auto Id = ModelPrimitiveId(*InModel->Data->Asset, Instance.Primitive);
		const auto Section = std::find_if(InModel->Sections.begin(), InModel->Sections.end(),
		                                  [&](const auto& InSection)
		                                  {
			                                  return InSection.Primitive == Id;
		                                  });
		if (Section == InModel->Sections.end() || Section->bVisible)
		{
			return true;
		}
	}
	return false;
}
} // namespace

void FEditorPlugin::PruneSelection()
{
	auto Updated = Selection;
	for (const auto Handle : Selection.All())
	{
		if (!Scene->FindNode(Handle))
		{
			Updated.Toggle(Handle);
		}
	}
	if (Updated != Selection)
	{
		SetSelection(std::move(Updated));
	}
}

std::vector<FSceneHandle> FEditorPlugin::SelectedRoots() const
{
	return SceneDocument.SelectedRoots();
}

void FEditorPlugin::DrawSelectionMarkers()
{
	const auto ActiveCamera = PickingCamera();
	if (!ActiveCamera)
	{
		return;
	}
	for (const auto Handle : Selection.All())
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(Handle, View) || (View.bEffectiveEnabled && HasVisibleGeometry(Scene->Find(Handle))) ||
		    (bShowLightMarkers && LightTexture(*View.Node) && View.bEffectiveEnabled))
		{
			continue;
		}
		const FVec3 Origin{View.World.Values[12], View.World.Values[13], View.World.Values[14]};
		if (const auto Point = ProjectViewportPoint(*ActiveCamera, ViewportRegion.Bounds, Origin))
		{
			const float Radius = Gui->Scale(6);
			const std::array<FVec2, 5> Points{{{Point->X, Point->Y - Radius},
			                                   {Point->X + Radius, Point->Y},
			                                   {Point->X, Point->Y + Radius},
			                                   {Point->X - Radius, Point->Y},
			                                   {Point->X, Point->Y - Radius}}};
			Gui->DrawImageOverlay(ViewportRegion.Bounds, Points, {1, .7f, .1f, 1}, 2);
		}
	}
}
} // namespace Hyperion
