#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Hyperion
{
enum class EDirectionalShadowPreview : std::uint8_t
{
	Lit,
	CascadeColors,
	Cascade0Depth,
	Cascade1Depth,
	Cascade2Depth,
	Cascade3Depth,
	Count
};

enum class EContactShadowPreview : std::uint8_t
{
	Lit,
	VisibilityMask,
	HierarchicalDepth,
	Count
};

struct FDirectionalShadowPreviewOption
{
	EDirectionalShadowPreview Id;
	std::uint32_t WireValue;
	std::string_view Label;
	std::optional<std::uint32_t> Cascade;
};

struct FContactShadowPreviewOption
{
	EContactShadowPreview Id;
	std::uint32_t WireValue;
	std::string_view Label;
};

std::span<const FDirectionalShadowPreviewOption> DirectionalShadowPreviewOptions();
std::span<const FContactShadowPreviewOption> ContactShadowPreviewOptions();
const FDirectionalShadowPreviewOption& DescribeShadowPreview(EDirectionalShadowPreview InId);
const FContactShadowPreviewOption& DescribeShadowPreview(EContactShadowPreview InId);
EDirectionalShadowPreview ParseDirectionalShadowPreview(std::uint32_t InValue);
EContactShadowPreview ParseContactShadowPreview(std::uint32_t InValue);
EContactShadowPreview ParseAppContactShadowPreview(std::int64_t InValue);
std::uint32_t ToShadowPreviewWireValue(EDirectionalShadowPreview InId);
std::uint32_t ToShadowPreviewWireValue(EContactShadowPreview InId);
std::optional<std::uint32_t> ShadowPreviewCascade(EDirectionalShadowPreview InId);
std::uint32_t ContactShadowPreviewMinimum();
std::uint32_t ContactShadowPreviewMaximum();
} // namespace Hyperion
