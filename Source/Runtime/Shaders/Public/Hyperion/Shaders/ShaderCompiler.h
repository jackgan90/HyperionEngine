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
	Sampler,
	StructuredBuffer,
	RawBuffer,
	Unsupported
};

enum class EShaderScalar
{
	Bool,
	Int,
	Uint,
	Float,
	Unsupported
};
enum class EShaderValueKind
{
	Numeric,
	Structure,
	Array
};
enum class EShaderResourceDimension
{
	None,
	Buffer,
	Texture2D,
	Unsupported
};

struct FShaderMember
{
	std::string Name;
	EShaderValueKind Kind = EShaderValueKind::Numeric;
	EShaderScalar Scalar = EShaderScalar::Float;
	std::uint32_t Rows = 1;
	std::uint32_t Columns = 1;
	std::uint32_t Offset{};
	std::uint32_t Size{};
	std::uint32_t ArrayCount{};
	std::uint32_t ArrayStride{};
	std::uint32_t MatrixStride{};
	bool bRowMajor{};
	bool bActive = true;
	std::vector<FShaderMember> Members;
	bool operator==(const FShaderMember&) const = default;
};

struct FShaderSignatureParameter
{
	std::string Semantic;
	std::uint32_t SemanticIndex{};
	std::uint32_t Location{};
	EShaderScalar Scalar = EShaderScalar::Float;
	std::uint32_t Components{};
	bool bSystemValue{};
	bool operator==(const FShaderSignatureParameter&) const = default;
};

struct FShaderBinding
{
	std::string Name;
	EBindingKind Kind;
	std::uint32_t Binding{};
	std::uint32_t Space{};
	std::uint32_t ByteSize{};
	std::uint32_t Register{}; // Original HLSL register; Binding is the actual target location.
	std::uint32_t Count = 1;
	EShaderStage Stage = EShaderStage::Vertex;
	EShaderResourceDimension Dimension = EShaderResourceDimension::None;
	bool bComparison{};
	std::uint32_t MslBinding = 0xffffffffU;
	std::vector<FShaderMember> Members;
	EShaderScalar ResourceScalar = EShaderScalar::Float; // Sampled texture component type, before format conversion.
	std::uint32_t StructureByteStride{};
	bool operator==(const FShaderBinding&) const = default;
};

struct FShaderReflection
{
	std::uint32_t Version = 5;
	EShaderFormat LayoutFormat = EShaderFormat::Dxil;
	std::vector<FShaderSignatureParameter> Inputs;
	std::vector<FShaderSignatureParameter> Outputs;
	bool operator==(const FShaderReflection&) const = default;
};

struct FShaderDefine
{
	std::string Name;
	std::string Value;
	bool operator==(const FShaderDefine&) const = default;
};

struct FShaderCompileOptions
{
	std::vector<FShaderDefine> Defines;
	bool bOptimize = true;
	bool operator==(const FShaderCompileOptions&) const = default;
};

inline constexpr std::uint32_t ShaderBindingMappingVersion = 2;
inline constexpr std::uint32_t ShaderRegisterSpaceCount = 4;
inline constexpr std::uint32_t ShaderRegistersPerKind = 1000;

struct FShaderArtifact
{
	EShaderFormat Format = EShaderFormat::Dxil;
	std::vector<std::uint8_t> Bytes;
	std::vector<FShaderBinding> Bindings;
	std::string CacheKey;
	bool bCacheHit{};
	EShaderStage Stage = EShaderStage::Vertex;
	FShaderReflection Reflection;
};

class FShaderCompiler
{
public:
	FShaderCompiler(std::filesystem::path InSourceRoot, std::filesystem::path InCacheRoot);
	~FShaderCompiler();
	FShaderArtifact Compile(const std::filesystem::path& InSource, std::string InEntry, EShaderStage InStage,
	                        EShaderFormat InFormat);
	FShaderArtifact Compile(const std::filesystem::path& InSource, std::string InEntry, EShaderStage InStage,
	                        EShaderFormat InFormat, FShaderCompileOptions InOptions);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
