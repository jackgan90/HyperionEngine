#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Hyperion
{
enum class EShaderStage
{
	Vertex,
	Pixel
};
enum class EShaderFormat
{
	Dxil,
	Spirv,
	Msl
};
enum class EBindingKind
{
	UniformBuffer,
	Texture,
	Sampler
};

struct FShaderBinding
{
	std::string Name;
	EBindingKind Kind;
	std::uint32_t Binding{};
	std::uint32_t Space{};
	std::uint32_t ByteSize{};
};

struct FShaderArtifact
{
	EShaderFormat Format;
	std::vector<std::uint8_t> Bytes;
	std::vector<FShaderBinding> Bindings; // SPIR-V/MSL resource metadata; DXIL stays opaque.
	std::string CacheKey;
	bool bCacheHit{};
};

class FShaderCompiler
{
public:
	FShaderCompiler(std::filesystem::path InSourceRoot, std::filesystem::path InCacheRoot);
	~FShaderCompiler();
	FShaderArtifact Compile(const std::filesystem::path& InSource, std::string InEntry, EShaderStage InStage,
	                        EShaderFormat InFormat);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
