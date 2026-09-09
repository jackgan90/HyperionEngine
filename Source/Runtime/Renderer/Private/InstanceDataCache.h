#pragma once
#include "Hyperion/Renderer/RenderBatch.h"

namespace Hyperion
{
// Render-owned packing history. Published bytes are immutable and independently shareable across views.
class FInstanceDataCache
{
public:
	explicit FInstanceDataCache(FRenderBatchLimits InLimits);
	~FInstanceDataCache();
	std::shared_ptr<const FInstanceBatchData> Pack(const FRenderSceneSnapshot& InSnapshot,
	                                               std::span<const std::size_t> InItems, FRenderBatchStats& OutStats);
	void Collect();
	void Clear();
	std::size_t ByteSize() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
