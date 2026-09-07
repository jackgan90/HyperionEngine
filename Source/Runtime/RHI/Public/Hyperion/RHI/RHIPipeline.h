#pragma once
#include "Hyperion/Platform/Window.h"
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
void ValidateGraphicsDynamicState(const FGraphicsDynamicState& InState);
void ValidateViewport(const FViewport& InViewport, FSize InTargetSize);
} // namespace Hyperion
