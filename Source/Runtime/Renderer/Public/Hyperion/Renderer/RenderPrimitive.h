#pragma once
#include "Hyperion/Renderer/SceneSpatialIndex.h"
#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Tasks/TaskSystem.h"
#include <optional>

namespace Hyperion
{
class FRenderResource;

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
};

struct FRenderView
{
	FMat4 ViewProjection = Identity();
	FVec3 Eye;
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	ESceneCullingMode CullingMode = ESceneCullingMode::Bvh;
	std::optional<FMat4> CullingViewProjection;
};

struct FRenderItem
{
	FRenderPrimitiveState State;
	FRenderPrimitiveHandle Primitive;
};

struct FRenderSceneSnapshot
{
	FRenderView View;
	std::vector<FRenderItem> Items;
	FSceneVisibilityStats Statistics;
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
};

void ValidatePrimitiveState(const FRenderPrimitiveState& InState);
} // namespace Hyperion
