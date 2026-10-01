#pragma once
#include "Hyperion/Materials/MaterialResources.h"
#include "Hyperion/Materials/MaterialSemanticId.h"
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
	Sampler,
	TextureCube
};

constexpr bool IsMaterialTexture(EMaterialValueKind InKind)
{
	return InKind == EMaterialValueKind::Texture2D || InKind == EMaterialValueKind::TextureCube;
}

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
	FMaterialSemanticId Semantic;
	std::vector<std::string> Targets;
	EMaterialParameterSource Source = EMaterialParameterSource::Manual;
	EMaterialOverridePolicy OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	std::uint32_t OverrideScopes = MaterialScopeBit(EMaterialScope::Material) |
	                               MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::Draw);
	bool bRequired = true;
	bool bActive = true;
	std::optional<FMaterialValue> Default;
};

struct FMaterialParameterHandle
{
	std::uint64_t SchemaIdentity{};
	std::uint64_t SchemaVersion{};
	std::size_t Index{};
	bool operator==(const FMaterialParameterHandle&) const = default;
};

class FMaterialParameterSchema;

// Names are authored boundary inputs. Runtime values retain semantic IDs or schema-bound handles.
struct FMaterialParameterEntry
{
	FMaterialParameterEntry() = default;

	FMaterialParameterEntry(std::string InName, FMaterialValue InValue)
	    : Name(std::move(InName)), Value(std::move(InValue))
	{
	}

	FMaterialParameterEntry(const char* InName, FMaterialValue InValue)
	    : FMaterialParameterEntry(std::string(InName), std::move(InValue))
	{
	}

	FMaterialParameterEntry(FMaterialSemanticId InSemantic, FMaterialValue InValue)
	    : Semantic(std::move(InSemantic)), Value(std::move(InValue))
	{
	}

	FMaterialParameterEntry(EEngineSemantic InSemantic, FMaterialValue InValue)
	    : FMaterialParameterEntry(FMaterialSemanticId(InSemantic), std::move(InValue))
	{
	}

	FMaterialParameterEntry(FMaterialParameterHandle InHandle, FMaterialValue InValue)
	    : Handle(InHandle), Value(std::move(InValue))
	{
	}

	FMaterialSemanticId GetSemantic() const;
	FMaterialParameterHandle Resolve(const FMaterialParameterSchema& InSchema) const;
	std::string_view GetName(const FMaterialParameterSchema& InSchema) const;

	std::string Name;
	FMaterialSemanticId Semantic;
	FMaterialParameterHandle Handle;
	FMaterialValue Value;
	bool operator==(const FMaterialParameterEntry&) const = default;
};

using FMaterialParameterValues = std::vector<FMaterialParameterEntry>;

struct FMaterialParameterIdentityToken
{
	std::uint64_t Identity{};
};

struct FMaterialParameterIdentity
{
	std::shared_ptr<const FMaterialParameterIdentityToken> AuthorIdentity;
	FMaterialSemanticId Semantic;
	bool operator==(const FMaterialParameterIdentity&) const = default;
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
	// Exact authored identity only; shader target aliases never participate in schema bridging.
	FMaterialParameterHandle FindAuthorIdentity(
	    const std::shared_ptr<const FMaterialParameterIdentityToken>& InIdentity) const;
	FMaterialParameterHandle FindSemantic(FMaterialSemanticId InSemantic) const;
	std::vector<FMaterialParameterHandle> FindSemantics(FMaterialSemanticId InSemantic) const;
	FMaterialParameterHandle GetHandle(std::size_t InIndex) const;
	const FMaterialParameterIdentity& GetParameterIdentity(std::size_t InIndex) const;
	const FMaterialParameterDeclaration& Get(FMaterialParameterHandle InHandle) const;

private:
	std::uint64_t Identity;
	std::uint64_t Version;
	bool bPrepared;
	std::vector<FMaterialParameterDeclaration> Parameters;
	std::vector<FMaterialParameterIdentity> ParameterIdentities;
	std::map<std::shared_ptr<const FMaterialParameterIdentityToken>, std::size_t> AuthorIdentities;
	std::map<std::string, std::size_t, std::less<>> Names;
	std::map<std::string, std::vector<std::size_t>, std::less<>> Aliases;
	std::map<FMaterialSemanticId, std::vector<std::size_t>> SemanticNames;
	void BuildLookup();
};

void ValidateMaterialOverride(const FMaterialParameterDeclaration& InParameter, const FMaterialValue& InValue,
                              EMaterialScope InScope);
} // namespace Hyperion
