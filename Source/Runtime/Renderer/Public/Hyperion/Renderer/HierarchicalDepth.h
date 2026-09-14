#pragma once
#include "Hyperion/Renderer/ComputePass.h"
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
enum class EDepthReduction : std::uint8_t
{
	Nearest,
	Farthest
};

struct FHierarchicalDepthRequest
{
	FRenderTargetSource Depth;
	FRenderView View;
	EDepthReduction Reduction = EDepthReduction::Nearest;
};

struct FHierarchicalDepthProduct
{
	std::shared_ptr<const FMaterialTextureSource> Texture;
	std::shared_ptr<const void> Lifetime;
	std::vector<FSize> MipSizes;
	EDepthConvention Convention = EDepthConvention::Standard;
	EDepthReduction Reduction = EDepthReduction::Nearest;
	std::uint64_t Bytes{};
	std::uint64_t GraphIdentity{};
	std::uint64_t SourceIdentity{};
	std::uint64_t ViewIdentity{};
	std::uint64_t ViewRevision{};
	FMat4 ViewProjection;
	FVec3 Eye;
	FViewport Viewport;

	void Validate(const FRenderGraph& InGraph, const FRenderView& InView) const;
};

struct FHierarchicalDepthStats
{
	std::uint32_t Consumers{};
	std::uint32_t Products{};
	std::uint32_t Dispatches{};
	std::uint64_t Bytes{};
};

// Render-owned demand producer. No effect switches or persistent consumer counters live here.
class FHierarchicalDepthProducer
{
public:
	void BeginFrame(const FRenderGraph& InGraph);
	FHierarchicalDepthProduct Request(FRenderSession& InSession, FRenderGraph& InGraph,
	                                  const FHierarchicalDepthRequest& InRequest, bool bInDeferPreparation = true);
	void EndFrame(); // Retire cached generations with no requests in the current graph.

	FHierarchicalDepthStats Statistics() const
	{
		return Stats;
	}

private:
	struct FEntry
	{
		std::shared_ptr<const FMaterialTextureSource> Source;
		std::uint64_t View{};
		FHierarchicalDepthProduct Product;
	};

	std::vector<FEntry> Entries;
	std::uint64_t Graph{};
	FHierarchicalDepthStats Stats;
};
} // namespace Hyperion
