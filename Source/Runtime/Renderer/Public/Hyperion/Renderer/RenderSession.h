#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "Hyperion/Renderer/RenderScene.h"
#include <set>

namespace Hyperion
{
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
	std::size_t Build(FRenderGraph& InGraph, FRenderView InView);
	std::size_t BuildViews(FRenderGraph& InGraph, std::span<const FRenderView> InViews,
	                       std::shared_ptr<const FMaterialFrameContext> InFrame = {}, std::uint64_t InFamily = 1);
	std::shared_ptr<const FMaterialFrameContext> FreezeFrame(float InTime = 0,
	                                                         FMaterialParameterValues InFrameValues = {});
	void SetGlobalParameters(FMaterialParameterValues InValues);
	void SetSceneParameters(FMaterialParameterValues InValues);
	FMaterialProviderRegistry& GetProviders();
	FRenderBatchSystem& GetBatchSystem(); // Register strategies on Main before the first build.
	FSceneVisibilityStats Statistics() const;
	FMaterialProviderStats ProviderStatistics() const; // Render only.
	void Close();

private:
	FTaskSystem& Tasks;
	FRenderResourceService Resources;
	FRenderSceneClient Scene;
	FRenderBatchSystem Batches;
	bool bClosed{};
	FSceneVisibilityStats LastStatistics;
	struct FMaterialState;
	std::unique_ptr<FMaterialState> MaterialState;
	std::set<std::uint64_t> AdmitFamily(std::span<const FRenderView> InViews, const FMaterialFrameContext& InFrame,
	                                    std::uint64_t InFamily);
	void PrepareMaterials(FRenderSceneSnapshot& InSnapshot);
};
} // namespace Hyperion
