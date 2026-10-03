#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace Hyperion
{
enum class EAssetPreviewShape
{
	Sphere,
	Plane,
	Cube
};

enum class EAssetPreviewChannel
{
	Rgba,
	Red,
	Green,
	Blue,
	Alpha
};

struct FAssetPreviewShapeOption
{
	EAssetPreviewShape Id;
	std::uint32_t WireValue;
	std::string_view Label;
	std::string_view ModelPath;
};

struct FAssetPreviewChannelOption
{
	EAssetPreviewChannel Id;
	std::uint32_t WireValue;
	std::string_view Label;
	// A selected component is displayed as grayscale; no component means normal RGBA compositing.
	std::optional<std::size_t> Component;
};

std::span<const FAssetPreviewShapeOption> AssetPreviewShapeOptions();
std::span<const FAssetPreviewChannelOption> AssetPreviewChannelOptions();
const FAssetPreviewShapeOption& DescribeAssetPreviewShape(EAssetPreviewShape InId);
const FAssetPreviewChannelOption& DescribeAssetPreviewChannel(EAssetPreviewChannel InId);
EAssetPreviewShape ParseAssetPreviewShape(std::uint32_t InValue);
EAssetPreviewChannel ParseAssetPreviewChannel(std::uint32_t InValue);
std::uint32_t ToAssetPreviewShapeWireValue(EAssetPreviewShape InId);
std::uint32_t ToAssetPreviewChannelWireValue(EAssetPreviewChannel InId);
} // namespace Hyperion
