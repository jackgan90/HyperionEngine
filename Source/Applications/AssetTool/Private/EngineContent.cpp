#include "EngineContent.h"
#include "Hyperion/AssetImport/ModelImport.h"
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Scene/PrimitiveShapes.h"
#include <array>

namespace Hyperion
{
namespace
{
template<class T>
FAssetRef Publish(FIOService& InIO, const std::string& InPath, const T& InValue, const std::string& InId)
{
	FAssetHeader Header;
	Header.Id = InId;
	const auto Encoded = EncodeAsset(RecordType<T>(), &InValue, Header);
	InIO.WriteAsync(InPath, Encoded.Bytes).Get(InIO.TaskSystem());
	return {Encoded.Header.Id, InPath, Encoded.Header.TypeId, Encoded.Header.Revision};
}

FAssetRef BuildDefaultMaterial(FIOService& InIO)
{
	FModelSource Source;
	Source.Primitives.push_back(MakePrimitiveShape(EPrimitiveShape::Cube));
	FModelMaterial Default;
	Default.Name = "Default primitive";
	Default.BaseColor = {.65f, .65f, .65f, 1};
	Default.Metallic = 0;
	Default.Roughness = .65f;
	Default.bDoubleSided = true;
	Source.Materials.push_back(Default);
	FModelNode Node;
	Node.Primitives = {0};
	Source.Nodes.push_back(Node);
	Source.Roots = {0};
	const auto Split = SplitModelSource(Source);
	FAssetRef White;
	for (const auto& Product : Split.Products)
	{
		if (Product.Type->CppType == typeid(FTextureAsset))
		{
			White =
			    Publish(InIO, "/Engine/Textures/PrimitiveWhite.hasset",
			            *static_cast<const FTextureAsset*>(Product.Object.get()), "13f964852a4143a0b38d000000000101");
		}
	}
	for (const auto& Product : Split.Products)
	{
		if (Product.Type->CppType == typeid(FMaterialAsset))
		{
			auto Material = *static_cast<const FMaterialAsset*>(Product.Object.get());
			for (auto& Value : Material.Values)
			{
				if (Value.Value.Texture)
				{
					Value.Value.Texture = White;
				}
			}
			return Publish(InIO, "/Engine/Materials/DefaultPrimitive.hasset", Material,
			               "13f964852a4143a0b38d000000000100");
		}
	}
	throw std::logic_error("Default material generation produced no material");
}
} // namespace

void BuildEnginePlacementContent(FIOService& InIO, std::ostream& InOutput)
{
	const auto Surface = BuildDefaultMaterial(InIO);
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		auto Primitive = MakePrimitiveShape(static_cast<EPrimitiveShape>(Index));
		FModelAsset Model;
		Model.Name = Primitive.Name;
		Model.Primitives.push_back(std::move(Primitive));
		Model.MaterialSlots.push_back(Surface);
		FModelNode Node;
		Node.Id = "root";
		Node.Name = Model.Name;
		Node.Primitives = {0};
		Model.Nodes.push_back(Node);
		Model.Roots = {0};
		ValidateModel(Model);
		Publish(InIO, "/Engine/Models/Primitives/" + Model.Name + ".hasset", Model,
		        "13f964852a4143a0b38d00000000020" + std::to_string(Index));
		InOutput << "Published " << Model.Name << '\n';
	}
	constexpr std::array Names{"PointLight", "DirectionalLight", "SpotLight"};
	for (std::size_t Index = 0; Index < Names.size(); ++Index)
	{
		const std::string Path = std::string("/Engine/Editor/Icons/") + Names[Index];
		auto Image = DecodeImage(*InIO.ReadAsync(Path + ".png").Get(InIO.TaskSystem()));
		// GUI samples display-encoded pixels through UNORM, so retain the source byte values.
		auto Texture = BuildTextureAsset(Names[Index], EMaterialTextureEncoding::Linear,
		                                 {Image.Width, Image.Height, std::move(Image.Rgba)});
		// Keep the generated originals; the shipped editor texture only needs small-screen mips.
		while (Texture.Mips.front().Width > 512 || Texture.Mips.front().Height > 512)
		{
			Texture.Mips.erase(Texture.Mips.begin());
		}
		Publish(InIO, Path + ".hasset", Texture, "13f964852a4143a0b38d00000000030" + std::to_string(Index));
		InOutput << "Published " << Names[Index] << '\n';
	}
}
} // namespace Hyperion
