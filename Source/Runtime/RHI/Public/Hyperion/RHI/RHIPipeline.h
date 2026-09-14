#pragma once
#include "Hyperion/Platform/Window.h"
#include "Hyperion/RHI/RHICapabilities.h"
#include "Hyperion/RHI/RHITypes.h"

namespace Hyperion
{
struct FVertexFormatInfo
{
	EShaderScalar Scalar;
	std::uint32_t Components{};
	std::uint32_t Bytes{};
};

FVertexFormatInfo GetVertexFormatInfo(EVertexFormat InFormat);
void ValidateGraphicsPipeline(const FPipelineDesc& InDesc);
void ValidateComputePipeline(const FComputePipelineDesc& InDesc, const FRHICapabilities& InCapabilities);
std::array<std::uint32_t, 3> ComputeDispatchGroups(std::array<std::uint32_t, 3> InExtent,
                                                   std::array<std::uint32_t, 3> InGroupSize);
void ValidateGraphicsDynamicState(const FGraphicsDynamicState& InState);
void ValidateViewport(const FViewport& InViewport, FSize InTargetSize);
} // namespace Hyperion
