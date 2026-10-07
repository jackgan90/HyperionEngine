#include "Hyperion/RasterOptions/ShadowPreviewOptions.h"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr std::array DirectionalOptions{
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::Lit, 0, "Lit", {}},
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::CascadeColors, 1, "Cascade colors", {}},
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::Cascade0Depth, 2, "Cascade 0 depth", 0},
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::Cascade1Depth, 3, "Cascade 1 depth", 1},
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::Cascade2Depth, 4, "Cascade 2 depth", 2},
    FDirectionalShadowPreviewOption{EDirectionalShadowPreview::Cascade3Depth, 5, "Cascade 3 depth", 3}};
constexpr std::array ContactOptions{
    FContactShadowPreviewOption{EContactShadowPreview::Lit, 0, "Lit"},
    FContactShadowPreviewOption{EContactShadowPreview::VisibilityMask, 1, "Visibility mask"},
    FContactShadowPreviewOption{EContactShadowPreview::HierarchicalDepth, 2, "HZB depth"}};

template<class T, std::size_t Size> consteval bool IsComplete(const std::array<T, Size>& InOptions)
{
	using FIdentity = decltype(T::Id);
	if constexpr (Size != static_cast<std::size_t>(FIdentity::Count))
	{
		return false;
	}
	for (std::size_t Index = 0; Index < Size; ++Index)
	{
		if (InOptions[Index].Id >= FIdentity::Count || InOptions[Index].Label.empty())
		{
			return false;
		}
		for (std::size_t Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (InOptions[Index].Id == InOptions[Earlier].Id ||
			    InOptions[Index].WireValue == InOptions[Earlier].WireValue)
			{
				return false;
			}
		}
	}
	return true;
}

static_assert(IsComplete(DirectionalOptions));
static_assert(IsComplete(ContactOptions));

template<class T, class E> const T& Describe(std::span<const T> InOptions, E InId)
{
	for (const auto& Option : InOptions)
	{
		if (Option.Id == InId)
		{
			return Option;
		}
	}
	throw std::invalid_argument("Unknown shadow preview identity");
}

template<class T> auto Parse(std::span<const T> InOptions, std::uint32_t InValue)
{
	for (const auto& Option : InOptions)
	{
		if (Option.WireValue == InValue)
		{
			return Option.Id;
		}
	}
	throw std::invalid_argument("Unknown shadow preview value");
}
} // namespace

std::span<const FDirectionalShadowPreviewOption> DirectionalShadowPreviewOptions()
{
	return DirectionalOptions;
}

std::span<const FContactShadowPreviewOption> ContactShadowPreviewOptions()
{
	return ContactOptions;
}

const FDirectionalShadowPreviewOption& DescribeShadowPreview(EDirectionalShadowPreview InId)
{
	return Describe(DirectionalShadowPreviewOptions(), InId);
}

const FContactShadowPreviewOption& DescribeShadowPreview(EContactShadowPreview InId)
{
	return Describe(ContactShadowPreviewOptions(), InId);
}

EDirectionalShadowPreview ParseDirectionalShadowPreview(std::uint32_t InValue)
{
	return Parse(DirectionalShadowPreviewOptions(), InValue);
}

EContactShadowPreview ParseContactShadowPreview(std::uint32_t InValue)
{
	return Parse(ContactShadowPreviewOptions(), InValue);
}

EContactShadowPreview ParseAppContactShadowPreview(std::int64_t InValue)
{
	if (InValue < 0 || InValue > std::numeric_limits<std::uint32_t>::max())
	{
		throw std::invalid_argument("Contact shadow preview value is out of range");
	}
	return ParseContactShadowPreview(static_cast<std::uint32_t>(InValue));
}

std::uint32_t ToShadowPreviewWireValue(EDirectionalShadowPreview InId)
{
	return DescribeShadowPreview(InId).WireValue;
}

std::uint32_t ToShadowPreviewWireValue(EContactShadowPreview InId)
{
	return DescribeShadowPreview(InId).WireValue;
}

std::optional<std::uint32_t> ShadowPreviewCascade(EDirectionalShadowPreview InId)
{
	return DescribeShadowPreview(InId).Cascade;
}

std::uint32_t ContactShadowPreviewMinimum()
{
	return std::ranges::min(ContactOptions, {}, &FContactShadowPreviewOption::WireValue).WireValue;
}

std::uint32_t ContactShadowPreviewMaximum()
{
	return std::ranges::max(ContactOptions, {}, &FContactShadowPreviewOption::WireValue).WireValue;
}
} // namespace Hyperion
