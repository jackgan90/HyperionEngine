#pragma once
#include "Hyperion/Renderer/RenderScene.h"
#include <map>
#include <mutex>

namespace Hyperion
{
struct FRenderBindingResult
{
	std::mutex Mutex;
	FRenderBindingStatus Status;
	std::weak_ptr<const FRenderResource> Resource;
	std::uint32_t Section{};
	void Publish(ERenderPrimitiveStatus InState, std::uint64_t InRevision, std::string InError = {},
	             std::shared_ptr<const FRenderResource> InResource = {}, std::uint32_t InSection = 0);
};

class FRenderScene
{
public:
	explicit FRenderScene(FTaskSystem& InTasks) : Tasks(InTasks)
	{
		Tasks.Require({EDomain::Render});
	}

	~FRenderScene();
	void Create(FRenderPrimitiveHandle InHandle, FRenderPrimitiveState InState,
	            const FRenderPrimitiveFactory& InFactory, std::shared_ptr<FRenderBindingResult> InResult);
	void Update(std::vector<FRenderPrimitiveUpdate> InUpdates);
	void Remove(FRenderPrimitiveHandle InHandle);
	FRenderSceneSnapshot Collect(FRenderView InView) const;

private:
	struct FEntry
	{
		FRenderPrimitiveHandle Handle;
		std::unique_ptr<IRenderPrimitive> Primitive;
		std::shared_ptr<FRenderBindingResult> Result;
	};

	FTaskSystem& Tasks;
	std::map<std::uint32_t, FEntry> Entries;
};

struct FRenderSceneMailbox
{
	explicit FRenderSceneMailbox(FTaskSystem& InTasks);
	FTaskSystem& Tasks;
	std::mutex Admission;
	std::uint64_t Identity{};
	bool bClosed{};
	std::vector<std::uint64_t> Generations;
	std::vector<bool> Active;
	// Only Render touches the scene, including destruction; clients retain this empty shell after Close.
	std::unique_ptr<FRenderScene> Scene;
	FTaskHandle Last;
	FTaskHandle Remove(FRenderPrimitiveHandle InHandle, const std::shared_ptr<FRenderBindingResult>& InResult);
};
} // namespace Hyperion
