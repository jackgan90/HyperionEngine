#pragma once
#include "Hyperion/Renderer/RenderScene.h"

namespace Hyperion
{
struct FCascadedShadowSettings
{
	bool bEnabled = true;
	std::uint32_t Resolution = 2048;
	float Distance = 100;
	float SplitLambda = .6f;
	float NormalOffset = .6f;  // World texels, clamped to [0, 2].
	float ReceiverBias = .15f; // World texels, clamped to [0, 2].
	float BlendFraction = .1f;
	float FadeFraction = .1f;
	std::uint32_t DebugMode{};                // 0 shaded, 1 cascade color, 2..5 depth previews.
	std::optional<FViewport> PreviewViewport; // Host may reserve space beside its UI; pixel coordinates.
};

struct FShadowCascade
{
	FMat4 ViewProjection = Identity();
	float Near{};
	float Far{};
	float WorldTexel{};
	float DepthRange = 1;
	FVec3 Center;
	FVec3 Eye;
	float Radius{};
	std::size_t CandidateCasters{};
};

// Defaults are initialized depth=1 and zero shadow strength; ordinary material clients need no pipeline.
FMaterialParameterValues DefaultShadowParameters();

class FCascadedShadowMap
{
public:
	using FBoundsQuery = std::function<std::vector<FBounds>(const ISceneVisibility&)>;
	FCascadedShadowMap();
	bool Prepare(const FRenderView& InMain, FVec3 InSurfaceToLight, const FCascadedShadowSettings& InSettings,
	             const FBoundsQuery& InQuery, std::optional<std::array<std::uint64_t, 2>> InSceneState = {});
	std::vector<FRenderView> Views(const FRenderView& InMain) const;
	std::vector<FRenderPassTargets> Targets(std::shared_ptr<const void> InLifetime) const;
	void Bind(FRenderView& InMain, FRenderPassTargets& InTargets, std::shared_ptr<const void> InLifetime) const;
	const std::array<FShadowCascade, 4>& Cascades() const;
	std::uint64_t TextureBytes() const;
	bool IsEnabled() const;

private:
	std::array<FShadowCascade, 4> Data;
	std::array<std::shared_ptr<const FMaterialTextureSource>, 4> Textures;
	std::array<std::uint64_t, 4> ViewIds;
	FCascadedShadowSettings Settings;
	FVec3 LightRight{1, 0, 0};
	FVec3 LightUp{0, 1, 0};
	FVec3 Light{0, 0, 1};
	bool bEnabled{};
	std::optional<std::array<std::uint64_t, 26>> PreparedKey;
	void PrepareCascade(std::size_t InIndex, const FRenderView& InMain, const FBoundsQuery& InQuery);
};
} // namespace Hyperion
