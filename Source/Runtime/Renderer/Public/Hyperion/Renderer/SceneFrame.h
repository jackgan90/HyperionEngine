#pragma once
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
class FSceneFrameSeed
{
public:
	FScenePublicationToken GetToken() const
	{
		return Token;
	}

private:
	FScenePublicationToken Token;
	std::shared_ptr<const FMaterialFrameContext> Frame;
	friend class FRenderSession;
};

struct FSceneViewRequest
{
	std::optional<FSceneHandle> Camera;
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	std::uint64_t Identity = 1;
	std::string Usage = "Forward";
	std::optional<FViewport> Viewport;
	EDepthConvention DepthConvention = EDepthConvention::Standard;
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	std::optional<FMat4> CullingViewProjection;
	bool bInstanceBatching = true;
	FMaterialParameterValues Parameters;
	FMaterialParameterValues PassParameters;
};

enum class ESceneCameraStatus : std::uint8_t
{
	Active,
	DefaultFallback,
	NoActiveCamera,
	EmptyViewport
};

struct FResolvedSceneFrame
{
	FRenderView View;
	std::shared_ptr<const FMaterialFrameContext> Frame;
	ESceneCameraStatus CameraStatus = ESceneCameraStatus::NoActiveCamera;
	std::optional<FSceneHandle> Camera;

	bool HasCamera() const
	{
		return CameraStatus == ESceneCameraStatus::Active || CameraStatus == ESceneCameraStatus::DefaultFallback;
	}
};
} // namespace Hyperion
