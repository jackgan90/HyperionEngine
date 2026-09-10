#pragma once
#include "Hyperion/Renderer/RenderGraph.h"

namespace Hyperion
{
inline FGraphicsPass MakeColorPass(FRenderGraph& InGraph, std::string InName,
                                   EAttachmentLoad InLoad = EAttachmentLoad::Load, FVec4 InClear = {})
{
	const auto Color = InGraph.ImportBackbuffer();
	InGraph.Export(Color, EResourceState::Present);
	FGraphicsPass Pass;
	Pass.Name = std::move(InName);
	Pass.Color = FGraphColorAttachment{Color, {InLoad}, InClear};
	Pass.Batches.push_back({});
	return Pass;
}
} // namespace Hyperion
