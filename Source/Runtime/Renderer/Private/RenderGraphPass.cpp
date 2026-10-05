#include "RenderGraphPass.h"

namespace Hyperion
{
FAcceptedGraphPass AcceptGraphPass(FGraphicsPass InPass)
{
	if (InPass.Color && !InPass.Colors.empty())
	{
		throw std::invalid_argument("Graph pass cannot mix single and multiple color attachment storage");
	}
	const auto Layout = InPass.Color ? EGraphColorCommandLayout::Single : EGraphColorCommandLayout::Multiple;
	if (InPass.Color)
	{
		InPass.Colors.push_back(std::move(*InPass.Color));
	}
	return {{std::move(InPass.Name), std::move(InPass.Reads), std::move(InPass.After), InPass.Timing},
	        FGraphGraphicsPayload{std::move(InPass.Colors), std::move(InPass.DepthStencil), InPass.Viewport,
	                              std::move(InPass.Batches), std::move(InPass.Prepare), std::move(InPass.Buffers),
	                              Layout}};
}

FAcceptedGraphPass AcceptGraphPass(FComputePass InPass)
{
	return {{std::move(InPass.Name), std::move(InPass.Reads), std::move(InPass.After), InPass.Timing},
	        FGraphComputePayload{std::move(InPass.Writes), std::move(InPass.Buffers), std::move(InPass.Dispatches),
	                             std::move(InPass.Prepare)}};
}
} // namespace Hyperion
