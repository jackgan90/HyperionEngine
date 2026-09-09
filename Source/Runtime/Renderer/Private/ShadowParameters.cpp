#include "Hyperion/Renderer/CascadedShadowMap.h"
#include <algorithm>

namespace Hyperion
{
FMaterialParameterValues DefaultShadowParameters()
{
	static const auto Neutral = std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{1, 1, 1});
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	Sampler.bComparison = true;
	Sampler.Compare = EMaterialSamplerCompare::LessEqual;
	FMaterialParameterValues Result{{"Engine.View.ShadowSplits", FMaterialValue::Float(FVec4{})},
	                                {"Engine.View.ShadowTexels", FMaterialValue::Float(FVec4{})},
	                                {"Engine.View.ShadowRanges", FMaterialValue::Float(FVec4{1, 1, 1, 1})},
	                                {"Engine.View.ShadowCamera", FMaterialValue::Float(FVec4{0, 0, -1, 0})},
	                                {"Engine.View.ShadowFilter", FMaterialValue::Float(FVec4{.15f, .6f, .1f, .1f})},
	                                {"Engine.View.ShadowControl", FMaterialValue::Float(FVec4{})},
	                                {"Engine.View.ShadowSampler", FMaterialValue::FromSampler(Sampler)}};
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Result.push_back({"Engine.View.ShadowMatrix" + std::to_string(Index), FMaterialValue::Matrix(Identity())});
		Result.push_back({"Engine.View.ShadowDepth" + std::to_string(Index), FMaterialValue::FromTexture(Neutral)});
	}
	return Result;
}

void FCascadedShadowMap::Bind(FRenderView& InMain, std::shared_ptr<const void> InLifetime) const
{
	auto Parameters = DefaultShadowParameters();
	const auto Set = [&Parameters](std::string InName, FMaterialValue InValue)
	{
		const auto Found = std::find_if(Parameters.begin(), Parameters.end(),
		                                [&InName](const auto& InParameter)
		                                {
			                                return InParameter.Name == "Engine.View." + InName;
		                                });
		Found->Value = std::move(InValue);
	};
	if (bEnabled)
	{
		Set("ShadowSplits", FMaterialValue::Float(FVec4{Data[0].Far, Data[1].Far, Data[2].Far, Data[3].Far}));
		Set("ShadowTexels", FMaterialValue::Float(
		                        FVec4{Data[0].WorldTexel, Data[1].WorldTexel, Data[2].WorldTexel, Data[3].WorldTexel}));
		Set("ShadowRanges", FMaterialValue::Float(
		                        FVec4{Data[0].DepthRange, Data[1].DepthRange, Data[2].DepthRange, Data[3].DepthRange}));
		const auto Direction = Normalize(InMain.Camera->Forward);
		Set("ShadowCamera",
		    FMaterialValue::Float(FVec4{Direction.X, Direction.Y, Direction.Z, -Dot(InMain.Eye, Direction)}));
		Set("ShadowFilter", FMaterialValue::Float(FVec4{Settings.ReceiverBias, Settings.NormalOffset,
		                                                Settings.BlendFraction, Settings.FadeFraction}));
		Set("ShadowControl", FMaterialValue::Float(FVec4{1, Settings.DebugMode == 1 ? 1.f : 0.f,
		                                                 1.f / Settings.Resolution, Settings.Distance}));
		for (std::size_t Index = 0; Index < Data.size(); ++Index)
		{
			Set("ShadowMatrix" + std::to_string(Index), FMaterialValue::Matrix(Data[Index].ViewProjection));
			Set("ShadowDepth" + std::to_string(Index), FMaterialValue::FromTexture(Textures[Index]));
			InMain.SampledDepth.push_back(Textures[Index]);
		}
		InMain.TargetLifetime = std::move(InLifetime);
	}
	for (auto& Parameter : Parameters)
	{
		std::erase_if(InMain.Parameters,
		              [&](const auto& InValue)
		              {
			              return InValue.Name == Parameter.Name;
		              });
		InMain.Parameters.push_back(std::move(Parameter));
	}
}
} // namespace Hyperion
