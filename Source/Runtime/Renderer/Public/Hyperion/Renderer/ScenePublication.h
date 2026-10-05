#pragma once
#include "Hyperion/RenderControls/ScenePublicationToken.h"
#include "Hyperion/Scene/LightSelection.h"
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{

struct FPublishedSceneCamera
{
	FSceneCamera Camera;
	FSceneCameraPose Pose;
	bool bEnabled{};
};

struct FPublishedDirectionalLight
{
	FSceneDirectionalLight Light;
	FVec3 SurfaceToLight;
	bool bEnabled{};
};

struct FPublishedEnvironmentLight
{
	FSceneEnvironmentLight Light;
	bool bEnabled{};
};

struct FPublishedPointLight
{
	FScenePointLight Light;
	FVec3 Position;
	bool bEnabled{};
};

struct FPublishedSpotLight
{
	FSceneSpotLight Light;
	FSceneCameraPose Pose;
	bool bEnabled{};
};

// CPU values only. No logical scene, editable material or application pointers cross this boundary.
struct FSceneMetadata
{
	FScenePublicationToken Token;
	std::uint64_t LocalLightRevision{};
	FSceneSettings Settings;
	FSceneLightingSelection Lighting;
	std::map<FSceneHandle, FPublishedSceneCamera> Cameras;
	std::map<FSceneHandle, FPublishedDirectionalLight> DirectionalLights;
	std::map<FSceneHandle, FPublishedEnvironmentLight> EnvironmentLights;
	std::map<FSceneHandle, FPublishedPointLight> PointLights;
	std::map<FSceneHandle, FPublishedSpotLight> SpotLights;
};
} // namespace Hyperion
