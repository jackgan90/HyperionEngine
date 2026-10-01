#pragma once
#include <compare>
#include <string>
#include <string_view>

namespace Hyperion
{
struct FMaterialSemantic;
enum class EEngineSemantic
{
	None,
#define HYP_UNIFORM_FIELD(Type, Name, Scope) Name,
#define HYP_SHADER_VALUE(Type, Name, Scope) Name,
// Visitor setup and cleanup must bracket the declaration include.
// clang-format off
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include "Hyperion/Materials/EngineSemantics.inl"
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
	// clang-format on
	Count
};

std::string_view GetEngineSemanticName(EEngineSemantic InSemantic);
EEngineSemantic FindEngineSemantic(std::string_view InName);

// Common enums and static owner descriptors carry typed identities. Custom names stay owned and extensible.
class FMaterialSemanticId
{
public:
	FMaterialSemanticId() = default;
	FMaterialSemanticId(EEngineSemantic InBuiltin);

	// Generated owner descriptors have static lifetime and distinct typed identity.
	explicit FMaterialSemanticId(const FMaterialSemantic* InDescriptor) : Descriptor(InDescriptor)
	{
	}

	template<typename T>
	    requires requires(T InValue) { GetShaderSemantic(InValue); }
	FMaterialSemanticId(T InValue) : FMaterialSemanticId(GetShaderSemantic(InValue))
	{
	}

	FMaterialSemanticId(std::string_view InName);

	FMaterialSemanticId(const std::string& InName) : FMaterialSemanticId(std::string_view(InName))
	{
	}

	FMaterialSemanticId(const char* InName) : FMaterialSemanticId(std::string_view(InName))
	{
	}

	std::string_view GetName() const;

	EEngineSemantic GetBuiltin() const
	{
		return Builtin;
	}

	std::size_t GetStorageBytes() const
	{
		return Custom.capacity();
	}

	const FMaterialSemantic* GetDescriptor() const
	{
		return Descriptor;
	}

	bool IsBuiltin() const
	{
		return Builtin != EEngineSemantic::None || Descriptor != nullptr;
	}

	bool IsEmpty() const
	{
		return !IsBuiltin() && Custom.empty();
	}

	bool operator==(const FMaterialSemanticId&) const = default;
	auto operator<=>(const FMaterialSemanticId&) const = default;

private:
	EEngineSemantic Builtin = EEngineSemantic::None;
	const FMaterialSemantic* Descriptor{};
	std::string Custom;
};
} // namespace Hyperion
