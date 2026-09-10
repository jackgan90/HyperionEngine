#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Renderer/RenderScene.h"
#include <set>

namespace Hyperion
{
struct FRenderViewStatistics
{
	std::uint64_t Identity{};
	std::string Usage;
	FSceneVisibilityStats Visibility;
};

struct FRenderViewFamilyStatistics
{
	std::vector<FRenderViewStatistics> Views;
	FSceneVisibilityStats Spatial;
	double Milliseconds{};
};

struct FPreparedViewFamily;

// Owns one family independently of subsequent Session builds. Statistics throws until prepared.
class FRenderViewPreparation
{
public:
	FRenderViewFamilyStatistics Statistics() const;

private:
	std::shared_ptr<FPreparedViewFamily> State;
	friend class FRenderSession;
};

// Main constructs/closes the session; Render builds its scene passes.
class FRenderSession
{
public:
	FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler);
	FRenderSession(FTaskSystem& InTasks, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
	               ERHIDepthFormat InDepthFormat, std::shared_ptr<const FMaterialSemanticRegistry> InSemantics);
	~FRenderSession();
	FRenderSession(const FRenderSession&) = delete;
	FRenderSession& operator=(const FRenderSession&) = delete;
	FRenderSceneClient& GetScene();
	FRenderResourceService& GetResources();
	FRenderPassTargets FrameTargets(std::optional<FVec4> InClear = {}) const;
	std::size_t Build(FRenderGraph& InGraph, FRenderView InView, FRenderPassTargets InTargets);
	std::size_t BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
	                       std::span<const FRenderPassTargets> InTargets,
	                       std::shared_ptr<const FMaterialFrameContext> InFrame = {}, std::uint64_t InFamily = 1,
	                       bool bInSpatialPrepared = false, bool bInDeferPreparation = false);
	std::size_t BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
	                       const FRenderPassTargets& InTargets,
	                       std::shared_ptr<const FMaterialFrameContext> InFrame = {}, std::uint64_t InFamily = 1,
	                       bool bInSpatialPrepared = false, bool bInDeferPreparation = false);
	// Render publishes deferred statistics after graph execution joins the RHI coordinator.
	double CompleteViews();
	FRenderViewPreparation GetViewPreparation() const; // Render, immediately after BuildViews.
	std::shared_ptr<const FMaterialFrameContext> FreezeFrame(float InTime = 0,
	                                                         FMaterialParameterValues InFrameValues = {});
	void SetGlobalParameters(FMaterialParameterValues InValues);
	void SetSceneParameters(FMaterialParameterValues InValues);
	FMaterialProviderRegistry& GetProviders();
	// Render only. Resolves Global/Frame/Scene dependencies; returns no value for any local dependency.
	FMaterialSharedValue ResolveFrameSemantic(const FMaterialFrameContext& InFrame, std::string_view InSemantic);
	FRenderBatchSystem& GetBatchSystem();     // Register strategies on Main before the first build.
	FSceneVisibilityStats Statistics() const; // Legacy: last-view visibility, family-wide draws/batches.
	const std::vector<FRenderViewStatistics>& ViewStatistics() const;
	FMaterialProviderStats ProviderStatistics() const; // Render only.
	void AppendDepthPreview(FRenderGraph& InGraph, std::shared_ptr<const FMaterialTextureSource> InSource,
	                        std::shared_ptr<const void> InLifetime, FViewport InViewport, bool bInDeferred = false);
	void Close();

private:
	FTaskSystem& Tasks;
	FRenderResourceService Resources;
	FRenderSceneClient Scene;
	FRenderBatchSystem Batches;
	bool bClosed{};
	FSceneVisibilityStats LastStatistics;
	std::vector<FRenderViewStatistics> LastViews;
	std::shared_ptr<FPreparedViewFamily> PendingFamily;
	struct FMaterialState;
	std::unique_ptr<FMaterialState> MaterialState;
	std::set<std::uint64_t> AdmitFamily(std::span<const FRenderView> InViews, const FMaterialFrameContext& InFrame,
	                                    std::uint64_t InFamily);
	void PrepareMaterials(FRenderSceneSnapshot& InSnapshot, bool bInStableCollection);
	FMaterialProviderInputs PrepareViewInputs(const FRenderSceneSnapshot& InSnapshot);
	void InvalidatePreparedViews();
	std::shared_ptr<const FRenderSceneSnapshot> PrepareView(const FRenderView& InView,
	                                                        const FRenderPassTargets& InTargets,
	                                                        std::shared_ptr<const FMaterialFrameContext> InFrame,
	                                                        std::uint64_t InFamily,
	                                                        std::optional<std::uint64_t> InSceneRevision,
	                                                        std::uint64_t InResourceRevision,
	                                                        std::vector<FRenderTargetSource>& OutReads);
};
} // namespace Hyperion
