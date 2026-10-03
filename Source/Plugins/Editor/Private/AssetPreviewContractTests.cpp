#include "AssetPreviewPresentation.h"
#include "AssetPreviewProducts.h"
#include "AssetTextureDisplay.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <source_location>

namespace
{
using namespace Hyperion;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Preview contract check failed: " + std::to_string(InLocation.line()));
	}
}

template<class TFunction> void Reject(TFunction InFunction)
{
	bool bRejected{};
	try
	{
		InFunction();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Check(bRejected);
}

void CheckShape(EAssetPreviewShape InId, std::uint32_t InWire, std::string_view InLabel, std::string_view InPath)
{
	Check(ParseAssetPreviewShape(InWire) == InId);
	Check(ToAssetPreviewShapeWireValue(InId) == InWire);
	const auto& Description = DescribeAssetPreviewShape(InId);
	Check(Description.Id == InId && Description.WireValue == InWire);
	Check(Description.Label == InLabel && Description.ModelPath == InPath);
}

void CheckChannel(EAssetPreviewChannel InId, std::uint32_t InWire, std::string_view InLabel,
                  std::optional<std::size_t> InComponent)
{
	Check(ParseAssetPreviewChannel(InWire) == InId);
	Check(ToAssetPreviewChannelWireValue(InId) == InWire);
	const auto& Description = DescribeAssetPreviewChannel(InId);
	Check(Description.Id == InId && Description.WireValue == InWire);
	Check(Description.Label == InLabel && Description.Component == InComponent);
}

void CheckContract()
{
	CheckShape(EAssetPreviewShape::Sphere, 0, "Sphere", "/Engine/Models/Primitives/Sphere.hasset");
	CheckShape(EAssetPreviewShape::Plane, 1, "Plane", "/Engine/Models/Primitives/Plane.hasset");
	CheckShape(EAssetPreviewShape::Cube, 2, "Cube", "/Engine/Models/Primitives/Cube.hasset");
	CheckChannel(EAssetPreviewChannel::Rgba, 0, "RGBA", {});
	CheckChannel(EAssetPreviewChannel::Red, 1, "R", 0);
	CheckChannel(EAssetPreviewChannel::Green, 2, "G", 1);
	CheckChannel(EAssetPreviewChannel::Blue, 3, "B", 2);
	CheckChannel(EAssetPreviewChannel::Alpha, 4, "A", 3);
	Check(AssetPreviewShapeOptions().size() == 3);
	Check(AssetPreviewChannelOptions().size() == 5);
	Check(AssetPreviewOptionLabels(AssetPreviewShapeOptions()) ==
	      std::vector<std::string>({"Sphere", "Plane", "Cube"}));
	Check(AssetPreviewOptionLabels(AssetPreviewChannelOptions()) ==
	      std::vector<std::string>({"RGBA", "R", "G", "B", "A"}));
	for (const auto Value : {3U, 4U, std::numeric_limits<std::uint32_t>::max()})
	{
		Reject(
		    [&]
		    {
			    (void)ParseAssetPreviewShape(Value);
		    });
	}
	for (const auto Value : {5U, 6U, std::numeric_limits<std::uint32_t>::max()})
	{
		Reject(
		    [&]
		    {
			    (void)ParseAssetPreviewChannel(Value);
		    });
	}
	Reject(
	    []
	    {
		    (void)ToAssetPreviewShapeWireValue(static_cast<EAssetPreviewShape>(99));
	    });
	Reject(
	    []
	    {
		    (void)ToAssetPreviewChannelWireValue(static_cast<EAssetPreviewChannel>(99));
	    });
}

void CheckPresentation()
{
	const auto CanonicalShapes = AssetPreviewShapeOptions();
	std::vector<FAssetPreviewShapeOption> Shapes(CanonicalShapes.rbegin(), CanonicalShapes.rend());
	Shapes.front().Label = "Relabelled cube";
	const auto ShapeView = std::span<const FAssetPreviewShapeOption>(Shapes);
	Check(AssetPreviewOptionIndex(ShapeView, EAssetPreviewShape::Cube) == 0);
	const auto Cube = AssetPreviewOptionIdentity(ShapeView, 0);
	Check(Cube == EAssetPreviewShape::Cube);
	Check(ToAssetPreviewShapeWireValue(Cube) == 2);
	Check(DescribeAssetPreviewShape(Cube).ModelPath == "/Engine/Models/Primitives/Cube.hasset");
	Check(AssetPreviewOptionLabels(ShapeView).front() == "Relabelled cube");
	const auto CanonicalChannels = AssetPreviewChannelOptions();
	std::vector<FAssetPreviewChannelOption> Channels(CanonicalChannels.rbegin(), CanonicalChannels.rend());
	Channels.front().Label = "Opacity";
	const auto ChannelView = std::span<const FAssetPreviewChannelOption>(Channels);
	Check(AssetPreviewOptionIndex(ChannelView, EAssetPreviewChannel::Alpha) == 0);
	const auto Alpha = AssetPreviewOptionIdentity(ChannelView, 0);
	Check(Alpha == EAssetPreviewChannel::Alpha);
	Check(ToAssetPreviewChannelWireValue(Alpha) == 4);
	Check(DescribeAssetPreviewChannel(Alpha).Component == 3);
	Check(AssetPreviewOptionLabels(ChannelView).front() == "Opacity");
	Reject(
	    [&]
	    {
		    (void)AssetPreviewOptionIdentity(ShapeView, ShapeView.size());
	    });
	Reject(
	    [&]
	    {
		    (void)AssetPreviewOptionIdentity(ChannelView, ChannelView.size());
	    });
	Reject(
	    [&]
	    {
		    (void)AssetPreviewOptionIndex(ShapeView.first(1), EAssetPreviewShape::Sphere);
	    });
	Reject(
	    [&]
	    {
		    (void)AssetPreviewOptionIndex(ChannelView.first(1), EAssetPreviewChannel::Rgba);
	    });
}

void CheckSkyProducts()
{
	const auto Descriptions = SkyPreviewProductDescriptions();
	Check(Descriptions.size() == 3);
	Check(Descriptions[0].Label == "Radiance" && Descriptions[0].Reference == &FSkyAsset::Radiance &&
	      Descriptions[0].Texture == &FSkyPreviewProducts::Radiance);
	Check(Descriptions[1].Label == "Specular" && Descriptions[1].Reference == &FSkyAsset::Specular &&
	      Descriptions[1].Texture == &FSkyPreviewProducts::Specular);
	Check(Descriptions[2].Label == "BRDF" && Descriptions[2].Reference == &FSkyAsset::Brdf &&
	      Descriptions[2].Texture == &FSkyPreviewProducts::Brdf);
	FSkyAsset Sky;
	Sky.Radiance.Path = "radiance-sentinel";
	Sky.Specular.Path = "specular-sentinel";
	Sky.Brdf.Path = "brdf-sentinel";
	FSkyPreviewProducts Products;
	for (auto Entry = Descriptions.rbegin(); Entry != Descriptions.rend(); ++Entry)
	{
		auto Texture = std::make_shared<FTextureAsset>();
		Texture->Name = (Sky.*Entry->Reference).Path;
		Products.*Entry->Texture = std::move(Texture);
	}
	Check(Products.Radiance->Name == "radiance-sentinel");
	Check(Products.Specular->Name == "specular-sentinel");
	Check(Products.Brdf->Name == "brdf-sentinel");
}

void CheckFormats()
{
	const auto Byte = DescribeAssetPreviewFormat(ETextureFormat::Rgba8Unorm);
	Check(Byte.Label == "RGBA8 UNORM" && Byte.CompactLabel == "RGBA8");
	const auto Half = DescribeAssetPreviewFormat(ETextureFormat::Rgba16Float);
	Check(Half.Label == "RGBA16 FLOAT" && Half.CompactLabel == "RGBA16F");
	const auto Float = DescribeAssetPreviewFormat(ETextureFormat::Rgba32Float);
	Check(Float.Label == "RGBA32 FLOAT" && Float.CompactLabel == "RGBA32F");
	Reject(
	    []
	    {
		    (void)DescribeAssetPreviewFormat(static_cast<ETextureFormat>(99));
	    });
}

void CheckPixel(const FTextureAsset& InTexture, EAssetPreviewChannel InChannel,
                const std::vector<std::uint8_t>& InExpected, float InExposure = 0, bool bInChecker = false)
{
	const auto Result = BuildAssetTextureDisplay(InTexture, 0, 0, InChannel, InExposure, bInChecker, {});
	if (Result.Mips.front().Bytes != InExpected)
	{
		std::cerr << "Pixel mismatch: " << InTexture.Name << " channel=" << ToAssetPreviewChannelWireValue(InChannel)
		          << " exposure=" << InExposure << " checker=" << bInChecker << " bytes=";
		for (const auto Byte : Result.Mips.front().Bytes)
		{
			std::cerr << unsigned(Byte) << ' ';
		}
		std::cerr << '\n';
	}
	Check(Result.Mips.front().Bytes == InExpected);
	Check(Result.Encoding == EMaterialTextureEncoding::Srgb && Result.Format == ETextureFormat::Rgba8Unorm);
}

void CheckPixels()
{
	const auto Source =
	    BuildTextureAsset("Channel sentinels", EMaterialTextureEncoding::Srgb, {1, 1, {32, 96, 160, 224}});
	CheckPixel(Source, EAssetPreviewChannel::Red, {32, 32, 32, 255});
	CheckPixel(Source, EAssetPreviewChannel::Green, {96, 96, 96, 255});
	CheckPixel(Source, EAssetPreviewChannel::Blue, {160, 160, 160, 255});
	CheckPixel(Source, EAssetPreviewChannel::Alpha, {224, 224, 224, 255});
	CheckPixel(Source, EAssetPreviewChannel::Rgba, {28, 84, 141, 255});
	CheckPixel(Source, EAssetPreviewChannel::Red, {32, 32, 32, 255}, 2, true);
	const auto Transparent = BuildTextureAsset("Transparent", EMaterialTextureEncoding::Srgb, {1, 1, {32, 96, 160, 0}});
	CheckPixel(Transparent, EAssetPreviewChannel::Rgba, {46, 46, 46, 255}, 0, true);
	CheckPixel(Transparent, EAssetPreviewChannel::Rgba, {0, 0, 0, 255});
	FTextureAsset Linear;
	Linear.Format = ETextureFormat::Rgba32Float;
	Linear.Mips.push_back({1, 1, std::vector<std::uint8_t>(16)});
	WriteTexturePixel(Linear.Mips.front(), Linear.Format, 0, {.25f, .5f, .75f, .5f});
	CheckPixel(Linear, EAssetPreviewChannel::Rgba, {68, 94, 112, 255});
	// The existing float sRGB conversion at 1 is just below 1; half-alpha rounds to 127.
	CheckPixel(Linear, EAssetPreviewChannel::Rgba, {94, 127, 127, 255}, 1);
	CheckPixel(Linear, EAssetPreviewChannel::Green, {128, 128, 128, 255}, 1, true);
	Reject(
	    [&]
	    {
		    (void)BuildAssetTextureDisplay(Source, 0, 0, static_cast<EAssetPreviewChannel>(99), 0, false, {});
	    });
}
} // namespace

int main()
{
	try
	{
		CheckContract();
		CheckPresentation();
		CheckSkyProducts();
		CheckFormats();
		CheckPixels();
		std::cout << "Preview identities, presentation, sky associations and fixed pixel expectations passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
