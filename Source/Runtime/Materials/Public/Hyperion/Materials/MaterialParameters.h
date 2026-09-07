#pragma once
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/Math/Math.h"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace Hyperion
{
enum class EMaterialValueKind : std::uint8_t
{
	Numeric,
	Structure,
	Array,
	Texture2D,
	ReadBuffer,
	Sampler
};

enum class EMaterialScalar : std::uint8_t
{
	Bool,
	Int,
	Uint,
	Float
};

// CPU numeric values use row-major logical order; target packing is a Renderer responsibility.
struct FMaterialParameterType
{
	EMaterialValueKind Kind = EMaterialValueKind::Numeric;
	EMaterialScalar Scalar = EMaterialScalar::Float;
	std::uint32_t Rows = 1;
	std::uint32_t Columns = 1;
	std::uint32_t ArrayCount{};
	std::vector<std::string> MemberNames;
	std::vector<FMaterialParameterType> Members;
	static FMaterialParameterType Numeric(EMaterialScalar InScalar, std::uint32_t InColumns = 1,
	                                      std::uint32_t InRows = 1);
	static FMaterialParameterType Resource(EMaterialValueKind InKind);
	static FMaterialParameterType Array(FMaterialParameterType InElement, std::uint32_t InCount);
	void Validate(std::uint32_t InDepth = 0) const;
	bool operator==(const FMaterialParameterType&) const = default;
};

struct FMaterialValue
{
	FMaterialParameterType Type;
	std::vector<std::uint32_t> Words;
	std::vector<FMaterialValue> Elements;
	std::shared_ptr<const FMaterialTextureSource> Texture;
	FMaterialBufferView Buffer;
	FMaterialSampler Sampler;
	static FMaterialValue Float(float InValue);
	static FMaterialValue Float(FVec2 InValue);
	static FMaterialValue Float(FVec3 InValue);
	static FMaterialValue Float(FVec4 InValue);
	static FMaterialValue Matrix(const FMat4& InValue);
	static FMaterialValue Floats(std::span<const float> InValues, std::uint32_t InRows = 1);
	static FMaterialValue Int(std::int32_t InValue);
	static FMaterialValue Uint(std::uint32_t InValue);
	static FMaterialValue Bool(bool bInValue);
	static FMaterialValue FromTexture(std::shared_ptr<const FMaterialTextureSource> InTexture);
	static FMaterialValue FromBuffer(FMaterialBufferView InBuffer);
	static FMaterialValue FromSampler(FMaterialSampler InSampler);
	static FMaterialValue Array(std::vector<FMaterialValue> InElements);
	void Validate() const;
	bool operator==(const FMaterialValue&) const = default;
};

enum class EMaterialScope : std::uint8_t
{
	Global,
	Frame,
	Scene,
	View,
	Pass,
	Material,
	Object,
	Draw,
	Count
};

constexpr std::uint32_t MaterialScopeBit(EMaterialScope InScope)
{
	return 1U << static_cast<unsigned>(InScope);
}

enum class EMaterialParameterSource : std::uint8_t
{
	Manual,
	Semantic
};

enum class EMaterialOverridePolicy : std::uint8_t
{
	Locked,
	AllowOverride
};

struct FMaterialParameterDeclaration
{
	std::string Name;
	FMaterialParameterType Type;
	std::string Semantic;
	std::vector<std::string> Targets;
	EMaterialParameterSource Source = EMaterialParameterSource::Manual;
	EMaterialOverridePolicy OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	std::uint32_t OverrideScopes = MaterialScopeBit(EMaterialScope::Material) |
	                               MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::Draw);
	bool bRequired = true;
	bool bActive = true;
	std::optional<FMaterialValue> Default;
};

struct FMaterialParameterEntry
{
	std::string Name;
	FMaterialValue Value;
	bool operator==(const FMaterialParameterEntry&) const = default;
};

using FMaterialParameterValues = std::vector<FMaterialParameterEntry>;

struct FMaterialParameterHandle
{
	std::uint64_t SchemaIdentity{};
	std::uint64_t SchemaVersion{};
	std::size_t Index{};
};

class FMaterialParameterSchema
{
public:
	explicit FMaterialParameterSchema(std::vector<FMaterialParameterDeclaration> InParameters,
	                                  std::uint64_t InVersion = 1, bool bInPrepared = true);
	FMaterialParameterSchema(const FMaterialParameterSchema&) = delete;
	FMaterialParameterSchema& operator=(const FMaterialParameterSchema&) = delete;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetVersion() const;
	bool IsPrepared() const;
	const std::vector<FMaterialParameterDeclaration>& GetParameters() const;
	FMaterialParameterHandle Find(std::string_view InName) const;
	FMaterialParameterHandle FindSemantic(std::string_view InSemantic) const;
	const FMaterialParameterDeclaration& Get(FMaterialParameterHandle InHandle) const;

private:
	std::uint64_t Identity;
	std::uint64_t Version;
	bool bPrepared;
	std::vector<FMaterialParameterDeclaration> Parameters;
	std::map<std::string, std::size_t, std::less<>> Names;
	std::map<std::string, std::vector<std::size_t>, std::less<>> Aliases;
	std::map<std::string, std::vector<std::size_t>, std::less<>> SemanticNames;
	void BuildLookup();
};

void ValidateMaterialOverride(const FMaterialParameterDeclaration& InParameter, const FMaterialValue& InValue,
                              EMaterialScope InScope);
} // namespace Hyperion
