#include "EditorApplication.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
FVec3 Origin(const FMat4& InWorld)
{
	return {InWorld.Values[12], InWorld.Values[13], InWorld.Values[14]};
}

bool Contains(FVec4 InBounds, FVec2 InPoint)
{
	return InPoint.X >= InBounds.X && InPoint.X < InBounds.Z && InPoint.Y >= InBounds.Y && InPoint.Y < InBounds.W;
}
} // namespace

std::uint64_t FEditorPlugin::LightTexture(const FSceneNode& InNode) const
{
	const char* Id = InNode.DirectionalLight() ? "DirectionalLight"
	                 : InNode.PointLight()     ? "PointLight"
	                 : InNode.SpotLight()      ? "SpotLight"
	                                           : "";
	const auto It = PlacementIcons.find(Id);
	return It != PlacementIcons.end() && It->second.Source.Texture ? It->second.Texture : 0;
}

void FEditorPlugin::DrawLightMarker(const FSceneNode& InNode, const FMat4& InWorld, bool bInSelected)
{
	const auto CameraView = PickingCamera();
	const auto Texture = LightTexture(InNode);
	if (!CameraView || !Texture)
	{
		return;
	}
	const auto Point = ProjectViewportPoint(*CameraView, ViewportRegion.Bounds, Origin(InWorld));
	if (!Point)
	{
		return;
	}
	const float Half = Gui->Scale(24);
	const FVec4 Bounds{Point->X - Half, Point->Y - Half, Point->X + Half, Point->Y + Half};
	Gui->DrawImageOverlay(Texture, ViewportRegion.Bounds, Bounds,
	                      bInSelected ? FVec4{1, .85f, .45f, 1} : FVec4{1, 1, 1, 1});
	if (InNode.DirectionalLight() || InNode.SpotLight())
	{
		const auto Direction = ExtractScenePose(InWorld).Forward;
		const auto End = ProjectViewportPoint(*CameraView, ViewportRegion.Bounds, Add(Origin(InWorld), Direction));
		if (End)
		{
			const float X = End->X - Point->X;
			const float Y = End->Y - Point->Y;
			const float Length = std::sqrt(X * X + Y * Y);
			if (Length > 1)
			{
				const FVec2 Delta{X / Length, Y / Length};
				const FVec2 Tip{Point->X + Delta.X * Half * 2.4f, Point->Y + Delta.Y * Half * 2.4f};
				const float HeadLength = Gui->Scale(9);
				const float HeadWidth = Gui->Scale(4);
				const std::array Line{FVec2{Point->X + Delta.X * Half, Point->Y + Delta.Y * Half}, Tip};
				const std::array Head{FVec2{Tip.X - Delta.X * HeadLength - Delta.Y * HeadWidth,
				                            Tip.Y - Delta.Y * HeadLength + Delta.X * HeadWidth},
				                      Tip,
				                      FVec2{Tip.X - Delta.X * HeadLength + Delta.Y * HeadWidth,
				                            Tip.Y - Delta.Y * HeadLength - Delta.X * HeadWidth}};
				Gui->DrawImageOverlay(ViewportRegion.Bounds, Line, {1, .95f, .8f, 1}, 2);
				Gui->DrawImageOverlay(ViewportRegion.Bounds, Head, {1, .95f, .8f, 1}, 2);
			}
		}
	}
}

std::vector<FEditorPlugin::FProjectedLightMarker> FEditorPlugin::CollectLightMarkers() const
{
	std::vector<FProjectedLightMarker> Result;
	const auto CameraView = PickingCamera();
	if (!bShowLightMarkers || !CameraView)
	{
		return Result;
	}
	const float Half = Gui->Scale(24);
	const auto Handles = Scene->GetNodes();
	// Draw far-to-near, with the earliest scene entry on top at equal depth. Picking reverses this same list.
	for (auto It = Handles.rbegin(); It != Handles.rend(); ++It)
	{
		FSceneNodeView View;
		if (!Scene->GetNodeView(*It, View) || !View.bEffectiveEnabled || !LightTexture(*View.Node))
		{
			continue;
		}
		if (const auto Point = ProjectViewportPoint(*CameraView, ViewportRegion.Bounds, Origin(View.World)))
		{
			Result.push_back({View, {Point->X - Half, Point->Y - Half, Point->X + Half, Point->Y + Half}, Point->Z});
		}
	}
	std::stable_sort(Result.begin(), Result.end(),
	                 [](const auto& InA, const auto& InB)
	                 {
		                 return InA.Depth > InB.Depth;
	                 });
	return Result;
}

void FEditorPlugin::DrawLightMarkers()
{
	for (const auto& Marker : CollectLightMarkers())
	{
		DrawLightMarker(*Marker.View.Node, Marker.View.World, Selection == Marker.View.Handle);
	}
	if (Placement.GetPreview())
	{
		if (const auto* Object = PlacementRegistry.Find(Placement.GetType()); Object && !Object->Icon.empty())
		{
			auto Node = Object->Create();
			Node.Local().Values[12] = Placement.GetPreview()->Position.X;
			Node.Local().Values[13] = Placement.GetPreview()->Position.Y;
			Node.Local().Values[14] = Placement.GetPreview()->Position.Z;
			DrawLightMarker(Node, Node.Local(), true);
		}
	}
}

std::optional<FSceneHandle> FEditorPlugin::PickLightMarker(FVec2 InPoint) const
{
	if (!Contains(ViewportRegion.Bounds, InPoint))
	{
		return {};
	}
	const auto Markers = CollectLightMarkers();
	for (auto It = Markers.rbegin(); It != Markers.rend(); ++It)
	{
		if (Contains(It->Bounds, InPoint))
		{
			return It->View.Handle;
		}
	}
	return {};
}

void FEditorPlugin::DrawMainLightAction(FSceneHandle InHandle)
{
	const bool bMain = Scene->GetSettings().MainDirectionalLight == InHandle;
	Gui->TextWrapped(bMain ? "Main directional light" : "Additional directional light (not used for scene lighting)");
	if (Gui->Button("Set as main directional light", !bMain))
	{
		auto Settings = Scene->GetSettings();
		Settings.MainDirectionalLight = InHandle;
		CommitSettings(std::move(Settings));
	}
}
} // namespace Hyperion
