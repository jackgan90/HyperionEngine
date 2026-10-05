#include "Hyperion/RenderControls/ViewportChoices.h"
#include "Hyperion/RasterOptions/RasterOptions.h"
#include <algorithm>
#include <array>

namespace Hyperion
{
namespace
{
constexpr std::array CullingOptions{FSceneCullingOption{ESceneCullingMode::None, 0, "None"},
                                    FSceneCullingOption{ESceneCullingMode::Linear, 1, "Linear frustum"},
                                    FSceneCullingOption{ESceneCullingMode::Bvh, 2, "BVH frustum"}};
constexpr std::array OutlineOptions{FOutlineOverlapOption{EOutlineOverlapMode::Union, 0, "Union"},
                                    FOutlineOverlapOption{EOutlineOverlapMode::PerObject, 1, "Per object"}};

template<class TOption, std::size_t Size> consteval bool IsUnambiguous(const std::array<TOption, Size>& InOptions)
{
	for (std::size_t Index = 0; Index < Size; ++Index)
	{
		if (InOptions[Index].Label.empty())
		{
			return false;
		}
		for (std::size_t Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (InOptions[Earlier].Id == InOptions[Index].Id ||
			    InOptions[Earlier].WireValue == InOptions[Index].WireValue)
			{
				return false;
			}
		}
	}
	return true;
}

static_assert(IsUnambiguous(CullingOptions));
static_assert(IsUnambiguous(OutlineOptions));

template<class TOption> const TOption* FindWireOption(std::span<const TOption> InOptions, std::uint32_t InValue)
{
	const auto Found = std::find_if(InOptions.begin(), InOptions.end(),
	                                [InValue](const TOption& InOption)
	                                {
		                                return InOption.WireValue == InValue;
	                                });
	return Found == InOptions.end() ? nullptr : &*Found;
}

template<class TOption> auto ParseWireOption(std::span<const TOption> InOptions, std::uint32_t InValue)
{
	if (const auto* Option = FindWireOption(InOptions, InValue))
	{
		return Option->Id;
	}
	throw std::invalid_argument("Unknown viewport option wire value: " + std::to_string(InValue));
}

template<class TOption>
void ValidatePresentation(std::span<const TOption> InOptions, std::span<const TOption> InCanonical)
{
	if (InOptions.size() != InCanonical.size())
	{
		throw std::invalid_argument("Viewport option presentation must contain every supported identity");
	}
	for (std::size_t Index = 0; Index < InOptions.size(); ++Index)
	{
		const auto& Option = InOptions[Index];
		const auto& Canonical = InCanonical[RasterOptionIndex(InCanonical, Option.Id)];
		if (Option.Label.empty() || Option.WireValue != Canonical.WireValue ||
		    RasterOptionIndex(InOptions, Option.Id) != Index)
		{
			throw std::invalid_argument("Viewport option presentation changes a stable identity or wire meaning");
		}
	}
}
} // namespace

std::span<const FSceneCullingOption> SceneCullingOptions()
{
	return CullingOptions;
}

std::span<const FOutlineOverlapOption> OutlineOverlapOptions()
{
	return OutlineOptions;
}

bool IsSceneCullingWireValue(std::uint32_t InValue)
{
	return FindWireOption(SceneCullingOptions(), InValue) != nullptr;
}

bool IsOutlineOverlapWireValue(std::uint32_t InValue)
{
	return FindWireOption(OutlineOverlapOptions(), InValue) != nullptr;
}

ESceneCullingMode ParseSceneCullingMode(std::uint32_t InValue)
{
	return ParseWireOption(SceneCullingOptions(), InValue);
}

EOutlineOverlapMode ParseOutlineOverlapMode(std::uint32_t InValue)
{
	return ParseWireOption(OutlineOverlapOptions(), InValue);
}

std::uint32_t ToCullingWireValue(ESceneCullingMode InId)
{
	const auto Options = SceneCullingOptions();
	return Options[RasterOptionIndex(Options, InId)].WireValue;
}

std::uint32_t ToOutlineWireValue(EOutlineOverlapMode InId)
{
	const auto Options = OutlineOverlapOptions();
	return Options[RasterOptionIndex(Options, InId)].WireValue;
}

void ValidateViewportOptionPresentation(std::span<const FSceneCullingOption> InOptions)
{
	ValidatePresentation(InOptions, SceneCullingOptions());
}

void ValidateViewportOptionPresentation(std::span<const FOutlineOverlapOption> InOptions)
{
	ValidatePresentation(InOptions, OutlineOverlapOptions());
}
} // namespace Hyperion
