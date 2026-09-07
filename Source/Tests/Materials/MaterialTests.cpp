#include "Hyperion/Materials/Material.h"
#include "Support/TestSupport.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <limits>

using namespace Hyperion;

namespace
{
void ExpectError(const std::function<void()>& InAction, std::string_view InText)
{
	try
	{
		InAction();
	}
	catch (const std::exception& Error)
	{
		HYP_CHECK(std::string_view(Error.what()).find(InText) != std::string_view::npos);
		return;
	}
	throw std::runtime_error("Expected material error: " + std::string(InText));
}

FMaterialDescription MakeDescription()
{
	FMaterialDescription Result;
	Result.Name = "Material CPU contract";
	FMaterialPass Pass;
	Pass.Vertex = {"Custom.hlsl", "VSMain"};
	Pass.Pixel = {"Custom.hlsl", "PSMain"};
	Result.Passes.push_back(Pass);
	FMaterialParameterDeclaration Strength;
	Strength.Name = "Pixel:Surface.DetailStrength";
	Strength.Type = FMaterialParameterType::Numeric(EMaterialScalar::Float);
	Strength.Default = FMaterialValue::Float(0.25F);
	Result.Parameters.push_back(Strength);
	Result.Parameters.push_back(
	    DeclareMaterialSemantic("Vertex:ViewData.Eye", "Engine.View.CameraPosition", *GetStandardMaterialSemantics()));
	return Result;
}

void CheckEdits()
{
	auto Definition = std::make_shared<const FMaterialDefinition>(MakeDescription());
	FMaterialInstance First(Definition);
	FMaterialInstance Second(Definition);
	const auto Original = First.Freeze();
	const FMaterialParameterHandle Handle = First.Find("DetailStrength");
	First.Set(Handle, FMaterialValue::Float(0.75F));
	HYP_CHECK(First.GetRevision() == 2 && Second.GetRevision() == 1 && Original->Overrides.empty());
	const auto Edited = First.Freeze();
	ExpectError(
	    [&]
	    {
		    First.Set(Handle, FMaterialValue::Int(3));
	    },
	    "type mismatch");
	ExpectError(
	    [&]
	    {
		    First.Set("Eye", FMaterialValue::Float(FVec3{1, 2, 3}));
	    },
	    "locked");
	ExpectError(
	    [&]
	    {
		    First.SetSemantic("Engine.View.CameraPosition", FMaterialValue::Float(FVec3{}));
	    },
	    "locked");
	ExpectError(
	    [&]
	    {
		    First.Set("Unknown", FMaterialValue::Float(1));
	    },
	    "InterfaceNotReady");
	ExpectError(
	    [&]
	    {
		    First.Set(Handle, FMaterialValue::Float(std::numeric_limits<float>::infinity()));
	    },
	    "finite");
	HYP_CHECK(First.Freeze() == Edited);
	First.Set(Handle, FMaterialValue::Float(0.75F));
	HYP_CHECK(First.Freeze() == Edited);
	FMaterialDescription Replacement = MakeDescription();
	Replacement.Version = 2;
	auto NewDefinition = std::make_shared<const FMaterialDefinition>(Replacement);
	FPreparedMaterialInterface Prepared{NewDefinition,
	                                    std::make_shared<const FMaterialParameterSchema>(Replacement.Parameters, 2)};
	First.ReplaceDefinition(Prepared);
	ExpectError(
	    [&]
	    {
		    First.Set(Handle, FMaterialValue::Float(1));
	    },
	    "Stale");
	ExpectError(
	    [&]
	    {
		    First.Find("Typo");
	    },
	    "UnknownParameter");
	HYP_CHECK(First.Freeze()->Overrides.front().Value == FMaterialValue::Float(0.75F));
	std::atomic<bool> bRejected{};
	std::thread Worker(
	    [&]
	    {
		    try
		    {
			    First.Freeze();
		    }
		    catch (const std::logic_error&)
		    {
			    bRejected = true;
		    }
		    HYP_CHECK(Original->Definition == Definition);
	    });
	Worker.join();
	HYP_CHECK(bRejected);
}

void CheckResolution()
{
	FMaterialDescription Description = MakeDescription();
	Description.Parameters[1].OverridePolicy = EMaterialOverridePolicy::AllowOverride;
	auto Definition = std::make_shared<const FMaterialDefinition>(Description);
	FMaterialInstance Instance(Definition);
	const FMaterialParameterValues Provider{{"Engine.View.CameraPosition", FMaterialValue::Float(FVec3{1, 2, 3})}};
	ExpectError(
	    [&]
	    {
		    ResolveMaterialParameters(*Instance.Freeze(), {});
	    },
	    "Missing");
	HYP_CHECK(ResolveMaterialParameters(*Instance.Freeze(), Provider)[1].Value == Provider[0].Value);
	Instance.SetSemantic("Engine.View.CameraPosition", FMaterialValue::Float(FVec3{4, 5, 6}));
	HYP_CHECK(ResolveMaterialParameters(*Instance.Freeze(), Provider)[1].Value ==
	          FMaterialValue::Float(FVec3{4, 5, 6}));
	const FMaterialParameterValues Object{{"Eye", FMaterialValue::Float(FVec3{7, 8, 9})}};
	const FMaterialParameterValues Draw{{"Eye", FMaterialValue::Float(FVec3{10, 11, 12})}};
	HYP_CHECK(ResolveMaterialParameters(*Instance.Freeze(), Provider, Object, Draw)[1].Value == Draw[0].Value);
	Instance.Clear("Eye");
	HYP_CHECK(ResolveMaterialParameters(*Instance.Freeze(), Provider)[1].Value == Provider[0].Value);
	FMaterialParameterValues Duplicate = Object;
	Duplicate.push_back({"Vertex:ViewData.Eye", Object[0].Value});
	ExpectError(
	    [&]
	    {
		    ResolveMaterialParameters(*Instance.Freeze(), Provider, Duplicate);
	    },
	    "Duplicate");
	Description.Parameters.push_back(Description.Parameters[0]);
	Description.Parameters.back().Name = "Vertex:Other.DetailStrength";
	Description.Parameters.back().bActive = false;
	FMaterialInstance Ambiguous(std::make_shared<const FMaterialDefinition>(Description));
	ExpectError(
	    [&]
	    {
		    Ambiguous.Find("DetailStrength");
	    },
	    "Ambiguous");
	HYP_CHECK(Ambiguous.Set("Vertex:Other.DetailStrength", FMaterialValue::Float(2)) == EMaterialWriteResult::Inactive);
}

void CheckResources()
{
	std::vector<FMaterialTextureMip> Mips{{2, 2, std::vector<std::uint8_t>(16, 127)}, {1, 1, {1, 2, 3, 4}}};
	auto Texture = std::make_shared<const FMaterialTextureSource>(EMaterialTextureEncoding::Srgb, Mips);
	Mips.front().Bytes.clear();
	HYP_CHECK(Texture->GetMips().front().Bytes.size() == 16);
	ExpectError(
	    [&]
	    {
		    FMaterialTextureSource Invalid(EMaterialTextureEncoding::Linear, Mips);
	    },
	    "mip");
	std::vector<std::byte> Bytes(32, std::byte{13});
	auto Buffer = std::make_shared<const FMaterialReadBufferSource>(Bytes);
	Bytes.clear();
	HYP_CHECK(Buffer->GetBytes().size() == 32 && Buffer->GetBytes()[0] == std::byte{13});
	FMaterialBufferView View{Buffer, EMaterialBufferViewKind::Structured, 8, 16, 8};
	View.Validate();
	View.Offset = std::numeric_limits<std::uint64_t>::max();
	ExpectError(
	    [&]
	    {
		    View.Validate();
	    },
	    "exceeds");
	View = {Buffer, EMaterialBufferViewKind::Raw, 1, 16};
	ExpectError(
	    [&]
	    {
		    View.Validate();
	    },
	    "four-byte");
	const FMaterialValue Textures =
	    FMaterialValue::Array(std::vector<FMaterialValue>(8, FMaterialValue::FromTexture(Texture)));
	HYP_CHECK(Textures.Type.ArrayCount == 8);
	FMaterialSampler Sampler;
	Sampler.MinLod = 3;
	Sampler.MaxLod = 2;
	ExpectError(
	    [&]
	    {
		    FMaterialValue::FromSampler(Sampler);
	    },
	    "LOD");
	const FMat4 Matrix = Translation({2, 3, 4});
	const FMaterialValue Value = FMaterialValue::Matrix(Matrix);
	HYP_CHECK(Value.Words[3] == FMaterialValue::Float(2).Words[0]);
	FMaterialValue Invalid = FMaterialValue::Bool(true);
	Invalid.Words[0] = 2;
	ExpectError(
	    [&]
	    {
		    Invalid.Validate();
	    },
	    "booleans");
}

void CheckStateAndRegistry()
{
	FMaterialDescription Description = MakeDescription();
	FMaterialDefinition Definition(Description);
	HYP_CHECK(!Definition.HasPass("ShadowCaster"));
	HYP_CHECK(ResolveMaterialPass(Definition.GetPass(), {}).IsAvailable());
	Description.Passes.front().State.bAlphaToCoverage = true;
	FMaterialDefinition Coverage(Description);
	HYP_CHECK(Coverage.GetPass().State.bAlphaToCoverage);
	HYP_CHECK(ResolveMaterialPass(Coverage.GetPass(), {}).Availability == EMaterialAvailability::Incompatible);
	Description.Passes.push_back(Description.Passes.front());
	ExpectError(
	    [&]
	    {
		    FMaterialDefinition Invalid(Description);
	    },
	    "Duplicate");
	Description = MakeDescription();
	Description.Passes.front().Vertex.Defines = {{"MODE", "1"}, {"MODE", "2"}};
	ExpectError(
	    [&]
	    {
		    FMaterialDefinition Invalid(Description);
	    },
	    "duplicate");
	FMaterialSemanticRegistry Registry;
	HYP_CHECK(Registry.Normalize("ALBEDO_TEXTURE") == "Pbr.BaseColorTexture");
	FMaterialSemantic Semantic{"Game.Wetness", FMaterialParameterType::Numeric(EMaterialScalar::Float),
	                           EMaterialScope::Scene, "Fraction 0-1"};
	Registry.Register(Semantic);
	ExpectError(
	    [&]
	    {
		    Registry.Register(Semantic);
	    },
	    "Conflicting");
	Semantic.Name = "Engine.Custom";
	ExpectError(
	    [&]
	    {
		    Registry.Register(Semantic);
	    },
	    "custom namespace");
	Registry.Freeze();
	Semantic.Name = "Game.Other";
	ExpectError(
	    [&]
	    {
		    Registry.Register(Semantic);
	    },
	    "before freeze");
}
} // namespace

int main()
{
	try
	{
		CheckEdits();
		CheckResolution();
		CheckResources();
		CheckStateAndRegistry();
		std::cout << "PASS: CPU material contracts, snapshots, semantics and owned resources\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
