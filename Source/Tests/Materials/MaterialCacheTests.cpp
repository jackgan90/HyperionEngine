#include "Hyperion/Renderer/MaterialConstantCache.h"
#include "Support/TestSupport.h"
#include <fstream>

using namespace Hyperion;

namespace
{
FMaterialScopeInput Scope(std::uint64_t InIdentity, std::uint64_t InRevision = 1)
{
	FMaterialScopeKey Key{InIdentity, InRevision};
	return {Key, std::make_shared<const FMaterialScopeKey>(Key)};
}

FCompiledMaterialDefinition Compile()
{
	const auto Root = std::filesystem::absolute("material-cache-test/source");
	std::filesystem::create_directories(Root);
	std::ofstream(Root / "Cache.hlsl") << R"(
cbuffer ViewData : register(b0) { float3 Eye; };
cbuffer ObjectData : register(b1) { float4x4 World; };
cbuffer MaterialData : register(b2) { float4 Tint; };
cbuffer MixedData : register(b3) { float3 OtherEye; float Gain; float4x4 Combined; };
float4 VSMain(float3 InPosition : POSITION) : SV_Position
{
    return mul(Combined, mul(World, float4(InPosition + Eye + OtherEye, 1))) * Gain;
}
float4 PSMain() : SV_Target0 { return Tint; }
)";
	FShaderCompiler Compiler(Root, "material-cache-test/cache");
	FMaterialDescription Description;
	Description.Name = "Scope cache";
	FMaterialPass Pass;
	Pass.Vertex = {"Cache.hlsl", "VSMain"};
	Pass.Pixel = {"Cache.hlsl", "PSMain"};
	Description.Passes.push_back(Pass);
	const auto Registry = GetStandardMaterialSemantics();
	auto Eye = DeclareMaterialSemantic("Eye", "Engine.View.CameraPosition", *Registry);
	Eye.Targets = {"ViewData.Eye", "MixedData.OtherEye"};
	Description.Parameters.push_back(Eye);
	Description.Parameters.push_back(DeclareMaterialSemantic("World", "Engine.Object.World", *Registry));
	Description.Parameters.push_back(
	    DeclareMaterialSemantic("Combined", "Engine.Object.WorldViewProjection", *Registry));
	FMaterialParameterDeclaration Gain;
	Gain.Name = "Gain";
	Gain.Default = FMaterialValue::Float(1);
	Description.Parameters.push_back(Gain);
	FMaterialParameterDeclaration Tint;
	Tint.Name = "Tint";
	Tint.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float, 4);
	Tint.Default = FMaterialValue::Float(FVec4{1, 1, 1, 1});
	Description.Parameters.push_back(Tint);
	return CompileMaterialDefinition(Compiler, std::make_shared<const FMaterialDefinition>(Description),
	                                 EShaderFormat::Dxil);
}

FMaterialBindingContext Context()
{
	FMaterialBindingContext Result;
	Result.Scopes[static_cast<std::size_t>(EMaterialScope::View)] = Scope(10);
	Result.Scopes[static_cast<std::size_t>(EMaterialScope::Object)] = Scope(20);
	Result.Scopes[static_cast<std::size_t>(EMaterialScope::Draw)] = Scope(30);
	Result.Providers = {
	    {"Engine.View.CameraPosition", FMaterialValue::Float(FVec3{.1F, .2F, .3F}),
	     MaterialScopeBit(EMaterialScope::View)},
	    {"Engine.Object.World", FMaterialValue::Matrix(Identity()), MaterialScopeBit(EMaterialScope::Object)},
	    {"Engine.Object.WorldViewProjection", FMaterialValue::Matrix(Identity()),
	     MaterialScopeBit(EMaterialScope::Object) | MaterialScopeBit(EMaterialScope::View)}};
	return Result;
}

bool Same(const FConstantBinding& InA, const FConstantBinding& InB)
{
	return InA.Slice.Buffer.Payload == InB.Slice.Buffer.Payload && InA.Slice.Offset == InB.Slice.Offset &&
	       InA.Slice.Publication == InB.Slice.Publication;
}
} // namespace

void RunMaterialCacheTests(IRHIDevice& InDevice)
{
	const auto Compiled = Compile();
	FMaterialInstance Instance(Compiled.Interface);
	FMaterialConstantCache Cache(InDevice, 512);
	auto Inputs = Context();
	const auto Resolve = [&](const FMaterialBindingContext& InInputs)
	{
		return ResolveMaterialBindingContext(Instance.Freeze(), Compiled, Compiled.GetPass(), InInputs);
	};
	const auto Bind = [&](const FMaterialBindingContext& InInputs)
	{
		return Cache.Bind(Compiled.GetPass(), *Compiled.Interface.Schema, Resolve(InInputs));
	};
	auto First = Bind(Inputs);
	auto Again = Bind(Inputs);
	HYP_CHECK(First.size() == 4 && Cache.Statistics().Packs == 4 && Cache.Statistics().Reuses == 4);
	for (std::size_t Index = 0; Index < First.size(); ++Index)
	{
		HYP_CHECK(Same(First[Index], Again[Index]));
	}
	auto SecondView = Inputs;
	SecondView.Scopes[static_cast<std::size_t>(EMaterialScope::View)] = Scope(11);
	SecondView.Providers[0].Value = FMaterialValue::Float(FVec3{.5F, .6F, .7F});
	auto Other = Bind(SecondView);
	for (std::size_t Index = 0; Index < First.size(); ++Index)
	{
		const auto& Name = Compiled.GetPass().Bindings[First[Index].Slot].Resource.Name;
		HYP_CHECK(Same(First[Index], Other[Index]) == (Name == "ObjectData" || Name == "MaterialData"));
	}
	HYP_CHECK(Cache.Statistics().Packs == 6);
	// The same caller identity/revision can emit distinct effective per-item values. Equality must still separate them.
	FMat4 World = Identity();
	World.Values[12] = .25F;
	Inputs.Providers[1].Value = FMaterialValue::Matrix(World);
	Inputs.Providers[2].Value = FMaterialValue::Matrix(World);
	auto Moved = Bind(Inputs);
	HYP_CHECK(Cache.Statistics().Packs == 8);
	auto DifferentLayout = Compiled.GetPass();
	for (auto& Binding : DifferentLayout.Bindings)
	{
		if (Binding.Resource.Name == "ViewData")
		{
			Binding.Resource.ByteSize = 32;
			Binding.Members.front().Layout.Offset = 16;
		}
	}
	auto Different = Cache.Bind(DifferentLayout, *Compiled.Interface.Schema, Resolve(Inputs));
	HYP_CHECK(Cache.Statistics().Packs == 9);
	Instance.Set("Gain", FMaterialValue::Float(2));
	auto Edited = Bind(Inputs);
	HYP_CHECK(Cache.Statistics().Packs == 11);
	Cache.Clear();
	HYP_CHECK(!Cache.CanRelease());
	First.clear();
	Again.clear();
	Other.clear();
	Moved.clear();
	Different.clear();
	Edited.clear();
	Cache.Collect();
	HYP_CHECK(Cache.CanRelease() && Cache.Statistics().LivePages == 1 && Cache.Statistics().PagesReset > 0);
	HYP_CHECK(InDevice.Statistics().ValidationErrors == 0);
}
