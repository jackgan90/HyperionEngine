#pragma once
#include "Hyperion/Renderer/RenderGraph.h"

namespace Hyperion
{
struct FGraphContent
{
	bool bFull{};
	std::vector<FViewport> Regions;
	void Load(EAttachmentLoad InLoad, const std::optional<FViewport>& InRegion, FSize InSize);
	void Store(EAttachmentStore InStore, const std::optional<FViewport>& InRegion, FSize InSize);
	bool Contains(const std::optional<FViewport>& InRegion, FSize InSize) const;
	void Invalidate(const std::optional<FViewport>& InRegion, FSize InSize);
};

struct FGraphResourceState
{
	FRenderTarget Target;
	EResourceState State;
	FGraphContent Color;
	FGraphContent Depth;
	FGraphContent Stencil;
};

void ValidateGraphImport(const FGraphTextureImport& InResource);
void ValidateGraphPass(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                       std::uint64_t InGraph);
void ValidateGraphState(const FGraphTextureImport& InResource, EResourceState InState);
std::vector<FGraphResourceState> ResolveGraphResources(std::span<const FGraphTextureImport> InResources);
void ApplyGraphPass(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                    std::vector<FGraphResourceState>& InStates, FPassCommands& OutCommands);
void FinishGraphAttachments(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                            std::vector<FGraphResourceState>& InStates);
void TransitionGraphResource(FGraphResourceState& InResource, EResourceState InState, FPassCommands& OutCommands);
} // namespace Hyperion
