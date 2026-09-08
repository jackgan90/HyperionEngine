#pragma once
#include "Hyperion/Renderer/RenderBatch.h"
#include "RenderResourcesInternal.h"

namespace Hyperion
{
struct FPreparedSceneDraws
{
	std::vector<std::optional<FDrawPacket>> Packets;
	std::vector<bool> Srgb;
	std::map<std::pair<std::uint64_t, std::uint64_t>, std::string> Failures;
	bool bDepth{};
	bool bStencil{};
	ERHIDepthFormat Depth = ERHIDepthFormat::None;
	FRenderBatchStats Statistics;
};

void PrepareBatchedDraws(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                         FPreparedSceneDraws& OutPrepared);
} // namespace Hyperion
