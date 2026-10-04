#include "Hyperion/RHI/RHICapabilities.h"
#include "Hyperion/Renderer/GBufferLayout.h"
#include "Support/TestSupport.h"
#include <functional>
#include <iostream>

using namespace Hyperion;

namespace
{
void Rejects(const std::function<void()>& InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckLegacyMapping()
{
	struct FExpected
	{
		EGBufferRole Role;
		std::size_t Slot;
		EDeferredLightingSemantic Semantic;
		EMaterialColorFormat Format;
	};

	const std::array Expected{
	    FExpected{EGBufferRole::BaseMetallic, 0, EDeferredLightingSemantic::GBuffer0, EMaterialColorFormat::Rgba8Unorm},
	    FExpected{EGBufferRole::Normals, 1, EDeferredLightingSemantic::GBuffer1, EMaterialColorFormat::Rgba16Float},
	    FExpected{EGBufferRole::Surface, 2, EDeferredLightingSemantic::GBuffer2, EMaterialColorFormat::Rgba8Unorm},
	    FExpected{EGBufferRole::Emissive, 3, EDeferredLightingSemantic::GBuffer3, EMaterialColorFormat::Rgba16Float}};
	HYP_CHECK(GBufferAttachments().size() == Expected.size());
	const FGBufferLayout Default;
	std::array<int, GBufferAttachmentCount> Values{41, 53, 67, 79};
	for (const auto& Entry : Expected)
	{
		const auto& Actual = DescribeGBufferAttachment(Entry.Role);
		HYP_CHECK(Actual.Slot == Entry.Slot && Actual.Semantic == Entry.Semantic);
		HYP_CHECK(Actual.DefaultFormat == Entry.Format);
		HYP_CHECK(GBufferAttachment(Default.Formats, Entry.Role) == Entry.Format);
		HYP_CHECK(&GBufferAttachment(Values, Entry.Role) == &Values[Entry.Slot]);
	}
	GBufferAttachment(Values, EGBufferRole::Surface) = 91;
	HYP_CHECK(Values[2] == 91);
	HYP_CHECK(Default.BytesPerPixel() == 24 && FGBufferLayout::HighPrecision().BytesPerPixel() == 32);
	Rejects(
	    []
	    {
		    (void)DescribeGBufferAttachment(EGBufferRole::Count);
	    });
	Rejects(
	    []
	    {
		    (void)DescribeGBufferAttachment(static_cast<EGBufferRole>(-1));
	    });
}

void CheckFormats()
{
	FRHICapabilities Caps;
	Caps.MaxColorTargets = 4;
	Caps.bSampledDepthTargets = true;
	Caps.SampledColorTargets.fill(true);
	FGBufferLayout{}.Validate(Caps);
	FGBufferLayout::HighPrecision().Validate(Caps);
	for (const auto& Attachment : GBufferAttachments())
	{
		for (const auto Format :
		     {EMaterialColorFormat::Rgba8Unorm, EMaterialColorFormat::Rgba16Float, EMaterialColorFormat::Rgba32Float,
		      EMaterialColorFormat::R32Float, EMaterialColorFormat::R8Unorm, static_cast<EMaterialColorFormat>(255)})
		{
			const bool bFloating =
			    Format == EMaterialColorFormat::Rgba16Float || Format == EMaterialColorFormat::Rgba32Float;
			const bool bColor =
			    Attachment.Role == EGBufferRole::BaseMetallic || Attachment.Role == EGBufferRole::Surface;
			const bool bExpected = bFloating || (bColor && Format == EMaterialColorFormat::Rgba8Unorm);
			HYP_CHECK(SupportsGBufferFormat(Attachment.Role, Format) == bExpected);
			FGBufferLayout Layout;
			GBufferAttachment(Layout.Formats, Attachment.Role) = Format;
			if (bExpected)
			{
				Layout.Validate(Caps);
			}
			else
			{
				Rejects(
				    [&]
				    {
					    Layout.Validate(Caps);
				    });
			}
		}
	}
	Caps.MaxColorTargets = 3;
	Rejects(
	    [&]
	    {
		    FGBufferLayout{}.Validate(Caps);
	    });
	Caps.MaxColorTargets = 4;
	Caps.bSampledDepthTargets = false;
	Rejects(
	    [&]
	    {
		    FGBufferLayout{}.Validate(Caps);
	    });
	Caps.bSampledDepthTargets = true;
	Caps.SampledColorTargets[static_cast<std::size_t>(ERHIColorFormat::Rgba16Float)] = false;
	Rejects(
	    [&]
	    {
		    FGBufferLayout{}.Validate(Caps);
	    });
}
} // namespace

int main()
{
	try
	{
		CheckLegacyMapping();
		CheckFormats();
		std::cout << "GBuffer role mapping and layout constraints passed\n";
		return 0;
	}
	catch (const std::exception& Failure)
	{
		std::cerr << Failure.what() << '\n';
		return 1;
	}
}
