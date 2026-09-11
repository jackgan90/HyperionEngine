#pragma once
#include "Hyperion/Materials/MaterialParameters.h"
#include "Hyperion/RHI/RHIGraphicsState.h"
#include "Hyperion/Renderer/MaterialFrame.h"
#include "Hyperion/Renderer/RenderItemList.h"
#include "Hyperion/Renderer/RenderPass.h"
#include "Hyperion/Renderer/SceneSpatialIndex.h"
#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Tasks/TaskSystem.h"
#include <array>
#include <atomic>
#include <optional>

namespace Hyperion
{
class FRenderResource;
class FRenderMaterial;
struct FMaterialEvaluationCache;
struct FMaterialSharedBinding;
struct FLocalMaterialItem;
struct FRenderBatchPlan;
struct FSceneItemPreparation;
struct FMaterialBindingGroups;

struct FRenderPrimitiveHandle
{
	std::uint64_t Scene{};
	std::uint32_t Slot{};
	std::uint64_t Generation{};
	bool operator==(const FRenderPrimitiveHandle&) const = default;
};

// Complete instance snapshot; no borrowed logical-object or GPU pointers.
struct FRenderPrimitiveState
{
	std::uint64_t Revision = 1;
	FMat4 World = Identity();
	bool bVisible = true;
	std::shared_ptr<const FRenderResource> Resource;
	std::uint32_t Section{};
	FMaterialOverride Material;
	FBounds LocalBounds;
	std::shared_ptr<const FRenderMaterial> Surface;
	FMaterialParameterValues ObjectParameters;
	FMaterialParameterValues SectionParameters;
	// World maps vertices to canonical standard-Z clip space; Renderer adapts depth in WVP only.
	bool bClipSpace{};
	bool bConservativeBounds{};
	FMaterialParameterValues ObjectInputs; // Semantic provider inputs, independently owned.
};

struct FRenderCamera
{
	FVec3 Forward{0, 0, -1};
	FVec3 Up{0, 1, 0};
	float VerticalRadians = 1;
	float Near = .05f;
	float Far = 500;
};

struct FRenderView
{
	FMat4 ViewProjection = Hyperion::Identity();
	FVec3 Eye;
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	std::optional<FMat4> CullingViewProjection;
	std::uint64_t Identity = 1;
	std::uint64_t Revision = 1;
	std::string Usage = "Forward";
	std::optional<FViewport> Viewport;
	FMaterialParameterValues Parameters;
	FMaterialParameterValues PassParameters;
	bool bInstanceBatching = true;
	std::optional<FRenderCamera> Camera;
	bool bSkipMissingPass{};
	std::vector<std::string> ExcludedPasses; // Materials with any listed usage belong to another route.
	EDepthConvention DepthConvention = EDepthConvention::Standard;
};

struct FRenderItem
{
	FRenderPrimitiveState State;
	FRenderPrimitiveHandle Primitive;
	std::uint64_t Group{};
	std::optional<std::uint64_t> LocalItemId;
	std::uint64_t Ordinal{};
	FMaterialParameterValues DrawParameters;
	std::shared_ptr<const void> Lifetime;
	FMaterialBindingContext Context;
	std::string PreparationError;
	std::function<void(FRenderDrawResult)> Report;
	std::optional<FMaterialDynamicState> DynamicState; // Requires the selected pass to allow draw overrides.
	FMaterialParameterValues DrawInputs;               // Semantic provider inputs; DrawParameters are name overrides.
	std::shared_ptr<FMaterialEvaluationCache> EvaluationCache; // Renderer-owned; collection must not edit it.
	std::shared_ptr<const FResolvedMaterialParameters> ResolvedParameters;
	std::shared_ptr<const FMaterialSharedParameters> SharedParameters;
	std::shared_ptr<const FSceneItemPreparation> Preparation;    // Renderer-owned immutable collection metadata.
	std::shared_ptr<const FMaterialSharedBinding> SharedBinding; // Renderer-owned local dependency proof.
	std::shared_ptr<const FLocalMaterialItem> LocalPreparation;  // Stable local values across visibility changes.

	const std::shared_ptr<const FMaterialValue>& GetMaterialValue(std::size_t InIndex) const
	{
		if (SharedParameters)
		{
			const auto& Value = SharedParameters->Values->Get(InIndex);
			if (Value)
			{
				return *Value;
			}
		}
		return ResolvedParameters->Values[InIndex];
	}
};

struct FRenderSceneSnapshot
{
	static constexpr std::size_t RetainedItemLimit = 2048;
	FRenderView View;
	FRenderPassTargets Targets;
	FRenderItemList Items;
	FRenderItemList RetainedItems; // Bounded static preparation outside the current visible set; never submitted.
	bool bRetainCulledItems{};
	FSceneVisibilityStats Statistics;
	std::shared_ptr<const FMaterialFrameContext> Frame;
	std::uint64_t Family = 1;
	std::shared_ptr<const FRenderBatchPlan> Batches;
	std::shared_ptr<const FMaterialBindingGroups> SharedBindingGroups; // Immutable indices for this ordered membership.
	// Renderer-owned identities: immutable prepared contents and the current preparation receipt frame.
	std::shared_ptr<const void> ContentIdentity;
	std::shared_ptr<const void> LocalContentIdentity; // Ordered local draw state; shared numeric inputs may change.
	std::shared_ptr<std::atomic_uint64_t> DrawFrame;
	// Renderer-owned collection provenance: scene identity, scene revision and resource publication revision.
	std::array<std::uint64_t, 3> CollectionKey{};
};

// All instance methods, including construction/destruction, belong to Render.
// Derived collection is read-only and may emit zero or multiple owned items.
class IRenderPrimitive
{
public:
	explicit IRenderPrimitive(FTaskSystem& InTasks);
	virtual ~IRenderPrimitive();
	IRenderPrimitive(const IRenderPrimitive&) = delete;
	IRenderPrimitive& operator=(const IRenderPrimitive&) = delete;
	void Apply(FRenderPrimitiveState InState) noexcept;
	const FRenderPrimitiveState& GetState() const;
	virtual FBounds GetWorldBounds() const;
	virtual void Collect(const FRenderView& InView, std::vector<FRenderItem>& OutItems) const = 0;

	// Opt-in: Collect and bounds depend only on published state/resource versions, never on view or time.
	virtual bool IsStaticCollection() const
	{
		return false;
	}

protected:
	FTaskSystem& Tasks;

private:
	FRenderPrimitiveState State;
};

class FStaticMeshRenderPrimitive final : public IRenderPrimitive
{
public:
	using IRenderPrimitive::IRenderPrimitive;
	FBounds GetWorldBounds() const override;
	void Collect(const FRenderView& InView, std::vector<FRenderItem>& OutItems) const override;

	bool IsStaticCollection() const override
	{
		return true;
	}
};

void ValidatePrimitiveState(const FRenderPrimitiveState& InState);
FMaterialParameterValues GetPrimitiveMaterialOverrides(const FRenderPrimitiveState& InState,
                                                       const FMaterialParameterSchema& InSchema);
} // namespace Hyperion
