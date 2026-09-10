#pragma once
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/Renderer/RenderGraph.h"

namespace Hyperion
{
struct FRenderTargetSource
{
	ERenderTargetKind Kind = ERenderTargetKind::None;
	std::shared_ptr<const FMaterialTextureSource> Texture;
	std::shared_ptr<const void> Lifetime;
	bool bInitialized = true;
	bool operator==(const FRenderTargetSource&) const = default;
};

struct FRenderColorTarget
{
	FRenderTargetSource Source;
	FAttachmentActions Actions;
	FVec4 Clear;
	EGraphColorView View = EGraphColorView::DrawBatch;
	bool operator==(const FRenderColorTarget& InOther) const;
};

struct FRenderDepthTarget
{
	FRenderTargetSource Source;
	ERHIDepthFormat Format = ERHIDepthFormat::None;
	std::optional<FAttachmentActions> Depth;
	std::optional<FAttachmentActions> Stencil;
	float ClearDepth = 1;
	std::uint8_t ClearStencil{};
	bool operator==(const FRenderDepthTarget&) const = default;
};

// Rendering outputs are separate from camera/culling data and reusable across graph instances.
struct FRenderPassTargets
{
	std::string Name;
	std::optional<FRenderColorTarget> Color;
	std::optional<FRenderDepthTarget> DepthStencil;
	std::vector<FRenderTargetSource> Reads;
	static FRenderPassTargets Frame(ERHIDepthFormat InDepth, std::optional<FVec4> InClear = {});
	static FRenderPassTargets ColorOnly(std::optional<FVec4> InClear = {});
	ERHIDepthFormat GetDepthFormat() const;
	std::uint32_t ColorCount() const;
	bool operator==(const FRenderPassTargets&) const = default;
};
} // namespace Hyperion
