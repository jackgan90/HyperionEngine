#pragma once
#include "Hyperion/Scene/SceneCamera.h"

namespace Hyperion
{
struct FAssetPreviewSettings
{
	std::optional<FSceneCameraView> Camera;
	std::optional<std::uint32_t> Shape;
	std::optional<float> Exposure;
	std::optional<float> Yaw;
	std::optional<std::uint32_t> Mip;
	std::optional<std::uint32_t> Face;
	std::optional<std::uint32_t> Channel;
	std::optional<float> ExposureEv;
	std::optional<float> Zoom;
	std::optional<FVec2> Pan;
	std::optional<bool> Fit;
	std::optional<bool> Checker;
};

struct FAssetPreviewState
{
	std::string Document;
	std::uint64_t Generation{};
	FAssetPreviewSettings Settings;
	bool bReady{};
	std::string Error;
};

class IAssetPreviewWorkspace
{
public:
	virtual ~IAssetPreviewWorkspace() = default;
	virtual FAssetPreviewState PreviewState(std::string_view InDocument) const = 0;
	virtual FAssetPreviewState EditPreview(std::string_view InDocument, std::uint64_t InGeneration,
	                                       const FAssetPreviewSettings& InSettings, bool bInFrame) = 0;
};

template<> const FRecordDescriptor& RecordType<FAssetPreviewSettings>();
template<> const FRecordDescriptor& RecordType<FAssetPreviewState>();
} // namespace Hyperion
