#include "LocalLightEncoding.h"
#include <array>
#include <bit>
#include <stdexcept>

namespace Hyperion::RendererPrivate
{
namespace
{
std::array<std::uint32_t, 4> Words(FVec4 InValue)
{
	return {std::bit_cast<std::uint32_t>(InValue.X), std::bit_cast<std::uint32_t>(InValue.Y),
	        std::bit_cast<std::uint32_t>(InValue.Z), std::bit_cast<std::uint32_t>(InValue.W)};
}

void CheckEncoding(const FLocalLight& InLight, const std::array<std::uint32_t, 4>& InExpected)
{
	if (Words(EncodeLightConeRange(InLight)) != InExpected)
	{
		throw std::runtime_error("Volume cone/range fields differ from independent fixed words");
	}
	const auto Cluster = EncodeClusterLight(InLight);
	if (Words({Cluster.PositionRange.W, Cluster.DirectionInner.W, Cluster.Outer.X, Cluster.RadianceType.W}) !=
	    InExpected)
	{
		throw std::runtime_error("Cluster attenuation fields differ from independent fixed words");
	}
}
} // namespace

void CheckLocalLightEncodingWords()
{
	FLocalLight Point;
	Point.Range = 8;
	Point.InnerCos = .875f;
	Point.OuterCos = .375f;
	CheckEncoding(Point, {0x3e000000U, 0x3f600000U, 0x3ec00000U, 0x00000000U});
	FLocalLight Spot;
	Spot.Range = 16;
	Spot.InnerCos = .75f;
	Spot.OuterCos = .25f;
	Spot.bSpot = true;
	CheckEncoding(Spot, {0x3d800000U, 0x3f400000U, 0x3e800000U, 0x3f800000U});
}
} // namespace Hyperion::RendererPrivate
