#include "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.h"
#include <algorithm>

namespace Hyperion
{
std::vector<FMaterialShaderDefine> MakeGBufferVisualizerShaderDefines(
    std::span<const FGBufferVisualizerOption> InOptions)
{
	ValidateRasterOptionPresentation(InOptions);
	std::vector<FGBufferVisualizerOption> Ordered(InOptions.begin(), InOptions.end());
	std::sort(Ordered.begin(), Ordered.end(),
	          [](const auto& InLeft, const auto& InRight)
	          {
		          return InLeft.Id < InRight.Id;
	          });
	std::vector<FMaterialShaderDefine> Result;
	Result.reserve(Ordered.size());
	for (const auto& Option : Ordered)
	{
		Result.push_back({std::string(Option.ShaderDefine), std::to_string(ToVisualizerShaderCode(Option.Id))});
	}
	return Result;
}
} // namespace Hyperion
