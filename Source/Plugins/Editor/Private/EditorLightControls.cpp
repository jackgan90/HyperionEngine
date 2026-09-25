#include "EditorApplication.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
FSceneMainLight FEditorPlugin::MainLight()
{
	FSceneNodeView View;
	const auto Handle = Scene ? Scene->GetSettings().MainDirectionalLight : std::optional<FSceneHandle>{};
	if (!Handle || !Scene->GetNodeView(*Handle, View) || !View.Node->DirectionalLight())
	{
		throw FSceneEditError("unavailable", "No main directional light selected");
	}
	return {Scene->GetRevision(), *Handle, *View.Node->DirectionalLight(),
	        ScaleVector(ExtractScenePose(View.World).Forward, -1)};
}

void FEditorPlugin::SetMainLight(const FSceneMainLight& InLight)
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
	SceneDocument.RequireIdle(SceneDocument.Id(), InLight.Revision);
	FSceneNodeView View;
	Scene->GetNodeView(Current.Handle, View);
	auto Node = *View.Node;
	Node.DirectionalLight() = InLight.Light;
	if (InLight.Direction.X != Current.Direction.X || InLight.Direction.Y != Current.Direction.Y ||
	    InLight.Direction.Z != Current.Direction.Z)
	{
		const auto Pose = ExtractScenePose(View.World);
		const auto Forward = ScaleVector(Normalize(InLight.Direction), -1);
		const FVec3 Up = std::abs(Forward.Y) > .999f ? FVec3{0, 0, 1} : FVec3{0, 1, 0};
		FSceneNodeView Parent;
		const auto ParentWorld =
		    Scene->GetNodeView(Scene->FindHandle(Node.Parent()), Parent) ? Parent.World : Identity();
		Node.Local() = Multiply(Inverse(ParentWorld), SceneCameraTransform(Pose.Eye, Add(Pose.Eye, Forward), Up));
	}
	SceneDocument.CommitEdits({{Current.Handle, std::move(Node)}}, Current.Revision);
}

} // namespace Hyperion
