#include "ModelMaterials.h"
#include "Hyperion/Renderer/MaterialBlocks.h"
#include <map>
#include <mutex>

namespace Hyperion
{
namespace
{
const std::array<std::string, 5> Roles{"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"};

FMaterialPass ModelPass(const FModelMaterial& InMaterial)
{
	FMaterialPass Pass;
	Pass.Vertex = {"Model.hlsl", "VSMain"};
	Pass.Pixel = {"Model.hlsl", "PSMain"};
	Pass.InstanceArrays = {{"HyperionObjectV1", "ObjectInstances"}, {"HyperionMaterialV1", "SurfaceInstances"}};
	Pass.bAllowBatchReordering = InMaterial.AlphaMode != EAlphaMode::Blend;
	Pass.bSrgbTarget = true;
	Pass.bAlphaClip = InMaterial.AlphaMode == EAlphaMode::Mask;
	Pass.Queue = InMaterial.AlphaMode == EAlphaMode::Blend  ? EMaterialQueue::Transparent
	             : InMaterial.AlphaMode == EAlphaMode::Mask ? EMaterialQueue::Masked
	                                                        : EMaterialQueue::Opaque;
	Pass.State.bDepthTest = true;
	Pass.State.bDepthWrite = InMaterial.AlphaMode != EAlphaMode::Blend;
	Pass.State.bBlend = InMaterial.AlphaMode == EAlphaMode::Blend;
	Pass.State.SourceRgb = EMaterialBlendFactor::SourceAlpha;
	Pass.State.DestinationRgb = EMaterialBlendFactor::InverseSourceAlpha;
	Pass.State.DestinationAlpha = EMaterialBlendFactor::InverseSourceAlpha;
	Pass.State.Cull = InMaterial.bDoubleSided ? EMaterialCull::None : EMaterialCull::Back;
	return Pass;
}

std::shared_ptr<const FMaterialDefinition> ModelDefinition(const FModelMaterial& InMaterial)
{
	static std::mutex Mutex;
	static std::map<std::pair<EAlphaMode, bool>, std::weak_ptr<const FMaterialDefinition>> Definitions;
	std::lock_guard Lock(Mutex);
	auto& Cached = Definitions[{InMaterial.AlphaMode, InMaterial.bDoubleSided}];
	if (auto Existing = Cached.lock())
	{
		return Existing;
	}
	FMaterialDescription Description;
	Description.Name = "Builtin glTF PBR";
	Description.Passes.push_back(ModelPass(InMaterial));
	const auto Semantics = GetStandardMaterialSemantics();
	Description.Parameters = GetStandardMaterialBlockParameters("HyperionMaterialV1", *Semantics);
	for (const auto& Role : Roles)
	{
		for (const auto* Kind : {"Texture", "Sampler"})
		{
			auto Parameter = DeclareMaterialSemantic(Role + Kind, "Pbr." + Role + Kind, *Semantics);
			Parameter.Targets = {Role + Kind};
			Description.Parameters.push_back(std::move(Parameter));
		}
	}
	auto Definition = std::make_shared<const FMaterialDefinition>(std::move(Description));
	Cached = Definition;
	return Definition;
}

EMaterialAddressMode Address(ERHIAddressMode InAddress)
{
	switch (InAddress)
	{
		case ERHIAddressMode::Repeat:
			return EMaterialAddressMode::Repeat;
		case ERHIAddressMode::Clamp:
			return EMaterialAddressMode::Clamp;
		case ERHIAddressMode::Mirror:
			return EMaterialAddressMode::Mirror;
		case ERHIAddressMode::Border:
			return EMaterialAddressMode::Border;
		case ERHIAddressMode::MirrorOnce:
			return EMaterialAddressMode::MirrorOnce;
	}
	throw std::invalid_argument("Invalid imported sampler address");
}

FMaterialSampler Sampler(const FSamplerDesc& InSampler)
{
	FMaterialSampler Result;
	Result.U = Address(InSampler.U);
	Result.V = Address(InSampler.V);
	Result.W = Address(InSampler.W);
	Result.bMinLinear = InSampler.bMinLinear;
	Result.bMagLinear = InSampler.bMagLinear;
	Result.bMipLinear = InSampler.bMipLinear;
	Result.MinLod = InSampler.MinLod;
	Result.MaxLod = InSampler.bMipmapped ? InSampler.MaxLod : 0;
	return Result;
}

void SetFactors(FMaterialInstance& InInstance, const FModelMaterial& InMaterial)
{
	InInstance.SetSemantic("Pbr.BaseColorFactor", FMaterialValue::Float(InMaterial.BaseColor));
	InInstance.SetSemantic("Pbr.EmissiveFactor", FMaterialValue::Float(InMaterial.Emissive));
	InInstance.SetSemantic("Pbr.NormalScale", FMaterialValue::Float(InMaterial.NormalScale));
	InInstance.SetSemantic("Pbr.MetallicFactor", FMaterialValue::Float(InMaterial.Metallic));
	InInstance.SetSemantic("Pbr.RoughnessFactor", FMaterialValue::Float(InMaterial.Roughness));
	InInstance.SetSemantic("Pbr.OcclusionStrength", FMaterialValue::Float(InMaterial.OcclusionStrength));
	InInstance.SetSemantic("Pbr.AlphaCutoff", FMaterialValue::Float(InMaterial.AlphaCutoff));
	InInstance.SetSemantic("Pbr.AlphaMode", FMaterialValue::Uint(static_cast<std::uint32_t>(InMaterial.AlphaMode)));
	InInstance.SetSemantic("Pbr.DoubleSided", FMaterialValue::Bool(InMaterial.bDoubleSided));
	InInstance.SetSemantic("Pbr.Unlit", FMaterialValue::Bool(InMaterial.bUnlit));
	InInstance.SetSemantic("Pbr.HasNormal", FMaterialValue::Bool(InMaterial.NormalTexture.Image >= 0));
	const std::array Uv{InMaterial.BaseColorTexture.TexCoord, InMaterial.MetallicRoughnessTexture.TexCoord,
	                    InMaterial.NormalTexture.TexCoord, InMaterial.OcclusionTexture.TexCoord,
	                    InMaterial.EmissiveTexture.TexCoord};
	for (std::size_t Index = 0; Index < Roles.size(); ++Index)
	{
		InInstance.SetSemantic("Pbr." + Roles[Index] + "UvSet", FMaterialValue::Uint(Uv[Index]));
	}
}
} // namespace

std::vector<FVertexAttribute> ModelVertexAttributes()
{
	return {{"POSITION", 0, EVertexFormat::Float3, offsetof(FModelVertex, Position)},
	        {"NORMAL", 0, EVertexFormat::Float3, offsetof(FModelVertex, Normal)},
	        {"TANGENT", 0, EVertexFormat::Float4, offsetof(FModelVertex, Tangent)},
	        {"COLOR", 0, EVertexFormat::Float4, offsetof(FModelVertex, Color)},
	        {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FModelVertex, Uv0)},
	        {"TEXCOORD", 1, EVertexFormat::Float2, offsetof(FModelVertex, Uv1)}};
}

std::vector<FRenderMaterialDesc> PrepareModelMaterials(const FPreparedModel& InModel, FShaderCompiler& InCompiler,
                                                       EShaderFormat InFormat)
{
	std::vector<std::shared_ptr<const FMaterialTextureSource>> Textures;
	for (const auto& Texture : InModel.Textures)
	{
		std::vector<FMaterialTextureMip> Mips;
		for (const auto& Mip : Texture.Mips)
		{
			Mips.push_back({Mip.Width, Mip.Height, Mip.Rgba});
		}
		Textures.push_back(std::make_shared<const FMaterialTextureSource>(
		    Texture.bSrgb ? EMaterialTextureEncoding::Srgb : EMaterialTextureEncoding::Linear, std::move(Mips)));
	}
	std::map<std::uint64_t, std::shared_ptr<const FCompiledMaterialDefinition>> Programs;
	std::vector<FRenderMaterialDesc> Result;
	for (const auto& Material : InModel.Materials)
	{
		const auto Definition = ModelDefinition(Material.Material);
		auto& Program = Programs[Definition->GetIdentity()];
		if (!Program)
		{
			Program = std::make_shared<const FCompiledMaterialDefinition>(
			    CompileMaterialDefinition(InCompiler, Definition, InFormat));
		}
		// Worker-local authoring has one owner; only the resulting immutable snapshot is published.
		FMaterialInstance Instance(Program->Interface);
		SetFactors(Instance, Material.Material);
		for (std::size_t Index = 0; Index < Roles.size(); ++Index)
		{
			Instance.SetSemantic("Pbr." + Roles[Index] + "Texture",
			                     FMaterialValue::FromTexture(Textures.at(Material.Textures[Index])));
			Instance.SetSemantic("Pbr." + Roles[Index] + "Sampler",
			                     FMaterialValue::FromSampler(Sampler(Material.Samplers[Index])));
		}
		FRenderMaterialDesc Surface;
		Surface.Surface = Instance.Freeze();
		Surface.Compiled = Program;
		Result.push_back(std::move(Surface));
	}
	return Result;
}
} // namespace Hyperion
