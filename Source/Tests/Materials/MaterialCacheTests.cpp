#include "Hyperion/Renderer/MaterialConstantCache.h"
#include "Hyperion/Renderer/MaterialPipeline.h"
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

void CheckEvictedPixels(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, const FCompiledMaterialDefinition& InCompiled,
                        const std::vector<FConstantBinding>& InBindings)
{
	const auto& Program = InCompiled.GetPass();
	const auto Layout = InDevice.CreateBindingLayout(DescribeMaterialLayout(Program));
	FDrawPacket Draw;
	Draw.Pipeline = InDevice.CreatePipeline(DescribeMaterialPipeline(
	    Program, InCompiled.Interface.Definition->GetPass(), Layout, {{"POSITION", 0, EVertexFormat::Float3, 0}},
	    sizeof(FVec3), ERHIPrimitiveTopology::TriangleList, {}));
	Draw.Bindings = InDevice.CreateBindingSet({Layout, {}});
	Draw.ConstantBindings = InBindings;
	const std::array<FVec3, 3> Vertices{{{-2, -2, 0}, {6, -2, 0}, {-2, 6, 0}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Vertices)));
	Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Draw.VertexStride = sizeof(FVec3);
	Draw.IndexCount = 3;
	Draw.Scissor = {0, 0, 64, 64};
	InSwapchain.BeginFrame({64, 64});
	FPassCommands Pass;
	Pass.Color = FColorAttachment{FRenderTarget::Backbuffer()};
	Pass.Name = "Evicted constant snapshot";
	Pass.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::Present, EResourceState::RenderTarget}};
	Pass.Color->Actions.Load = EAttachmentLoad::Clear;
	Pass.Draws = {Draw};
	FPassCommands Present;
	Present.Transitions = {{FRenderTarget::Backbuffer(), EResourceState::RenderTarget, EResourceState::Present}};
	const std::array Lists{InSwapchain.Record(0, Pass), InSwapchain.Record(1, Present)};
	const auto Image = InSwapchain.EndFrame(Lists, false, true);
	HYP_CHECK(std::abs(Image.Rgba[(32 * 64 + 32) * 4] - .2f) < .01f);
}

void CheckConstantRecency(IRHIDevice& InDevice, const FCompiledMaterialDefinition& InCompiled)
{
	FMaterialInstance Instance(InCompiled.Interface);
	FMaterialConstantCache Cache(InDevice, 65536, {8, 65536, 0});
	auto Inputs = Context();
	const auto View = static_cast<std::size_t>(EMaterialScope::View);
	const auto Bind = [&](std::uint64_t InRevision)
	{
		Inputs.Scopes[View].Key.Revision = InRevision;
		return Cache.Bind(InCompiled.GetPass(), *InCompiled.Interface.Schema,
		                  ResolveMaterialBindingContext(Instance.Freeze(), InCompiled, InCompiled.GetPass(), Inputs));
	};
	const auto Frozen = Bind(1);
	Bind(2);
	Bind(3);
	HYP_CHECK(Cache.Statistics().CachedBlocks == 8);
	Bind(1); // Keep the oldest insertion hot; the two revision-2 blocks become the next victims.
	Bind(4);
	const auto Before = Cache.Statistics().Packs;
	const auto Again = Bind(1);
	Bind(3);
	HYP_CHECK(Cache.Statistics().Packs == Before);
	for (std::size_t Index = 0; Index < Frozen.size(); ++Index)
	{
		HYP_CHECK(Same(Frozen[Index], Again[Index]));
	}
	Bind(2);
	HYP_CHECK(Cache.Statistics().Packs == Before + 2);
	Cache.Clear();
	Bind(5);
	HYP_CHECK(Cache.Statistics().CachedBlocks == 4);
}

void CheckConstantBudget(IRHIDevice& InDevice, IRHISwapchain& InSwapchain,
                         const FCompiledMaterialDefinition& InCompiled, FMaterialConstantLimits InLimits)
{
	FMaterialInstance Instance(InCompiled.Interface);
	Instance.Set("Tint", FMaterialValue::Float(FVec4{.2f, .3f, .4f, 1}));
	FMaterialConstantCache Cache(InDevice, 512, InLimits);
	auto Inputs = Context();
	const auto Bind = [&]
	{
		return Cache.Bind(InCompiled.GetPass(), *InCompiled.Interface.Schema,
		                  ResolveMaterialBindingContext(Instance.Freeze(), InCompiled, InCompiled.GetPass(), Inputs));
	};
	auto Frozen = Bind();
	const auto Publication = Frozen[0].Slice.Publication;
	for (std::uint64_t Index = 0; Index < 512; ++Index)
	{
		// Keep all scope owners alive while both a stable key's value and another key's revision change.
		Inputs.Scopes[static_cast<std::size_t>(EMaterialScope::View)].Key.Revision = Index + 2;
		Inputs.Providers[0].Value = FMaterialValue::Float(FVec3{float(Index) * .001f, 0, 0});
		Inputs.Providers[1].Value = FMaterialValue::Matrix(Translation({float(Index) * .001f, 0, 0}));
		Inputs.Providers[2].Value = Inputs.Providers[1].Value;
		Bind();
		Cache.Collect();
		const auto Stats = Cache.Statistics();
		HYP_CHECK(Stats.CachedBlocks <= InLimits.MaxBlocks && Stats.CachedBytes <= InLimits.MaxBytes);
		HYP_CHECK(Stats.LivePages <= 12);
	}
	HYP_CHECK(Cache.Statistics().Evictions > 0 && Frozen[0].Slice.Publication == Publication);
	Cache.Clear();
	HYP_CHECK(!Cache.CanRelease());
	CheckEvictedPixels(InDevice, InSwapchain, InCompiled, Frozen);
	Frozen.clear();
	InSwapchain.WaitIdle();
	Cache.Collect();
	HYP_CHECK(Cache.CanRelease() && Cache.Statistics().LivePages == 1);
	FMaterialConstantCache Uncached(InDevice, 512, {0, 0, 0});
	for (std::uint64_t Index = 0; Index < 32; ++Index)
	{
		Inputs.Scopes[static_cast<std::size_t>(EMaterialScope::View)].Key.Revision = Index + 1000;
		Uncached.Bind(InCompiled.GetPass(), *InCompiled.Interface.Schema,
		              ResolveMaterialBindingContext(Instance.Freeze(), InCompiled, InCompiled.GetPass(), Inputs));
		Uncached.Collect();
	}
	HYP_CHECK(Uncached.Statistics().CachedBlocks == 0 && Uncached.Statistics().LivePages == 1);
}
} // namespace

void RunMaterialCacheTests(IRHIDevice& InDevice, IRHISwapchain& InSwapchain)
{
	const auto Compiled = Compile();
	CheckConstantRecency(InDevice, Compiled);
	CheckConstantBudget(InDevice, InSwapchain, Compiled, {7, 16 * 1024 * 1024, 2});
	CheckConstantBudget(InDevice, InSwapchain, Compiled, {4096, 7 * 256, 2});
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
