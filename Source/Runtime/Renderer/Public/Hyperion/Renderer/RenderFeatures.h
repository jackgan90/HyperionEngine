#pragma once
#include "Hyperion/Renderer/ContactShadows.h"
#include "Hyperion/Renderer/RenderPipelineFrame.h"

namespace Hyperion
{
struct FScenePipelineSettings;
struct FTransientGeometry;
struct FSelectionOutlineRequest;
enum class ERenderFeatureStage
{
	AfterOpaque,
	BeforeLighting,
	BeforeTonemap,
	AfterTonemap
};

struct FRenderFeatureResources
{
	FRenderTargetSource Depth;
	FRenderTargetSource Color;
	std::array<FRenderTargetSource, 4> GBuffer;
	FRenderTargetSource Output;
	// Optional lighting input. The producer supplies the exact source and its retention token.
	FRenderTargetSource DirectionalVisibility;
};

// Render-only, synchronous frame context. Contributions capture owned snapshots in deferred passes,
// never this context or its borrowed references. All GPU reads/writes go through RenderGraph.
struct FRenderFeatureContext
{
	FRenderSession& Session;
	FRenderGraph& Graph;
	const FRenderView& View;
	const FMaterialFrameContext& Frame;
	const FScenePipelineSettings& Settings;
	FRenderFeatureResources& Resources;
	FForwardPipelineStatistics& Statistics;
	std::shared_ptr<FFullscreenPreparationStatistics> FullscreenStatistics;
	bool bDeferPreparation{};
	std::shared_ptr<const FMaterialFrameContext> FrameOwner;
	std::shared_ptr<const FTransientGeometry> TransientGeometry;
	std::shared_ptr<const FSelectionOutlineRequest> SelectionOutline;
};

class IRenderFeature
{
public:
	virtual ~IRenderFeature() = default;

	virtual void BeginFrame(FRenderFeatureContext&)
	{
	}

	virtual void Build(ERenderFeatureStage InStage, FRenderFeatureContext& InContext) = 0;

	virtual void EndFrame(FRenderFeatureContext&)
	{
	}

	virtual void Reset()
	{
	}

	virtual std::uint64_t ResourceBytes() const
	{
		return 0;
	}
};

using FRenderFeatureList = std::vector<std::unique_ptr<IRenderFeature>>;
using FRenderFeatureFactory = std::function<std::unique_ptr<IRenderFeature>()>;

// Startup-only registry; each viewport gets independent feature state. Create seals registration.
class FRenderFeatureRegistry
{
public:
	void Add(std::string InId, FRenderFeatureFactory InFactory);
	void Remove(const std::string& InId) noexcept;
	FRenderFeatureList Create();

private:
	std::vector<std::pair<std::string, FRenderFeatureFactory>> Factories;
	bool bSealed{};
};

std::unique_ptr<IRenderFeature> MakeContactShadowFeature();
// Compatibility for standalone renderer consumers. Application hosts pass their selected feature list.
FRenderFeatureList MakeDefaultRenderFeatures();
} // namespace Hyperion
