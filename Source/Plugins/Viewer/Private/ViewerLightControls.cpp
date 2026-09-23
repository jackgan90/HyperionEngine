#include "Hyperion/SceneEditing/SceneDocument.h"
#include "ViewerApplication.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
FSceneMainLight FViewerPlugin::MainLight()
{
	auto* Scene = GetSceneInstance();
	FSceneNodeView View;
	const auto Handle = Scene ? Scene->GetSettings().MainDirectionalLight : std::optional<FSceneHandle>{};
	if (!Handle || !Scene->GetNodeView(*Handle, View) || !View.Node->DirectionalLight())
	{
		throw FSceneEditError("unavailable", "No main directional light selected");
	}
	return {Scene->GetRevision(), *Handle, *View.Node->DirectionalLight(),
	        ScaleVector(ExtractScenePose(View.World).Forward, -1)};
}

void FViewerPlugin::SetMainLight(const FSceneMainLight& InLight)
{
	const auto Current = MainLight();
	if (Current.Handle != InLight.Handle || Current.Revision != InLight.Revision)
	{
		throw FSceneEditError("stale_revision", "Main light changed; read light.main.get again");
	}
	if (!IsFinite(InLight.Direction) || !std::isfinite(Length(InLight.Direction)) || Length(InLight.Direction) < .0001f)
	{
		throw std::invalid_argument("Light direction must be finite and nonzero");
	}
	auto& Scene = *GetSceneInstance();
	FSceneNodeView View;
	Scene.GetNodeView(Current.Handle, View);
	auto Node = *View.Node;
	Node.DirectionalLight() = InLight.Light;
	if (InLight.Direction.X != Current.Direction.X || InLight.Direction.Y != Current.Direction.Y ||
	    InLight.Direction.Z != Current.Direction.Z)
	{
		const auto Pose = ExtractScenePose(View.World);
		const auto Forward = ScaleVector(Normalize(InLight.Direction), -1);
		const FVec3 Up = std::abs(Forward.Y) > .999f ? FVec3{0, 0, 1} : FVec3{0, 1, 0};
		FSceneNodeView Parent;
		const auto ParentWorld = Scene.GetNodeView(Scene.FindHandle(Node.Parent()), Parent) ? Parent.World : Identity();
		Node.Local() = Multiply(Inverse(ParentWorld), SceneCameraTransform(Pose.Eye, Add(Pose.Eye, Forward), Up));
	}
	Scene.EditNodes({{Current.Handle, std::move(Node)}}, Current.Revision);
}

void FViewerPlugin::DrawMainLightGui()
{
	try
	{
		auto Light = MainLight();
		bool bChanged = Gui->InputVector("Main light color", Light.Light.Color);
		bChanged |= Gui->InputFloat("Main light intensity", Light.Light.Intensity);
		bChanged |= Gui->Checkbox("Main light casts shadows", Light.Light.bCastShadows);
		float AzimuthDegrees = std::atan2(Light.Direction.X, Light.Direction.Z) * 180 / 3.14159265f;
		float ElevationDegrees = std::asin(std::clamp(Light.Direction.Y, -1.f, 1.f)) * 180 / 3.14159265f;
		bool bDirectionChanged = Gui->Slider("Light azimuth", AzimuthDegrees, -180, 180);
		bDirectionChanged |= Gui->Slider("Light elevation", ElevationDegrees, -90, 90);
		if (bDirectionChanged)
		{
			const float Azimuth = AzimuthDegrees * 3.14159265f / 180;
			const float Elevation = ElevationDegrees * 3.14159265f / 180;
			Light.Direction = {std::sin(Azimuth) * std::cos(Elevation), std::sin(Elevation),
			                   std::cos(Azimuth) * std::cos(Elevation)};
		}
		if (bChanged || bDirectionChanged)
		{
			SetMainLight(Light);
		}
	}
	catch (const std::exception& Failure)
	{
		Gui->TextWrapped(Failure.what());
	}
}
} // namespace Hyperion
