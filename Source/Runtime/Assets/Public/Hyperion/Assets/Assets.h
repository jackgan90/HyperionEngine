#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Reflection/Reflection.h"
#include <cstdint>
#include <filesystem>
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

// Loads local-space data from one static triangle primitive, not a glTF scene.
FMesh LoadGltfPrimitive(const std::filesystem::path& InPath, std::size_t InMesh = 0, std::size_t InPrimitive = 0);
enum class EColorSpace
{
	Linear,
	Srgb
};

struct FImage
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	EColorSpace Encoding = EColorSpace::Linear;
	std::vector<float> Rgba;
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
} // namespace Hyperion
