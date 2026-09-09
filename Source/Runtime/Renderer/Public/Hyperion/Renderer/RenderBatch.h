#pragma once
#include "Hyperion/RHI/RHICapabilities.h"
#include "Hyperion/RHI/RHITypes.h"
#include "Hyperion/Renderer/MaterialPreparation.h"
#include "Hyperion/Renderer/RenderBatchStats.h"
#include "Hyperion/Renderer/RenderPrimitive.h"

namespace Hyperion
{
struct FRenderBatchStructure
{
	std::uint64_t GeometryIdentity{};
	std::uint32_t GeometryIndex{};
	std::uint32_t FirstIndex{};
	std::uint32_t IndexCount{};
	std::uint32_t VertexStride{};
	ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
	std::vector<FVertexAttribute> Attributes;
	std::string VertexProgram;
	std::string PixelProgram;
	FGraphicsState State;
	FGraphicsDynamicState DynamicState;
	FGraphicsTarget Target;
	FResourceBindingLayoutDesc Layout;
	std::vector<std::uint64_t> Resources;
	bool operator==(const FRenderBatchStructure& InOther) const = default;
	std::size_t Hash() const;
};

using FRenderBatchValues = std::vector<std::shared_ptr<const FMaterialValue>>;

struct FRenderBatchSignature
{
	std::shared_ptr<const FRenderBatchStructure> Structure;
	std::shared_ptr<const FRenderBatchValues> SharedValues;
	bool operator==(const FRenderBatchSignature& InOther) const;
	std::size_t Hash() const;
};

struct FRenderBatchCandidate
{
	FRenderBatchSignature Signature;
	std::shared_ptr<const FCompiledMaterialDefinition> Program;
	const FCompiledMaterialPass* Pass{};
	std::size_t CompatibilityHash{};
	bool bReorderable{};
};

struct FRenderBatchDecision
{
	std::uint32_t Capacity{};
	ERenderBatchFallback Reason = ERenderBatchFallback::Shader;
};

// Strategies decide compatibility/capacity. The coordinator alone owns source-item claims and publication order.
// New batch execution methods can extend the plan's payload while retaining this scheduling protocol.
class IRenderBatchStrategy
{
public:
	virtual ~IRenderBatchStrategy() = default;
	virtual FRenderBatchDecision Evaluate(const FRenderBatchCandidate& InCandidate,
	                                      const FRHICapabilities& InCapabilities) const = 0;
	virtual bool CanCombine(const FRenderBatchCandidate& InA, const FRenderBatchCandidate& InB) const = 0;
};

class FInstanceBatchStrategy final : public IRenderBatchStrategy
{
public:
	FRenderBatchDecision Evaluate(const FRenderBatchCandidate& InCandidate,
	                              const FRHICapabilities& InCapabilities) const override;
	bool CanCombine(const FRenderBatchCandidate& InA, const FRenderBatchCandidate& InB) const override;
};

struct FInstanceConstantBlock
{
	std::uint32_t Slot{};
	std::shared_ptr<const std::vector<std::byte>> Bytes;
};

struct FInstanceBatchData
{
	std::uint32_t InstanceCount{};
	std::vector<FInstanceConstantBlock> Constants;
	std::vector<std::weak_ptr<const void>> Owners;
	bool IsLive() const;
	std::size_t ByteSize() const;
};

struct FRenderBatch
{
	std::vector<std::size_t> Items;
	std::shared_ptr<const FInstanceBatchData> Instances; // Null means ordinary draws for these items.
};

struct FRenderBatchPlan
{
	std::vector<FRenderBatch> Batches;
	FRenderBatchStats Statistics;
};

struct FRenderBatchLimits
{
	std::size_t MaxItems = 4096;
	std::size_t MaxChunks = 512;
	std::size_t MaxBytes = 16 * 1024 * 1024;
};

// Construction/registration may precede rendering; all Build/Clear/cache access belongs to Render.
class FRenderBatchSystem
{
public:
	explicit FRenderBatchSystem(FTaskSystem& InTasks, FRHICapabilities InCapabilities,
	                            FRenderBatchLimits InLimits = {});
	~FRenderBatchSystem();
	void Register(std::unique_ptr<IRenderBatchStrategy> InStrategy);
	std::shared_ptr<const FRenderBatchPlan> Build(const FRenderSceneSnapshot& InSnapshot, bool bInEnabled = true);
	void Clear();
	// Scene invalidation releases plan parameter owners even when no later family is rendered.
	void InvalidatePlans();
	bool HasCustomStrategies() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

FRenderBatchCandidate DescribeBatchCandidate(const FRenderItem& InItem, const FRenderView& InView,
                                             FGraphicsTarget InTarget);
std::shared_ptr<const FInstanceBatchData> PackInstanceBatch(const FRenderSceneSnapshot& InSnapshot,
                                                            std::span<const std::size_t> InItems);
} // namespace Hyperion
