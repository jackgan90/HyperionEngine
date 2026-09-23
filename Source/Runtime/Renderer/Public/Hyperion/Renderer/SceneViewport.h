#pragma once
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
// Null fields are unsupported on this host. In a patch, null means retain the current value.
struct FSceneViewportOptions
{
	std::optional<float> Exposure;
	std::optional<bool> LightMarkers;
	std::optional<std::uint32_t> OutlineMode;
	std::optional<bool> SmoothOutlines;
	std::optional<std::uint32_t> Culling;
	std::optional<bool> Frozen;
	std::optional<bool> InstanceBatching;
	std::optional<bool> ModelBounds;
	std::optional<bool> LightBounds;
	std::optional<bool> Animate;
};

struct FSceneViewportState
{
	FSceneCameraView Camera;
	std::optional<FSceneHandle> PreviewCamera;
	FSceneViewportOptions Options;
	float MovementSpeed{};
	bool bReady{};
	bool bCameraAuthoring{};
};

class ISceneViewport
{
public:
	virtual ~ISceneViewport() = default;
	virtual FSceneViewportState ViewportState() const = 0;
	virtual void SetViewportCamera(const FSceneCameraView& InCamera) = 0;
	virtual void FrameScene() = 0;
	virtual void SetViewportOptions(const FSceneViewportOptions& InOptions) = 0;
	virtual void SetViewportSpeed(float InSpeed);
	virtual void PreviewSceneCamera(std::optional<FSceneHandle> InHandle);
	virtual void SaveInitialView();
	virtual void CreateViewCamera();
	virtual void ApplyViewToCamera(FSceneHandle InHandle);
};

void ValidateViewportOptions(const FSceneViewportOptions& InPatch, const FSceneViewportOptions& InSupported);
template<> const FRecordDescriptor& RecordType<FSceneViewportOptions>();
template<> const FRecordDescriptor& RecordType<FSceneViewportState>();
} // namespace Hyperion
