// Reusable typed visitors. Rendering owners supply their own declaration file.
#define HYP_DETAIL_JOIN_INNER(A, B) A##B
#define HYP_DETAIL_JOIN(A, B) HYP_DETAIL_JOIN_INNER(A, B)
#define HYP_DETAIL_STRING_INNER(A) #A
#define HYP_DETAIL_STRING(A) HYP_DETAIL_STRING_INNER(A)
#define HYP_DOMAIN_SEMANTIC HYP_DETAIL_JOIN(HYP_DETAIL_JOIN(E, HYP_SHADER_DOMAIN), Semantic)
#define HYP_DOMAIN_UNIFORM HYP_DETAIL_JOIN(HYP_DETAIL_JOIN(E, HYP_SHADER_DOMAIN), Uniform)
#define HYP_DOMAIN_FUNCTION(Suffix) HYP_DETAIL_JOIN(HYP_DETAIL_JOIN(Get, HYP_SHADER_DOMAIN), Suffix)
#define HYP_FIELD_ENUM(Name) HYP_DETAIL_JOIN(HYP_DETAIL_JOIN(E, Name), Field)
#define HYP_FIELD_TYPE(Type) HYP_DETAIL_JOIN(HYP_SHADER_TYPE_, Type)

namespace Hyperion
{
#ifndef HYP_SHADER_COMMON
enum class HYP_DOMAIN_SEMANTIC
{
	None,
#define HYP_SHADER_VALUE(Type, Name, Scope) Name,
#define HYP_TEXTURE_2D(Type, Name, Scope) Name,
#define HYP_TEXTURE_CUBE(Type, Name, Scope) Name,
#define HYP_RW_TEXTURE_2D(Type, Name, Scope) Name,
#define HYP_SAMPLER(Name, Scope) Name,
#define HYP_COMPARISON_SAMPLER(Name, Scope) Name,
#define HYP_READ_BUFFER(Type, Name, Scope) Name,
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
	Count
};
#endif

enum class HYP_DOMAIN_UNIFORM
{
#define HYP_UNIFORM_BEGIN(Name, Instance) Name,
#define HYP_UNIFORM_FLAT_BEGIN(Name) Name,
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
	Count
};

#define HYP_UNIFORM_BEGIN(Name, Instance)                                                                              \
	enum class HYP_FIELD_ENUM(Name)                                                                                    \
	{
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#define HYP_UNIFORM_FIELD(Type, Name, Scope) Name,
#define HYP_UNIFORM_END()                                                                                              \
	Count                                                                                                              \
	}                                                                                                                  \
	;
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"

inline const std::vector<FMaterialSemantic>& HYP_DOMAIN_FUNCTION(ShaderSemantics)()
{
	static const auto Values = []
	{
		std::vector<FMaterialSemantic> Result;
		const std::string Domain = HYP_DETAIL_STRING(HYP_SHADER_DOMAIN);
		std::string UniformPrefix;
		std::string ResourcePrefix = Domain;
		FEngineSemanticPolicy Policy;
#define HYP_CONTRACT_POLICY(Group, SceneOwned, DefaultInput)                                                           \
	Policy = {EEngineSemanticGroup::Group, SceneOwned, DefaultInput};
#define HYP_RESOURCE_NAMESPACE(Wire) ResourcePrefix = Wire;
#define HYP_UNIFORM_BEGIN(Name, Instance) UniformPrefix = Domain + "." #Name;
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#define HYP_UNIFORM_WIRE_NAMESPACE(Wire) UniformPrefix = Wire;
#define HYP_UNIFORM_FIELD(Type, Name, Scope)                                                                           \
	Result.push_back({UniformPrefix + "." #Name,                                                                       \
	                  HYP_FIELD_TYPE(Type)::GetType(),                                                                 \
	                  EMaterialScope::Scope,                                                                           \
	                  "Shader uniform contract",                                                                       \
	                  true,                                                                                            \
	                  {},                                                                                              \
	                  Policy});
#define HYP_SHADER_VALUE(Type, Name, Scope)                                                                            \
	Result.push_back({Domain + "." #Name,                                                                              \
	                  HYP_FIELD_TYPE(Type)::GetType(),                                                                 \
	                  EMaterialScope::Scope,                                                                           \
	                  "Shader value contract",                                                                         \
	                  true,                                                                                            \
	                  {},                                                                                              \
	                  Policy});
#define HYP_DETAIL_RESOURCE(Name, Scope, Kind)                                                                         \
	Result.push_back({ResourcePrefix + "." #Name,                                                                      \
	                  FMaterialParameterType::Resource(EMaterialValueKind::Kind),                                      \
	                  EMaterialScope::Scope,                                                                           \
	                  "Shader resource contract",                                                                      \
	                  true,                                                                                            \
	                  {},                                                                                              \
	                  Policy});
#define HYP_TEXTURE_2D(Type, Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, Texture2D)
#define HYP_TEXTURE_CUBE(Type, Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, TextureCube)
#define HYP_RW_TEXTURE_2D(Type, Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, Texture2D)
#define HYP_SAMPLER(Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, Sampler)
#define HYP_COMPARISON_SAMPLER(Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, Sampler)
#define HYP_READ_BUFFER(Type, Name, Scope) HYP_DETAIL_RESOURCE(Name, Scope, ReadBuffer)
#define HYP_SEMANTIC_WIRE(Wire) Result.back().Name = Wire;
#define HYP_SEMANTIC_CONVENTION(InConvention) Result.back().Convention = InConvention;
#define HYP_SEMANTIC_ALIAS(Alias) Result.back().Aliases.push_back(Alias);
#define HYP_SEMANTIC_POLICY(Group, SceneOwned, DefaultInput, Hint)                                                     \
	Result.back().Policy = {EEngineSemanticGroup::Group, SceneOwned, DefaultInput, EMaterialEditHint::Hint};
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
#undef HYP_DETAIL_RESOURCE
		return Result;
	}();
	return Values;
}

inline const std::shared_ptr<const FShaderParameterContractSet>& HYP_DOMAIN_FUNCTION(ShaderContracts)()
{
	static const auto Contracts = []
	{
		auto Result = std::make_shared<FShaderParameterContractSet>();
		Result->IncludeName = HYP_DETAIL_STRING(HYP_SHADER_DOMAIN) "Parameters.generated.hlsli";
		const auto& Semantics = HYP_DOMAIN_FUNCTION(ShaderSemantics)();
		for (const auto& Semantic : Semantics)
		{
			Result->Semantics.push_back(&Semantic);
		}
		std::size_t SemanticIndex = 0;
#ifdef HYP_SHADER_COMMON
		const auto NextSemantic = [&]
		{
			return FMaterialSemanticId(static_cast<EEngineSemantic>(++SemanticIndex));
		};
#else
		const auto NextSemantic = [&]
		{
			return FMaterialSemanticId(&Semantics.at(SemanticIndex++));
		};
#endif
		std::vector<std::pair<std::size_t, std::string>> ResourceTypes;
		std::vector<std::pair<std::string, FStandardMaterialBlock>> ElementLayouts;
#define HYP_SHADER_CONTRACT(Name, VersionNumber)                                                                       \
	static_assert(std::string_view(#Name) == HYP_DETAIL_STRING(HYP_SHADER_DOMAIN));                                    \
	Result->Version = VersionNumber;
#define HYP_UNIFORM_BEGIN(Name, Instance)                                                                              \
	{                                                                                                                  \
		const std::string UniformName = #Name;                                                                         \
		FShaderParameterLayoutBuilder Layout(#Name, #Instance);
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#define HYP_UNIFORM_FIELD(Type, Name, Scope) Layout.AddField(#Name, HYP_FIELD_TYPE(Type)::GetType(), NextSemantic());
#define HYP_UNIFORM_ABI_OFFSET(Offset) Layout.SetNextOffset(Offset);
#define HYP_UNIFORM_ABI_SIZE(Size) Layout.SetMinimumSize(Size);
#define HYP_UNIFORM_END()                                                                                              \
	Result->Uniforms.push_back({UniformName, Layout.Build()});                                                         \
	}
#define HYP_SHADER_VALUE(Type, Name, Scope) (void)NextSemantic();
#define HYP_UNIFORM_ALIAS(Alias, Name) Result->UniformAliases.push_back({#Alias, #Name});
#define HYP_DETAIL_RESOURCE(Name, Kind, Comparison, Writable)                                                          \
	Result->Resources.push_back({#Name, {NextSemantic(), EMaterialValueKind::Kind, 0, Comparison, {}, Writable}});
#define HYP_TEXTURE_2D(Type, Name, Scope)                                                                              \
	ValidateShaderTextureType(HYP_FIELD_TYPE(Type)::GetType(), #Name);                                                 \
	HYP_DETAIL_RESOURCE(Name, Texture2D, false, false)
#define HYP_TEXTURE_CUBE(Type, Name, Scope)                                                                            \
	ValidateShaderTextureType(HYP_FIELD_TYPE(Type)::GetType(), #Name);                                                 \
	HYP_DETAIL_RESOURCE(Name, TextureCube, false, false)
#define HYP_RW_TEXTURE_2D(Type, Name, Scope)                                                                           \
	ValidateShaderTextureType(HYP_FIELD_TYPE(Type)::GetType(), #Name);                                                 \
	HYP_DETAIL_RESOURCE(Name, Texture2D, false, true)
#define HYP_SAMPLER(Name, Scope) HYP_DETAIL_RESOURCE(Name, Sampler, false, false)
#define HYP_COMPARISON_SAMPLER(Name, Scope) HYP_DETAIL_RESOURCE(Name, Sampler, true, false)
#define HYP_READ_BUFFER(Type, Name, Scope)                                                                             \
	HYP_DETAIL_RESOURCE(Name, ReadBuffer, false, false)                                                                \
	ResourceTypes.push_back({Result->Resources.size() - 1, #Type});
#define HYP_RESOURCE_SHADER_NAME(Name) Result->Resources.back().first = Name;
#define HYP_STRUCTURED_BEGIN(Name)                                                                                     \
	{                                                                                                                  \
		const std::string ElementName = #Name;                                                                         \
		FShaderParameterLayoutBuilder Layout(#Name, {}, EShaderPackingProfile::Structured);
#define HYP_STRUCTURED_FIELD(Type, Name) Layout.AddField(#Name, HYP_FIELD_TYPE(Type)::GetType());
#define HYP_STRUCTURED_END()                                                                                           \
	ElementLayouts.push_back({ElementName, Layout.Build()});                                                           \
	}

#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
#undef HYP_DETAIL_RESOURCE
		for (const auto& ResourceType : ResourceTypes)
		{
			auto& Resource = Result->Resources.at(ResourceType.first).second;
			const auto Found = std::find_if(ElementLayouts.begin(), ElementLayouts.end(),
			                                [&](const auto& InElement)
			                                {
				                                return InElement.first == ResourceType.second;
			                                });
			if (Found == ElementLayouts.end())
			{
				throw std::invalid_argument("Missing structured element declaration: " + ResourceType.second);
			}
			Resource.ElementLayout = Found->second;
			Resource.StructureByteStride = Found->second.Size;
		}
		return std::shared_ptr<const FShaderParameterContractSet>(std::move(Result));
	}();
	return Contracts;
}

#ifndef HYP_SHADER_COMMON
inline FMaterialSemanticId GetShaderSemantic(HYP_DOMAIN_SEMANTIC InSemantic)
{
	if (InSemantic == HYP_DOMAIN_SEMANTIC::None)
	{
		return {};
	}
	if (InSemantic >= HYP_DOMAIN_SEMANTIC::Count)
	{
		throw std::invalid_argument("Invalid owner semantic identity");
	}
	static const auto Values = []
	{
		std::vector<FMaterialSemanticId> Result;
		[[maybe_unused]] const auto& Semantics = HYP_DOMAIN_FUNCTION(ShaderSemantics)();
		[[maybe_unused]] std::size_t Index = 0;
#define HYP_UNIFORM_FIELD(Type, Name, Scope) ++Index;
#define HYP_SHADER_VALUE(Type, Name, Scope) Result.push_back(FMaterialSemanticId(&Semantics.at(Index++)));
#define HYP_TEXTURE_2D(Type, Name, Scope) HYP_SHADER_VALUE(Type, Name, Scope)
#define HYP_TEXTURE_CUBE(Type, Name, Scope) HYP_SHADER_VALUE(Type, Name, Scope)
#define HYP_RW_TEXTURE_2D(Type, Name, Scope) HYP_SHADER_VALUE(Type, Name, Scope)
#define HYP_SAMPLER(Name, Scope) HYP_SHADER_VALUE(float, Name, Scope)
#define HYP_COMPARISON_SAMPLER(Name, Scope) HYP_SHADER_VALUE(float, Name, Scope)
#define HYP_READ_BUFFER(Type, Name, Scope) HYP_SHADER_VALUE(Type, Name, Scope)
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
		return Result;
	}();
	return Values.at(static_cast<std::size_t>(InSemantic) - 1);
}
#endif

#define HYP_UNIFORM_BEGIN(Name, Instance)                                                                              \
	inline FMaterialSemanticId GetShaderSemantic(HYP_FIELD_ENUM(Name) InField)                                         \
	{                                                                                                                  \
		if (InField >= HYP_FIELD_ENUM(Name)::Count)                                                                    \
		{                                                                                                              \
			throw std::invalid_argument("Invalid shader field identity: " #Name);                                      \
		}                                                                                                              \
		return HYP_DOMAIN_FUNCTION(ShaderContracts)()                                                                  \
		    ->Uniforms.at(static_cast<std::size_t>(HYP_DOMAIN_UNIFORM::Name))                                          \
		    .second.Members.at(static_cast<std::size_t>(InField))                                                      \
		    .Semantic;                                                                                                 \
	}
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"

inline std::string_view GetShaderUniformName(HYP_DOMAIN_UNIFORM InUniform)
{
	if (InUniform >= HYP_DOMAIN_UNIFORM::Count)
	{
		throw std::invalid_argument("Invalid owner uniform identity");
	}
	return HYP_DOMAIN_FUNCTION(ShaderContracts)()->Uniforms.at(static_cast<std::size_t>(InUniform)).first;
}

inline FStandardMaterialBlock GetStandardMaterialBlock(HYP_DOMAIN_UNIFORM InUniform)
{
	(void)GetShaderUniformName(InUniform);
	return HYP_DOMAIN_FUNCTION(ShaderContracts)()->Uniforms.at(static_cast<std::size_t>(InUniform)).second;
}

inline std::vector<FMaterialParameterDeclaration> GetStandardMaterialBlockParameters(
    HYP_DOMAIN_UNIFORM InUniform, const FMaterialSemanticRegistry& InRegistry)
{
	const std::array Contracts{HYP_DOMAIN_FUNCTION(ShaderContracts)()};
	return GetStandardMaterialBlockParameters(GetShaderUniformName(InUniform), InRegistry, Contracts);
}

inline FMaterialInstanceArray MakeEngineInstanceArray(HYP_DOMAIN_UNIFORM InUniform)
{
#define HYP_INSTANCE_ARRAY(Name, Member)                                                                               \
	if (InUniform == HYP_DOMAIN_UNIFORM::Name)                                                                         \
	{                                                                                                                  \
		return {#Name, #Member};                                                                                       \
	}
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
	(void)InUniform;
	throw std::invalid_argument("Owner uniform has no instance array");
}

#define HYP_UNIFORM_BEGIN(Name, Instance)                                                                              \
	struct F##Name##Parameters                                                                                         \
	{
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#define HYP_UNIFORM_FIELD(Type, Name, Scope) HYP_FIELD_TYPE(Type)::FType Name{};
#define HYP_UNIFORM_END()                                                                                              \
	}                                                                                                                  \
	;
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"

#define HYP_UNIFORM_BEGIN(Name, Instance)                                                                              \
	inline void AppendShaderParameters(FMaterialParameterValues& OutValues, const F##Name##Parameters& InParameters)   \
	{                                                                                                                  \
		using FField = HYP_FIELD_ENUM(Name);
#define HYP_UNIFORM_FLAT_BEGIN(Name) HYP_UNIFORM_BEGIN(Name, )
#define HYP_UNIFORM_FIELD(Type, Name, Scope) OutValues.push_back({FField::Name, MakeShaderValue(InParameters.Name)});
#define HYP_UNIFORM_END() }
#include "Hyperion/Materials/ShaderDeclarationDefaults.inl"
#include HYP_SHADER_DECLARATIONS
#include "Hyperion/Materials/ShaderDeclarationCleanup.inl"
} // namespace Hyperion

#undef HYP_FIELD_TYPE
#undef HYP_FIELD_ENUM
#undef HYP_DOMAIN_FUNCTION
#undef HYP_DOMAIN_UNIFORM
#undef HYP_DOMAIN_SEMANTIC
#undef HYP_DETAIL_STRING
#undef HYP_DETAIL_STRING_INNER
#undef HYP_DETAIL_JOIN
#undef HYP_DETAIL_JOIN_INNER
