#include "Hyperion/AssetEditing/AssetPreviewOptions.h"
#include <array>
#include <stdexcept>

namespace Hyperion
{
namespace
{
constexpr std::array Shapes{
    FAssetPreviewShapeOption{EAssetPreviewShape::Sphere, 0, "Sphere", "/Engine/Models/Primitives/Sphere.hasset"},
    FAssetPreviewShapeOption{EAssetPreviewShape::Plane, 1, "Plane", "/Engine/Models/Primitives/Plane.hasset"},
    FAssetPreviewShapeOption{EAssetPreviewShape::Cube, 2, "Cube", "/Engine/Models/Primitives/Cube.hasset"}};
constexpr std::array Channels{FAssetPreviewChannelOption{EAssetPreviewChannel::Rgba, 0, "RGBA", {}},
                              FAssetPreviewChannelOption{EAssetPreviewChannel::Red, 1, "R", 0},
                              FAssetPreviewChannelOption{EAssetPreviewChannel::Green, 2, "G", 1},
                              FAssetPreviewChannelOption{EAssetPreviewChannel::Blue, 3, "B", 2},
                              FAssetPreviewChannelOption{EAssetPreviewChannel::Alpha, 4, "A", 3}};
} // namespace

std::span<const FAssetPreviewShapeOption> AssetPreviewShapeOptions()
{
	return Shapes;
}

std::span<const FAssetPreviewChannelOption> AssetPreviewChannelOptions()
{
	return Channels;
}

const FAssetPreviewShapeOption& DescribeAssetPreviewShape(EAssetPreviewShape InId)
{
	for (const auto& Option : Shapes)
	{
		if (Option.Id == InId)
		{
			return Option;
		}
	}
	throw std::invalid_argument("Unknown asset preview shape");
}

const FAssetPreviewChannelOption& DescribeAssetPreviewChannel(EAssetPreviewChannel InId)
{
	for (const auto& Option : Channels)
	{
		if (Option.Id == InId)
		{
			return Option;
		}
	}
	throw std::invalid_argument("Unknown asset preview channel");
}

EAssetPreviewShape ParseAssetPreviewShape(std::uint32_t InValue)
{
	for (const auto& Option : Shapes)
	{
		if (Option.WireValue == InValue)
		{
			return Option.Id;
		}
	}
	throw std::invalid_argument("Unknown asset preview shape value");
}

EAssetPreviewChannel ParseAssetPreviewChannel(std::uint32_t InValue)
{
	for (const auto& Option : Channels)
	{
		if (Option.WireValue == InValue)
		{
			return Option.Id;
		}
	}
	throw std::invalid_argument("Unknown asset preview channel value");
}

std::uint32_t ToAssetPreviewShapeWireValue(EAssetPreviewShape InId)
{
	return DescribeAssetPreviewShape(InId).WireValue;
}

std::uint32_t ToAssetPreviewChannelWireValue(EAssetPreviewChannel InId)
{
	return DescribeAssetPreviewChannel(InId).WireValue;
}
} // namespace Hyperion
