#include "Hyperion/Materials/PbrParameters.h"
#include "Hyperion/Materials/ShaderParameters.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include "Hyperion/Renderer/ShaderParameters/HierarchicalDepthParameters.h"
#include "Materials/UniformDeclarationFixture.h"
#include "Support/RendererShaderSupport.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <limits>
#include <type_traits>

using namespace Hyperion;

namespace
{
template<typename T> void Reject(T InOperation)
{
	bool bRejected{};
	try
	{
		InOperation();
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckPredeclaredLocalIdentity()
{
	FMaterialDescription Description;
	Description.Name = "Predeclared local uniform";
	FMaterialPass Pass;
	Pass.Vertex = {"Unused.hlsl", "VSMain"};
	Pass.Pixel = {"Unused.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	Description.ShaderContracts = {GetContactShadowShaderContracts()};
	auto Parameter = DeclareMaterialSemantic("RayLength", EContactV1Field::RayLength, *GetStandardMaterialSemantics());
	Parameter.Source = EMaterialParameterSource::Manual;
	Parameter.OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	Description.Parameters.push_back(Parameter);
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	HYP_CHECK(Definition->GetDescription().Parameters.front().Semantic == EContactV1Field::RayLength);
	const auto Handle = Definition->GetSchema()->FindSemantic(EContactV1Field::RayLength);
	FMaterialInstance Instance(Definition);
	Instance.SetSemantic(EContactV1Field::RayLength, FMaterialValue::Float(3));
	HYP_CHECK(Instance.Freeze()->Overrides.front().Handle == Handle);
	Instance.SetParameters({{EContactV1Field::RayLength, FMaterialValue::Float(4)}});
	HYP_CHECK(Instance.Freeze()->Overrides.front().Value == FMaterialValue::Float(4));
}

void CheckContractBaseline()
{
	const auto Contracts = TestShaderContracts();
	const auto Registry = GetStandardMaterialSemantics();
#define HYP_EXPECT_UNIFORM(InName, InSize, InInstance)                                                                 \
	{                                                                                                                  \
		const auto Block = GetStandardMaterialBlock(#InName, Contracts);                                               \
		HYP_CHECK(Block.Size == InSize && Block.Instance == InInstance);                                               \
		std::size_t Index = 0;
#define HYP_EXPECT_FIELD(InName, InOffset, InWire, InScope, InScalar, InColumns, InRows, InGroup, bScene, bDefault,    \
                         Hint)                                                                                         \
	{                                                                                                                  \
		const auto& Member = Block.Members.at(Index++);                                                                \
		const auto& Semantic = Registry->Find(Member.Semantic);                                                        \
		HYP_CHECK(Member.Name == InName && Member.Offset == InOffset && Member.Semantic.GetName() == InWire);          \
		HYP_CHECK(Member.Columns == InColumns && Member.Rows == InRows && Member.Scalar == EMaterialScalar::InScalar); \
		HYP_CHECK(Semantic.Scope == EMaterialScope::InScope);                                                          \
		const FEngineSemanticPolicy Policy{EEngineSemanticGroup::InGroup, bScene, bDefault, EMaterialEditHint::Hint};  \
		HYP_CHECK(GetEngineSemanticPolicy(Member.Semantic) == Policy);                                                 \
	}
#define HYP_EXPECT_END()                                                                                               \
	HYP_CHECK(Index == Block.Members.size());                                                                          \
	}
#define HYP_EXPECT_RESOURCE(InName, InWire, InScope, InKind, InStride, bInComparison, bInWritable, InGroup, bScene,    \
                            bDefault, Hint)                                                                            \
	{                                                                                                                  \
		const auto Resource = GetEngineMaterialResource(InName, Contracts);                                            \
		HYP_CHECK(Resource.Semantic.GetName() == InWire &&                                                             \
		          Registry->Find(Resource.Semantic).Scope == EMaterialScope::InScope);                                 \
		HYP_CHECK(Resource.Kind == EMaterialValueKind::InKind && Resource.StructureByteStride == InStride &&           \
		          Resource.bComparison == bInComparison && Resource.bWritable == bInWritable);                         \
		const FEngineSemanticPolicy Policy{EEngineSemanticGroup::InGroup, bScene, bDefault, EMaterialEditHint::Hint};  \
		HYP_CHECK(GetEngineSemanticPolicy(Resource.Semantic) == Policy);                                               \
	}
#define HYP_EXPECT_UNIFORM_ALIAS(InAlias, InName)                                                                      \
	HYP_CHECK(GetStandardMaterialBlock(InAlias, Contracts) == GetStandardMaterialBlock(InName, Contracts));
#define HYP_EXPECT_SEMANTIC_ALIAS(InName, InAlias)                                                                     \
	HYP_CHECK(GetEngineMaterialResource(InName, Contracts).Semantic == FindStandardShaderSemantic(InAlias));
#include "Materials/ShaderContractBaseline.inl"
#undef HYP_EXPECT_UNIFORM
#undef HYP_EXPECT_FIELD
#undef HYP_EXPECT_END
#undef HYP_EXPECT_RESOURCE
#undef HYP_EXPECT_UNIFORM_ALIAS
#undef HYP_EXPECT_SEMANTIC_ALIAS
}

void CheckLayoutRules()
{
	Reject(
	    []
	    {
		    FShaderParameterLayoutBuilder("Invalid", {}, static_cast<EShaderPackingProfile>(-1));
	    });
	Reject(
	    []
	    {
		    FShaderParameterLayoutBuilder("Empty").AddField("",
		                                                    FMaterialParameterType::Numeric(EMaterialScalar::Float));
	    });
	Reject(
	    []
	    {
		    ValidateShaderTextureType(FMaterialParameterType::Numeric(EMaterialScalar::Uint), "IntegerTexture");
	    });
	const auto View = GetStandardMaterialBlock(EUniformDeclarationFixtureUniform::ScopeViewFixture);
	const auto Pass = GetStandardMaterialBlock(EUniformDeclarationFixtureUniform::ScopePassFixture);
	HYP_CHECK(View.Size == Pass.Size && View.Members.size() == Pass.Members.size());
	for (std::size_t Index = 0; Index < View.Members.size(); ++Index)
	{
		HYP_CHECK(View.Members[Index].Offset == Pass.Members[Index].Offset);
		HYP_CHECK(GetStandardMaterialSemantics()->Find(View.Members[Index].Semantic).Scope == EMaterialScope::View);
		HYP_CHECK(GetStandardMaterialSemantics()->Find(Pass.Members[Index].Semantic).Scope == EMaterialScope::Pass);
	}
	FShaderParameterLayoutBuilder Layout("Packing");
	Layout.AddField("A", FMaterialParameterType::Numeric(EMaterialScalar::Float, 3));
	Layout.AddField("bEnabled", FMaterialParameterType::Numeric(EMaterialScalar::Bool));
	Layout.AddField("Next", FMaterialParameterType::Numeric(EMaterialScalar::Float, 2));
	Layout.AddField("Matrix", FMaterialParameterType::Numeric(EMaterialScalar::Float, 4, 4));
	const auto Block = Layout.Build();
	HYP_CHECK(Block.Size == 96 && Block.Members[1].Offset == 12 && Block.Members[2].Offset == 16 &&
	          Block.Members[3].Offset == 32);
	FShaderParameterLayoutBuilder Structured("Packing", {}, EShaderPackingProfile::Structured);
	Structured.AddField("A", FMaterialParameterType::Numeric(EMaterialScalar::Float, 3));
	Structured.AddField("B", FMaterialParameterType::Numeric(EMaterialScalar::Float, 2));
	HYP_CHECK(Structured.Build().Size == 20 && Structured.Build().Members[1].Offset == 12);
	FShaderParameterLayoutBuilder Fixed("Historical");
	Fixed.AddField("A", FMaterialParameterType::Numeric(EMaterialScalar::Float));
	Fixed.SetNextOffset(32);
	Fixed.AddField("B", FMaterialParameterType::Numeric(EMaterialScalar::Bool));
	Fixed.SetMinimumSize(64);
	HYP_CHECK(Fixed.Build().Members[1].Offset == 32 && Fixed.Build().Size == 64);
	Reject(
	    [&]
	    {
		    Fixed.SetNextOffset(16);
	    });
	Reject(
	    [&]
	    {
		    Layout.AddField("A", FMaterialParameterType::Numeric(EMaterialScalar::Float));
	    });
	Reject(
	    [&]
	    {
		    Layout.AddField("Unsupported", FMaterialParameterType::Numeric(EMaterialScalar::Float, 3, 3));
	    });
	Reject(
	    [&]
	    {
		    Layout.AddField("Array",
		                    FMaterialParameterType::Array(FMaterialParameterType::Numeric(EMaterialScalar::Float), 2));
	    });
	Reject(
	    [&]
	    {
		    Structured.AddField("Matrix", FMaterialParameterType::Numeric(EMaterialScalar::Float, 4, 4));
	    });
	FShaderParameterLayoutBuilder Overflow("Overflow");
	Overflow.SetNextOffset(std::numeric_limits<std::uint32_t>::max() - 3);
	Reject(
	    [&]
	    {
		    Overflow.AddField("Value", FMaterialParameterType::Numeric(EMaterialScalar::Float));
	    });
	FShaderParameterLayoutBuilder Misaligned("Misaligned");
	Misaligned.SetNextOffset(4);
	Reject(
	    [&]
	    {
		    Misaligned.AddField("Vector", FMaterialParameterType::Numeric(EMaterialScalar::Float, 4));
	    });
	Reject(
	    [&]
	    {
		    Misaligned.Build();
	    });
}

void CheckTypedPublication()
{
	Reject(
	    []
	    {
		    GetShaderSemantic(EContactV1Field::Count);
	    });
	Reject(
	    []
	    {
		    GetShaderSemantic(EContactShadowSemantic::Count);
	    });
	Reject(
	    []
	    {
		    GetStandardMaterialBlock(EContactShadowUniform::Count);
	    });
	const FMaterialSemanticId DeferredViewport(EDeferredLightV1Field::Viewport);
	const FMaterialSemanticId ContactViewport(EContactV1Field::Viewport);
	HYP_CHECK(DeferredViewport.IsBuiltin() && ContactViewport.IsBuiltin() && DeferredViewport != ContactViewport);
	HYP_CHECK(GetStandardMaterialSemantics()->Find(DeferredViewport).Type ==
	          GetStandardMaterialSemantics()->Find(ContactViewport).Type);
	HYP_CHECK(GetStandardMaterialBlock("ContactV1").Size == 0);
	const auto Conflicting = std::make_shared<FShaderParameterContractSet>(*GetContactShadowShaderContracts());
	const FShaderParameterContracts Selected{GetContactShadowShaderContracts(), Conflicting};
	Conflicting->Resources.front().second.bComparison = true;
	Reject(
	    [&]
	    {
		    GenerateShaderIncludes(Selected);
	    });
	Conflicting->IncludeName = "ConflictingParameters.generated.hlsli";
	Reject(
	    [&]
	    {
		    GenerateShaderIncludes(Selected);
	    });
	static_assert(std::is_same_v<decltype(FHZBCopyV1Parameters::Width), std::uint32_t>);
	static_assert(std::is_same_v<decltype(FHZBReduceV1Parameters::bMaximum), bool>);
	const auto Values = MakeShaderParameters(FHZBCopyV1Parameters{17, 9, {0, 1}, {0, 0, 17, 9}, 1});
	HYP_CHECK(Values.size() == 5 && Values[0].Semantic == EHZBCopyV1Field::Width);
	for (const auto& Value : Values)
	{
		HYP_CHECK(Value.Name.empty() && !Value.Semantic.IsEmpty() && !Value.Handle.SchemaIdentity);
	}
	FMaterialDescription Description;
	Description.Name = "Typed authored names";
	FMaterialPass Pass;
	Pass.Vertex = {"Unused.hlsl", "VSMain"};
	Pass.Pixel = {"Unused.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	Description.Parameters = {
	    DeclareMaterialSemantic("RenamedTint", EHyperionMaterialV1Field::BaseColor, *GetStandardMaterialSemantics()),
	    DeclareMaterialSemantic("RenamedRoughness", EHyperionMaterialV1Field::Roughness,
	                            *GetStandardMaterialSemantics())};
	const auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	FMaterialInstance Instance(Definition);
	Instance.SetParameters({{EHyperionMaterialV1Field::BaseColor, FMaterialValue::Float(FVec4{1, 0, 0, 1})}});
	const auto Retained = Instance.Freeze();
	const auto Handle = Retained->Overrides[0].Handle;
	HYP_CHECK(Handle.SchemaIdentity && Retained->Overrides[0].Name.empty());
	Reject(
	    [&]
	    {
		    Instance.SetParameters({{EHyperionMaterialV1Field::BaseColor, FMaterialValue::Float(FVec4{})},
		                            {EHyperionMaterialV1Field::Roughness, FMaterialValue::Uint(7)}});
	    });
	HYP_CHECK(Instance.Freeze() == Retained);
	Instance.Clear(Handle);
	HYP_CHECK(Instance.Freeze()->Overrides.empty() && Retained->Overrides.size() == 1);
	const auto NextSchema = std::make_shared<const FMaterialParameterSchema>(Description.Parameters);
	const auto Rebound = RebindMaterialParameters(Retained->Overrides, *Retained->Schema, *NextSchema);
	HYP_CHECK(Rebound[0].Handle.SchemaIdentity == NextSchema->GetIdentity());
	HYP_CHECK(Rebound[0].Value == Retained->Overrides[0].Value);
	auto Snapshot = *Retained;
	RebindMaterialSnapshot(Snapshot, NextSchema);
	HYP_CHECK(Snapshot.Schema == NextSchema && Snapshot.Overrides == Rebound);
	HYP_CHECK(Retained->Schema != NextSchema);
	auto Named = std::make_shared<FMaterialSnapshot>(*Retained);
	Named->Overrides = {{"RenamedTint", Retained->Overrides[0].Value}};
	FMaterialInstance Clone(Named);
	Clone.SetSemantic(EHyperionMaterialV1Field::BaseColor, FMaterialValue::Float(FVec4{}));
	HYP_CHECK(Clone.Freeze()->Overrides.size() == 1 && Clone.Freeze()->Overrides[0].Name.empty());
	Reject(
	    [&]
	    {
		    NextSchema->Get(Handle);
	    });
	HYP_CHECK(NextSchema->GetParameterIdentity(0) == Retained->Schema->GetParameterIdentity(0));
	Description.Parameters[0].Name = "DifferentIdentity";
	const FMaterialParameterSchema Different(Description.Parameters);
	HYP_CHECK(Different.GetParameterIdentity(0) != NextSchema->GetParameterIdentity(0));
	HYP_CHECK(GetEngineSemanticPolicy(EHyperionMaterialV1Field::BaseColor).EditHint == EMaterialEditHint::Color);
	HYP_CHECK(GetEngineSemanticPolicy(EHyperionMaterialV1Field::NormalUv).EditHint == EMaterialEditHint::UvSet);
}

void CheckIndependentContracts(FShaderCompiler& InCompiler, const std::filesystem::path& InRoot, EShaderFormat InFormat)
{
	const FShaderParameterContracts Contracts{GetDeferredLightingShaderContracts(), GetContactShadowShaderContracts()};
	std::ofstream(InRoot / "Independent.hlsl")
	    << "#include \"DeferredLightingParameters.generated.hlsli\"\n"
	    << "#include \"ContactShadowParameters.generated.hlsli\"\n"
	    << R"(cbuffer DeferredLightV1 : register(b0) { FDeferredLightV1Uniform DeferredLighting; };
cbuffer ContactV1 : register(b1) { FContactV1Uniform ContactShadow; };
float4 VSMain(float3 InPosition : POSITION) : SV_Position { return float4(InPosition, 1); }
float4 PSMain() : SV_Target0 { return float4(DeferredLighting.LightDirection + ContactShadow.LightDirection, 1); }
)";
	FShaderCompileOptions Options;
	for (const auto& Include : GenerateShaderIncludes(Contracts))
	{
		Options.VirtualIncludes.push_back({Include.first, Include.second});
	}
	const auto Artifact = InCompiler.Compile("Independent.hlsl", "PSMain", EShaderStage::Pixel, InFormat, Options);
	HYP_CHECK(Artifact.Bindings.size() == 2);
	for (auto Binding : Artifact.Bindings)
	{
		HYP_CHECK(NormalizeStandardMaterialBlock(Binding, Contracts));
		const auto Parameters =
		    GetStandardMaterialBlockParameters(Binding.Name, *GetStandardMaterialSemantics(), Contracts);
		HYP_CHECK(!Parameters.empty() && Parameters.front().Semantic.GetDescriptor());
	}
	FMaterialDescription Description;
	Description.Name = "Independent local contracts";
	Description.ShaderContracts = Contracts;
	FMaterialPass Pass;
	Pass.Vertex = {"Independent.hlsl", "VSMain"};
	Pass.Pixel = {"Independent.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	const auto Definition = std::make_shared<const FMaterialDefinition>(std::move(Description));
	const auto Program =
	    CompileMaterialDefinition(InCompiler, Definition, InFormat, {}, EMaterialEngineBindingMode::Explicit);
	const auto& Schema = *Program.Interface.Schema;
	const auto Deferred = Schema.FindSemantics(EDeferredLightV1Field::LightDirection);
	const auto Contact = Schema.FindSemantics(EContactV1Field::LightDirection);
	HYP_CHECK(Deferred.size() == 1 && Contact.size() == 1 && Deferred.front() != Contact.front());
	FMaterialInstance Instance(Program.Interface);
	Instance.SetParameters({{Deferred.front(), FMaterialValue::Float(FVec3{1, 0, 0})},
	                        {Contact.front(), FMaterialValue::Float(FVec3{0, 1, 0})}});
	const auto Retained = Instance.Freeze();
	Instance.SetParameters({{Deferred.front(), FMaterialValue::Float(FVec3{0, 0, 1})}});
	HYP_CHECK(Retained->Overrides.size() == 2 && Retained->Overrides != Instance.Freeze()->Overrides);
	std::ofstream(InRoot / "Absent.hlsl") << "float4 PSMain() : SV_Target0 { return 1; }";
	const auto Absent = InCompiler.Compile("Absent.hlsl", "PSMain", EShaderStage::Pixel, InFormat, Options);
	HYP_CHECK(Absent.Bindings.empty());
}

void CheckGeneratedUniform(FShaderCompiler& InCompiler, const std::filesystem::path& InRoot, const std::string& InName,
                           const FStandardMaterialBlock& InContract, const FShaderParameterContracts& InContracts,
                           EShaderFormat InFormat)
{
	const std::string& Name = InName;
	const FStandardMaterialBlock& Contract = InContract;
	const auto& Member = Contract.Members.front();
	const auto File = Name + ".hlsl";
	const auto IncludeName =
	    InContracts.empty() ? "HyperionUniforms.generated.hlsli" : InContracts.front()->IncludeName;
	std::ofstream Source(InRoot / File);
	Source << "#include \"" << IncludeName << "\"\n";
	if (Contract.Instance.empty())
	{
		Source << "HYP_UNIFORM_" << Name << "(b3);\n";
	}
	else
	{
		Source << "cbuffer " << Name << " : register(b3) { F" << Name << "Uniform " << Contract.Instance << "; };\n";
	}
	Source << "float4 PSMain() : SV_Target0 { return " << Member.Name
	       << (Member.Rows > 1      ? "[0][0]"
	           : Member.Columns > 1 ? ".x"
	                                : "")
	       << "; }\n";
	Source.close();
	FShaderCompileOptions Options;
	for (const auto& Include : GenerateShaderIncludes(InContracts))
	{
		Options.VirtualIncludes.push_back({Include.first, Include.second});
	}
	const auto First = InCompiler.Compile(File, "PSMain", EShaderStage::Pixel, InFormat, Options);
	const auto Warm = InCompiler.Compile(File, "PSMain", EShaderStage::Pixel, InFormat, Options);
	HYP_CHECK(Warm.bCacheHit && Warm.Bindings == First.Bindings);
	HYP_CHECK(First.Bindings.size() == 1);
	auto Binding = First.Bindings[0];
	HYP_CHECK(NormalizeStandardMaterialBlock(Binding, InContracts));
	HYP_CHECK(Binding.Members.size() == Contract.Members.size());
	const auto Generated = std::find_if(Options.VirtualIncludes.begin(), Options.VirtualIncludes.end(),
	                                    [&](const auto& InInclude)
	                                    {
		                                    return InInclude.Name == IncludeName;
	                                    });
	Generated->Source += "\n// different immutable generated input\n";
	const auto Changed = InCompiler.Compile(File, "PSMain", EShaderStage::Pixel, InFormat, Options);
	HYP_CHECK(Changed.CacheKey != First.CacheKey && Changed.Bindings == First.Bindings);
	Binding = First.Bindings[0];
	if (Contract.Instance.empty())
	{
		Binding.Members[0].Offset += 4;
	}
	else
	{
		Binding.Members[0].Members[0].Offset += 4;
	}
	Reject(
	    [&]
	    {
		    NormalizeStandardMaterialBlock(Binding, InContracts);
	    });
}
} // namespace

void RunShaderParameterTests()
{
	CheckContractBaseline();
	CheckLayoutRules();
	CheckTypedPublication();
	CheckPredeclaredLocalIdentity();
	const auto Root = std::filesystem::absolute("typed-shader-test/source");
	std::filesystem::create_directories(Root);
	FShaderCompiler Compiler(Root, "typed-shader-test/cache");
	for (const auto Format : {EShaderFormat::Dxil, EShaderFormat::Spirv, EShaderFormat::Msl})
	{
		for (const auto& Uniform : GetUniformDeclarationFixtureShaderContracts()->Uniforms)
		{
			CheckGeneratedUniform(Compiler, Root, Uniform.first, Uniform.second,
			                      {GetUniformDeclarationFixtureShaderContracts()}, Format);
		}
		for (const auto& Uniform : GetEngineShaderContracts()->Uniforms)
		{
			CheckGeneratedUniform(Compiler, Root, Uniform.first, Uniform.second, {}, Format);
		}
		for (const auto Sets :
		     {std::span<const std::shared_ptr<const FShaderParameterContractSet>>(GetStandardShaderContracts()),
		      std::span<const std::shared_ptr<const FShaderParameterContractSet>>(TestShaderContracts())})
		{
			for (const auto& Set : Sets)
			{
				for (const auto& Uniform : Set->Uniforms)
				{
					CheckGeneratedUniform(Compiler, Root, Uniform.first, Uniform.second, {Set}, Format);
				}
			}
		}
		CheckIndependentContracts(Compiler, Root, Format);
	}
}
