#pragma once
#include "Hyperion/Renderer/ViewportRay.h"
#include "Hyperion/Scene/SceneQuery.h"

namespace Hyperion
{
struct FViewportPlacement
{
	FVec3 Position;
	FVec3 Normal{0, 1, 0};
	bool bSurface{};
};

struct FViewportPlacementContext
{
	std::uint64_t Document{};
	std::uint64_t Revision{};
	FVec4 Bounds;
	FVec2 PixelScale;
	float ApplicationScale = 1;
	FSceneCameraView Camera;
	bool operator==(const FViewportPlacementContext& InOther) const;
};

// Host-independent gesture ownership. The host delivers a candidate only on a valid drop;
// leaving the target hides the preview without losing the source or changing document history.
class FViewportPlacementSession
{
public:
	void Begin(std::string InType, FViewportPlacementContext InContext);
	bool IsCurrent(std::string_view InType, const FViewportPlacementContext& InContext) const;
	void SetPreview(std::optional<FViewportPlacement> InPreview);
	void Cancel();
	bool IsActive() const;
	const std::string& GetType() const;
	const std::optional<FViewportPlacement>& GetPreview() const;

private:
	std::string Type;
	FViewportPlacementContext Context;
	std::optional<FViewportPlacement> Preview;
};

// Pure calculation. The caller supplies a current scene query and retains gesture/document ownership.
std::optional<FViewportPlacement> ResolveViewportPlacement(const FRay& InRay, const FSceneRayResult& InHit,
                                                           const FSceneCameraView& InCamera,
                                                           const FBounds& InLocalBounds);
std::optional<FVec3> ProjectViewportPoint(const FSceneCameraView& InCamera, FVec4 InBounds, FVec3 InWorld);
} // namespace Hyperion
