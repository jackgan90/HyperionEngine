#pragma once
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/Renderer/ShaderParameters/DeferredLightingParameters.h"

namespace Hyperion
{
struct FRHICapabilities;

enum class EGBufferRole
{
	BaseMetallic,
	Normals,
	Surface,
	Emissive,
	Count
};

enum class EGBufferStorage
{
	Color,
	FloatingPoint
};

inline constexpr std::size_t GBufferAttachmentCount = static_cast<std::size_t>(EGBufferRole::Count);

struct FGBufferAttachment
{
	EGBufferRole Role;
	std::size_t Slot;
	EDeferredLightingSemantic Semantic;
	EMaterialColorFormat DefaultFormat;
	EGBufferStorage Storage;
};

// Layout version 1. Slots and shader semantics are an explicit ABI, independent of enum ordinals.
std::span<const FGBufferAttachment> GBufferAttachments();
const FGBufferAttachment& DescribeGBufferAttachment(EGBufferRole InRole);
bool SupportsGBufferFormat(EGBufferRole InRole, EMaterialColorFormat InFormat);
std::array<EMaterialColorFormat, GBufferAttachmentCount> DefaultGBufferFormats();

template<class T> T& GBufferAttachment(std::array<T, GBufferAttachmentCount>& InValues, EGBufferRole InRole)
{
	return InValues.at(DescribeGBufferAttachment(InRole).Slot);
}

template<class T> const T& GBufferAttachment(const std::array<T, GBufferAttachmentCount>& InValues, EGBufferRole InRole)
{
	return InValues.at(DescribeGBufferAttachment(InRole).Slot);
}

struct FGBufferLayout
{
	std::array<EMaterialColorFormat, GBufferAttachmentCount> Formats = DefaultGBufferFormats();
	static FGBufferLayout HighPrecision();
	void Validate(const FRHICapabilities& InCapabilities) const;
	std::uint32_t BytesPerPixel() const;
	bool operator==(const FGBufferLayout&) const = default;
};
} // namespace Hyperion
