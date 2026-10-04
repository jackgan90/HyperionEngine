#include "Hyperion/Renderer/GBufferLayout.h"
#include "Hyperion/RHI/RHICapabilities.h"
#include "Hyperion/Renderer/RenderPass.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr std::array Attachments{FGBufferAttachment{EGBufferRole::BaseMetallic, 0, EDeferredLightingSemantic::GBuffer0,
                                                    EMaterialColorFormat::Rgba8Unorm, EGBufferStorage::Color},
                                 FGBufferAttachment{EGBufferRole::Normals, 1, EDeferredLightingSemantic::GBuffer1,
                                                    EMaterialColorFormat::Rgba16Float, EGBufferStorage::FloatingPoint},
                                 FGBufferAttachment{EGBufferRole::Surface, 2, EDeferredLightingSemantic::GBuffer2,
                                                    EMaterialColorFormat::Rgba8Unorm, EGBufferStorage::Color},
                                 FGBufferAttachment{EGBufferRole::Emissive, 3, EDeferredLightingSemantic::GBuffer3,
                                                    EMaterialColorFormat::Rgba16Float, EGBufferStorage::FloatingPoint}};

constexpr bool ValidAttachments()
{
	std::array<bool, GBufferAttachmentCount> Roles{};
	std::array<bool, GBufferAttachmentCount> Slots{};
	for (const auto& Attachment : Attachments)
	{
		const auto Role = static_cast<std::size_t>(Attachment.Role);
		if (Role >= Roles.size() || Attachment.Slot >= Slots.size() || Roles[Role] || Slots[Attachment.Slot])
		{
			return false;
		}
		Roles[Role] = true;
		Slots[Attachment.Slot] = true;
		for (const auto& Other : Attachments)
		{
			if (Attachment.Role != Other.Role && Attachment.Semantic == Other.Semantic)
			{
				return false;
			}
		}
	}
	return Attachments.size() == GBufferAttachmentCount;
}

static_assert(ValidAttachments());
} // namespace

std::span<const FGBufferAttachment> GBufferAttachments()
{
	return Attachments;
}

const FGBufferAttachment& DescribeGBufferAttachment(EGBufferRole InRole)
{
	for (const auto& Attachment : Attachments)
	{
		if (Attachment.Role == InRole)
		{
			return Attachment;
		}
	}
	throw std::invalid_argument("Unknown GBuffer attachment role");
}

bool SupportsGBufferFormat(EGBufferRole InRole, EMaterialColorFormat InFormat)
{
	const auto Storage = DescribeGBufferAttachment(InRole).Storage;
	switch (InFormat)
	{
		case EMaterialColorFormat::Rgba8Unorm:
			return Storage == EGBufferStorage::Color;
		case EMaterialColorFormat::Rgba16Float:
		case EMaterialColorFormat::Rgba32Float:
			return true;
		default:
			return false;
	}
}

std::array<EMaterialColorFormat, GBufferAttachmentCount> DefaultGBufferFormats()
{
	std::array<EMaterialColorFormat, GBufferAttachmentCount> Result{};
	for (const auto& Attachment : Attachments)
	{
		Result[Attachment.Slot] = Attachment.DefaultFormat;
	}
	return Result;
}

FGBufferLayout FGBufferLayout::HighPrecision()
{
	FGBufferLayout Result;
	Result.Formats.fill(EMaterialColorFormat::Rgba16Float);
	return Result;
}

void FGBufferLayout::Validate(const FRHICapabilities& InCapabilities) const
{
	if (InCapabilities.MaxColorTargets < Formats.size() || !InCapabilities.bSampledDepthTargets)
	{
		throw std::invalid_argument("Deferred requires four color targets and sampled D32 depth");
	}
	for (const auto& Attachment : Attachments)
	{
		const auto Format = Formats[Attachment.Slot];
		if (!SupportsGBufferFormat(Attachment.Role, Format))
		{
			throw std::invalid_argument(
			    "GBuffer requires RGBA storage; normals and HDR emissive require floating point");
		}
		if (!InCapabilities.SampledColorTargets.at(static_cast<std::size_t>(GetRenderColorFormat(Format))))
		{
			throw std::invalid_argument("GBuffer format lacks sampled render-target support");
		}
	}
}

std::uint32_t FGBufferLayout::BytesPerPixel() const
{
	std::uint32_t Result{};
	for (const auto Format : Formats)
	{
		switch (Format)
		{
			case EMaterialColorFormat::Rgba8Unorm:
				Result += 4;
				break;
			case EMaterialColorFormat::Rgba16Float:
				Result += 8;
				break;
			case EMaterialColorFormat::Rgba32Float:
				Result += 16;
				break;
			default:
				throw std::invalid_argument("Unknown GBuffer format");
		}
	}
	return Result;
}
} // namespace Hyperion
