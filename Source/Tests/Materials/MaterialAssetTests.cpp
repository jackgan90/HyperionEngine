#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Support/TestSupport.h"
#include <iostream>
#include <limits>

using namespace Hyperion;

namespace
{
void Reject(const std::function<void()>& InAction)
{
	bool bFailed{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bFailed = true;
	}
	HYP_CHECK(bFailed);
}

void CheckTextures()
{
	const FMaterialTextureMip Base{2, 1, {0, 0, 0, 0, 255, 255, 255, 255}};
	auto Linear = BuildTextureAsset("linear", EMaterialTextureEncoding::Linear, Base);
	auto Srgb = BuildTextureAsset("color", EMaterialTextureEncoding::Srgb, Base);
	HYP_CHECK(Linear.Mips.back().Bytes[0] == 128);
	HYP_CHECK(Srgb.Mips.back().Bytes[0] == 188 && Srgb.Mips.back().Bytes[3] == 128);
	const auto Encoded = EncodeAsset(RecordType<FTextureAsset>(), &Srgb);
	const auto Document = DecodeAsset(std::make_shared<const std::vector<std::byte>>(Encoded.Bytes));
	const auto Loaded =
	    std::static_pointer_cast<FTextureAsset>(ReadRecord(RecordType<FTextureAsset>(), Document.Object));
	HYP_CHECK(Loaded->Mips == Srgb.Mips && Loaded->Encoding == Srgb.Encoding);
	FMaterialTextureSource Source(Loaded);
	HYP_CHECK(&Source.GetMips() == &Loaded->Mips);
	Srgb.Mips.pop_back();
	Reject(
	    [&]
	    {
		    ValidateTextureAsset(Srgb);
	    });
	Srgb = Linear;
	Srgb.Mips[0].Bytes.pop_back();
	Reject(
	    [&]
	    {
		    ValidateTextureAsset(Srgb);
	    });
	Reject(
	    [&]
	    {
		    BuildTextureAsset("invalid", EMaterialTextureEncoding::Linear, {0, 1, {}});
	    });
}

FMaterialAsset MakeMaterial()
{
	FMaterialDescription Description;
	Description.Name = "Authored";
	FMaterialPass Pass;
	Pass.Vertex = {"SharedAsset.hlsl", "CustomVertex", {{"VARIANT", "2"}}};
	Pass.Pixel = {"SharedAsset.hlsl", "CustomPixel", {{"TINT", "1"}}};
	Pass.InstanceArrays = {{"InstanceData", "Objects"}};
	Pass.bAllowBatchReordering = true;
	Pass.State.bStencil = true;
	Pass.State.FrontStencil.Fail = EMaterialStencilOp::Replace;
	Pass.DynamicState.StencilReference = 7;
	Description.Passes.push_back(Pass);
	Pass.Usage = "OtherPass";
	Pass.State.bBlend = true;
	Pass.State.SourceRgb = EMaterialBlendFactor::SourceAlpha;
	Description.Passes.push_back(Pass);
	FMaterialParameterDeclaration Numeric;
	Numeric.Name = "Layer";
	Numeric.Type.Kind = EMaterialValueKind::Structure;
	Numeric.Type.MemberNames = {"Tint", "Weights"};
	Numeric.Type.Members = {FMaterialParameterType::Numeric(EMaterialScalar::Float, 4),
	                        FMaterialParameterType::Array(FMaterialParameterType::Numeric(EMaterialScalar::Uint), 2)};
	FMaterialValue Value;
	Value.Type = Numeric.Type;
	Value.Elements = {FMaterialValue::Float(FVec4{.1f, .2f, .3f, 1}),
	                  FMaterialValue::Array({FMaterialValue::Uint(1), FMaterialValue::Uint(2)})};
	Numeric.Default = Value;
	Description.Parameters.push_back(Numeric);
	FMaterialParameterDeclaration Texture;
	Texture.Name = "Texture";
	Texture.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Description.Parameters.push_back(Texture);
	FMaterialParameterDeclaration Sampler;
	Sampler.Name = "Sampler";
	Sampler.Type = FMaterialParameterType::Resource(EMaterialValueKind::Sampler);
	Description.Parameters.push_back(Sampler);
	auto Result = PersistMaterialDescription(Description);
	FMaterialAssetValue TextureValue;
	TextureValue.Type = Texture.Type;
	TextureValue.Texture = FAssetRef{"", "texture.hasset", RecordType<FTextureAsset>().Id, ""};
	Result.Values.push_back({"Texture", TextureValue});
	FMaterialSampler SamplerValue;
	SamplerValue.U = EMaterialAddressMode::Mirror;
	SamplerValue.MaxAnisotropy = 8;
	Result.Values.push_back({"Sampler", PersistMaterialValue(FMaterialValue::FromSampler(SamplerValue))});
	return Result;
}

void CheckMaterials()
{
	auto Material = MakeMaterial();
	const auto Bytes = EncodeAsset(RecordType<FMaterialAsset>(), &Material);
	const auto Document = DecodeAsset(std::make_shared<const std::vector<std::byte>>(Bytes.Bytes));
	const auto Loaded =
	    std::static_pointer_cast<FMaterialAsset>(ReadRecord(RecordType<FMaterialAsset>(), Document.Object));
	HYP_CHECK(Loaded->Values == Material.Values);
	HYP_CHECK(Loaded->Parameters[0].Default == Material.Parameters[0].Default);
	HYP_CHECK(Loaded->Passes[0].Vertex == Material.Passes[0].Vertex);
	HYP_CHECK(Loaded->Passes[0].State == Material.Passes[0].State);
	HYP_CHECK(Loaded->Passes[0].DynamicState == Material.Passes[0].DynamicState);
	HYP_CHECK(Loaded->Passes[0].InstanceArrays == Material.Passes[0].InstanceArrays);
	HYP_CHECK(Document.Header.Dependencies.size() == 1);
	HYP_CHECK(Document.Header.Dependencies[0].Reference == *Material.Values[0].Value.Texture);
	Material.Values[0].Value.Texture->TypeId = "hyperion.materialasset";
	Reject(
	    [&]
	    {
		    ValidateMaterialAsset(Material);
	    });
	Material = MakeMaterial();
	Material.Passes[0].Pixel.Defines.push_back({"TINT", "2"});
	Reject(
	    [&]
	    {
		    ValidateMaterialAsset(Material);
	    });
	Material = MakeMaterial();
	Material.Values.push_back({"NoSuchParameter", PersistMaterialValue(FMaterialValue::Float(1))});
	Reject(
	    [&]
	    {
		    ValidateMaterialAsset(Material);
	    });
	auto Target = std::make_shared<const FMaterialTextureSource>(FMaterialDepthTexture{});
	Reject(
	    [&]
	    {
		    PersistMaterialValue(FMaterialValue::FromTexture(Target));
	    });
	const std::array<std::byte, 4> Raw{};
	auto Buffer = std::make_shared<const FMaterialReadBufferSource>(Raw);
	Reject(
	    [&]
	    {
		    PersistMaterialValue(FMaterialValue::FromBuffer({Buffer, EMaterialBufferViewKind::Raw, 0, 4}));
	    });
}
} // namespace

int main()
{
	try
	{
		CheckTextures();
		CheckMaterials();
		std::cout << "PASS: reflected material/texture assets, typed values, shader state and resource boundaries\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
