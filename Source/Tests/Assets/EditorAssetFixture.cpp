#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Materials/PbrMaterial.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/ShaderSourceSupport.h"
#include <fstream>

namespace Hyperion
{
namespace
{
template<class T> FAssetRef StoreFixture(const std::filesystem::path& InPath, const T& InValue)
{
	const auto Encoded = EncodeAsset(RecordType<T>(), &InValue);
	FLocalFileSystem Files;
	Files.WriteAtomic(InPath, Encoded.Bytes);
	return {Encoded.Header.Id, "/Game/" + InPath.filename().generic_string(), Encoded.Header.TypeId, {}};
}

FMaterialAsset CustomMaterial(const FAssetRef& InTexture)
{
	FMaterialDescription Description;
	Description.Name = "Custom shader";
	FMaterialPass Pass;
	Pass.Vertex = {"/Game/Shaders/SharedAsset.hlsl", "AssetVertex"};
	Pass.Pixel = {"/Game/Shaders/SharedAsset.hlsl", "AssetPixel", {{"ASSET_GAIN", "1"}}};
	Pass.bSrgbTarget = true;
	Description.Passes.push_back(Pass);
	for (const auto& Entry : FMaterialParameterValues{{"Tint", FMaterialValue::Float(FVec4{.1f, .6f, 1, 1})},
	                                                  {"ImageSampler", FMaterialValue::FromSampler({})}})
	{
		FMaterialParameterDeclaration Parameter;
		Parameter.Name = Entry.Name;
		Parameter.Type = Entry.Value.Type;
		Parameter.Targets = {Entry.Name == "Tint" ? "Surface.Tint" : Entry.Name};
		Parameter.Default = Entry.Value;
		Description.Parameters.push_back(Parameter);
	}
	FMaterialParameterDeclaration Texture;
	Texture.Name = "ImageTexture";
	Texture.Type = FMaterialParameterType::Resource(EMaterialValueKind::Texture2D);
	Description.Parameters.push_back(Texture);
	auto Material = PersistMaterialDescription(Description);
	FMaterialAssetValue Value;
	Value.Type = Texture.Type;
	Value.Texture = InTexture;
	Material.Values.push_back({Texture.Name, Value});
	return Material;
}
} // namespace

void WriteAssetEditorFixture(const std::filesystem::path& InRoot)
{
	std::filesystem::create_directories(InRoot / "Shaders");
	FLocalFileSystem Files;
	Files.WriteAtomic(InRoot / "Shaders/SharedAsset.hlsl",
	                  Files.Read(TestShaderRoot() / "SharedAsset.hlsl", 1024 * 1024));
	FMaterialTextureMip Base{32, 32};
	Base.Bytes.resize(32 * 32 * 4);
	for (std::size_t I = 0; I < 32 * 32; ++I)
	{
		WriteTexturePixel(Base, ETextureFormat::Rgba8Unorm, I, {float(I % 32) / 31, float(I / 32) / 31, .3f, 1});
	}
	const auto Texture =
	    StoreFixture(InRoot / "Texture.hasset", BuildTextureAsset("Texture", EMaterialTextureEncoding::Linear, Base));
	StoreFixture(InRoot / "SecondTexture.hasset",
	             BuildTextureAsset("Second texture", EMaterialTextureEncoding::Srgb, Base));
	auto BaseMaterial = ReadValue<FMaterialAsset>(
	    DecodeAsset(Files.Read(std::filesystem::path(HYP_SOURCE_DIR) / "Content/Materials/DefaultPrimitive.hasset",
	                           16 * 1024 * 1024))
	        .Object);
	BaseMaterial.Name = "Material";
	for (auto& Value : BaseMaterial.Values)
	{
		if (Value.Value.Texture)
		{
			Value.Value.Texture = Texture;
		}
	}
	const auto Material = StoreFixture(InRoot / "Material.hasset", BaseMaterial);
	StoreFixture(InRoot / "CustomMaterial.hasset", CustomMaterial(Texture));
	auto Model = ReadValue<FModelAsset>(
	    DecodeAsset(Files.Read(std::filesystem::path(HYP_SOURCE_DIR) / "Content/Models/Primitives/Cube.hasset",
	                           16 * 1024 * 1024))
	        .Object);
	Model.Name = "Model";
	Model.MaterialSlots = {Material};
	for (auto& Primitive : Model.Primitives)
	{
		Primitive.Material = 0;
	}
	const auto ModelReference = StoreFixture(InRoot / "Model.hasset", Model);
	FTextureAsset Cube;
	Cube.Name = "Radiance";
	Cube.Dimension = ETextureDimension::Cube;
	Cube.Format = ETextureFormat::Rgba32Float;
	Cube.Mips = {{1, 1, std::vector<std::uint8_t>(6 * 16)}};
	for (std::size_t I = 0; I < 6; ++I)
	{
		WriteTexturePixel(Cube.Mips[0], Cube.Format, I, {.3f + float(I) * .1f, .4f, .6f, 1});
	}
	const auto Radiance = StoreFixture(InRoot / "Radiance.hasset", Cube);
	Cube.Name = "Specular";
	const auto Specular = StoreFixture(InRoot / "Specular.hasset", Cube);
	const auto Brdf = StoreFixture(InRoot / "Brdf.hasset", BuildEnvironmentBrdf(4, 16));
	FSkyAsset Sky{"Sky", Radiance, Specular, Brdf};
	Sky.Irradiance = ProjectEnvironmentSh(Cube);
	StoreFixture(InRoot / "Sky.hasset", Sky);
	FSceneManifest Scene;
	Scene.Assets.push_back({"model", ModelReference});
	FSceneNodeEntry ModelNode;
	ModelNode.Id = "model";
	ModelNode.Name = "Scene model";
	ModelNode.Model = FSceneNodeModel{"model"};
	ModelNode.Model->Surface.Reference = Material;
	ModelNode.Model->Surface.Overrides.push_back(
	    {"Pbr.MetallicFactor", PersistMaterialValue(FMaterialValue::Float(.42f))});
	Scene.Nodes.push_back(ModelNode);
	FSceneNodeEntry Camera;
	Camera.Id = "camera";
	Camera.Name = "Camera";
	Camera.Camera = FSceneCamera{};
	Camera.Transform = SceneCameraTransform({3, 2, 5}, {});
	Scene.Nodes.push_back(Camera);
	Scene.DefaultCamera = "camera";
	FSceneNodeEntry Light;
	Light.Id = "light";
	Light.DirectionalLight = FSceneDirectionalLight{};
	Light.Transform = SceneCameraTransform({}, {-1, -1, -1});
	Scene.Nodes.push_back(Light);
	Scene.MainDirectionalLight = "light";
	StoreFixture(InRoot / "Scene.hasset", Scene);
	std::ofstream(InRoot / "Broken.hasset") << "invalid asset";
}
} // namespace Hyperion
