#include "Hyperion/Scene/Scene.h"

namespace Hyperion
{
FSceneNode MakeSceneCameraNode(std::string InId, FVec3 InEye, FVec3 InTarget, FSceneCamera InCamera)
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.Name = Node.Id;
	Node.Local = SceneCameraTransform(InEye, InTarget);
	InCamera.FocusDistance = Length(Subtract(InTarget, InEye));
	ValidateSceneCamera(InCamera);
	Node.Camera = InCamera;
	return Node;
}

FSceneNode MakeSceneDirectionalLightNode(std::string InId)
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.Name = Node.Id;
	Node.Local = SceneCameraTransform({}, {.45f, -.8f, -.65f});
	Node.DirectionalLight = FSceneDirectionalLight{{3.f, 2.85f, 2.7f}, 1.f, true};
	return Node;
}

FSceneNode MakeSceneEnvironmentLightNode(std::string InId)
{
	FSceneNode Node;
	Node.Id = std::move(InId);
	Node.Name = Node.Id;
	Node.EnvironmentLight = FSceneEnvironmentLight{{.22f, .25f, .3f}, 1.f};
	return Node;
}

std::array<FSceneHandle, 3> AddDefaultSceneContent(FScene& InScene, FVec3 InEye, FVec3 InTarget, FSceneCamera InCamera)
{
	std::array<FSceneHandle, 3> Handles;
	Handles[0] = InScene.AddNode(MakeSceneCameraNode({}, InEye, InTarget, InCamera));
	Handles[1] = InScene.AddNode(MakeSceneDirectionalLightNode({}));
	Handles[2] = InScene.AddNode(MakeSceneEnvironmentLightNode({}));
	InScene.SetSettings({Handles[0], Handles[1], Handles[2]});
	return Handles;
}
} // namespace Hyperion
