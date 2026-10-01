#include "Hyperion/Materials/Lighting/ShadowParameters.h"
#include "Hyperion/Renderer/CascadedShadowMap.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
const std::array MatrixSemantics{EShadowViewV1Field::ShadowMatrix0, EShadowViewV1Field::ShadowMatrix1,
                                 EShadowViewV1Field::ShadowMatrix2, EShadowViewV1Field::ShadowMatrix3};
const std::array DepthSemantics{EShadowSemantic::ShadowDepth0, EShadowSemantic::ShadowDepth1,
                                EShadowSemantic::ShadowDepth2, EShadowSemantic::ShadowDepth3};
} // namespace

FMaterialParameterValues DefaultShadowParameters(EDepthConvention InConvention)
{
	static const std::array NeutralTextures{
	    std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{1, 1, 1}),
	    std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{1, 1, 0})};
	const auto& Neutral = NeutralTextures.at(static_cast<std::size_t>(InConvention));
	FMaterialSampler Sampler;
	Sampler.U = EMaterialAddressMode::Clamp;
	Sampler.V = EMaterialAddressMode::Clamp;
	Sampler.W = EMaterialAddressMode::Clamp;
	Sampler.bComparison = true;
	Sampler.Compare = InConvention == EDepthConvention::Reversed ? EMaterialSamplerCompare::GreaterEqual
	                                                             : EMaterialSamplerCompare::LessEqual;
	FMaterialParameterValues Result{
	    {EShadowViewV1Field::ShadowSplits, FMaterialValue::Float(FVec4{})},
	    {EShadowViewV1Field::ShadowTexels, FMaterialValue::Float(FVec4{})},
	    {EShadowViewV1Field::ShadowRanges, FMaterialValue::Float(FVec4{1, 1, 1, 1})},
	    {EShadowViewV1Field::ShadowCamera, FMaterialValue::Float(FVec4{0, 0, -1, 0})},
	    {EShadowViewV1Field::ShadowFilter,
	     FMaterialValue::Float(FVec4{.15f * GetDepthDirection(InConvention), .6f, .1f, .1f})},
	    {EShadowViewV1Field::ShadowControl, FMaterialValue::Float(FVec4{})},
	    {EShadowSemantic::ShadowSampler, FMaterialValue::FromSampler(Sampler)}};
	for (unsigned Index = 0; Index < 4; ++Index)
	{
		Result.push_back({MatrixSemantics[Index], FMaterialValue::Matrix(Identity())});
		Result.push_back({DepthSemantics[Index], FMaterialValue::FromTexture(Neutral)});
	}
	return Result;
}

void FCascadedShadowMap::Bind(FRenderView& InMain, FRenderPassTargets& InTargets,
                              std::shared_ptr<const void> InLifetime) const
{
	auto Parameters = DefaultShadowParameters(InMain.DepthConvention);
	const auto Set = [&Parameters](FMaterialSemanticId InSemantic, FMaterialValue InValue)
	{
		const auto Found = std::find_if(Parameters.begin(), Parameters.end(),
		                                [InSemantic](const auto& InParameter)
		                                {
			                                return InParameter.GetSemantic() == InSemantic;
		                                });
		Found->Value = std::move(InValue);
	};
	if (bEnabled)
	{
		Set(EShadowViewV1Field::ShadowSplits,
		    FMaterialValue::Float(FVec4{Data[0].Far, Data[1].Far, Data[2].Far, Data[3].Far}));
		Set(EShadowViewV1Field::ShadowTexels, FMaterialValue::Float(FVec4{Data[0].WorldTexel, Data[1].WorldTexel,
		                                                                  Data[2].WorldTexel, Data[3].WorldTexel}));
		Set(EShadowViewV1Field::ShadowRanges, FMaterialValue::Float(FVec4{Data[0].DepthRange, Data[1].DepthRange,
		                                                                  Data[2].DepthRange, Data[3].DepthRange}));
		const auto Direction = Normalize(InMain.Camera->Forward);
		Set(EShadowViewV1Field::ShadowCamera,
		    FMaterialValue::Float(FVec4{Direction.X, Direction.Y, Direction.Z, -Dot(InMain.Eye, Direction)}));
		Set(EShadowViewV1Field::ShadowFilter,
		    FMaterialValue::Float(FVec4{Settings.ReceiverBias * GetDepthDirection(DepthConvention),
		                                Settings.NormalOffset, Settings.BlendFraction, Settings.FadeFraction}));
		Set(EShadowViewV1Field::ShadowControl,
		    FMaterialValue::Float(
		        FVec4{1, Settings.DebugMode == 1 ? 1.f : 0.f, 1.f / Settings.Resolution, Settings.Distance}));
		for (std::size_t Index = 0; Index < Data.size(); ++Index)
		{
			Set(MatrixSemantics[Index], FMaterialValue::Matrix(Data[Index].ViewProjection));
			Set(DepthSemantics[Index], FMaterialValue::FromTexture(Textures[Index]));
			InTargets.Reads.push_back({ERenderTargetKind::Texture, Textures[Index], InLifetime, false});
		}
	}
	for (auto& Parameter : Parameters)
	{
		std::erase_if(InMain.Parameters,
		              [&](const auto& InValue)
		              {
			              return InValue.GetSemantic() == Parameter.GetSemantic();
		              });
		InMain.Parameters.push_back(std::move(Parameter));
	}
}
} // namespace Hyperion
