#pragma once
#include "Hyperion/ImageData/ImageData.h"
#include "Hyperion/Math/Math.h"
#include "Hyperion/Reflection/Reflection.h"
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace Hyperion
{
struct FVertex
{
	FVec3 Position;
	FVec4 Color{1, 1, 1, 1};
	FVec2 Uv;
};

struct FMesh
{
	std::vector<FVertex> Vertices;
	std::vector<std::uint32_t> Indices;
};

// PNG values remain sRGB encoded; EXR values remain linear. PNG writing quantizes to 8 bits.
FImage LoadImageFile(const std::filesystem::path& InPath);
void SaveImage(const std::filesystem::path& InPath, const FImage& InImage);

struct FAssetReference
{
	std::string Id;
	std::string Source;
};

const FTypeDescriptor& AssetReferenceType();

FImagePixels DecodeImage(std::span<const std::byte> InBytes);
// Bounded linear RGB/RGBA Radiance HDR or single-part EXR decoded from tracked IO bytes.
FImage DecodeHdrImage(std::span<const std::byte> InBytes);
std::vector<std::byte> EncodePng(const FImage& InImage);
} // namespace Hyperion
