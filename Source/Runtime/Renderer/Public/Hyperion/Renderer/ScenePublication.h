#pragma once
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
struct FScenePublicationToken
{
	std::uint64_t LogicalSceneIdentity{};
	std::uint64_t AttachmentEpoch{};
	std::uint64_t PublicationSerial{};
	std::uint64_t LogicalRevision{};
	bool operator==(const FScenePublicationToken&) const = default;
};

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

// CPU values only. No logical scene, editable material or application pointers cross this boundary.
struct FSceneMetadata
{
	FScenePublicationToken Token;
	FSceneSettings Settings;
	std::map<FSceneHandle, FPublishedSceneCamera> Cameras;
	std::map<FSceneHandle, FPublishedDirectionalLight> DirectionalLights;
	std::map<FSceneHandle, FPublishedEnvironmentLight> EnvironmentLights;
};
} // namespace Hyperion
