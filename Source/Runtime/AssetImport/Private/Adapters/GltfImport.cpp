#define CGLTF_IMPLEMENTATION
#include "GltfImportInternal.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
namespace
{
using namespace Private;
using FGltfData = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

FGltfData ParseSource(FAssetImportContext& InContext)
{
	HYP_PERF_SCOPE_C(Assets, ParseGltf);
	cgltf_options Options{};
	Options.memory.alloc_func = [](void*, cgltf_size InSize) -> void*
	{
		if (InSize > MaxImportBytes)
		{
			return nullptr;
		}
		try
		{
			return Allocate(InSize, alignof(std::max_align_t), EMemoryTag::Assets);
		}
		catch (...)
		{
			return nullptr;
		}
	};
	Options.memory.free_func = [](void*, void* InPointer)
	{
		Deallocate(InPointer);
	};
	// A defensive hook ensures accidental future file-loader calls cannot bypass IO.
	Options.file.read = [](const cgltf_memory_options*, const cgltf_file_options*, const char*, cgltf_size*, void**)
	{
		return cgltf_result_io_error;
	};
	cgltf_data* Raw{};
	Require(cgltf_parse(&Options, InContext.Bytes->data(), InContext.Bytes->size(), &Raw) == cgltf_result_success,
	        "parse failed");
	return FGltfData(Raw, cgltf_free);
}

// Owns the parsed document and every external buffer referenced by its accessors.
// The source context remains alive for the entire synchronous importer invocation.
class FGltfImport
{
public:
	explicit FGltfImport(FAssetImportContext& InContext)
	    : Context(InContext), Data(ParseSource(InContext)),
	      Model(std::static_pointer_cast<FModelAsset>(RecordType<FModelAsset>().Create())),
	      Total(InContext.Bytes->size()), MeshPrimitives(Data->meshes_count)
	{
	}

	std::shared_ptr<FModelAsset> Run();

	FModelPrimitive Primitive(std::size_t InMesh, std::size_t InPrimitive)
	{
		Run();
		return Model->Primitives.at(MeshPrimitives.at(InMesh).at(InPrimitive));
	}

private:
	void CheckExtensions();
	void LoadBuffers();
	void LoadNodes();
	void LoadImages();
	void LoadMeshes();
	void SelectRoots();
	FAssetImportContext& Context;
	FGltfData Data;
	std::shared_ptr<FModelAsset> Model;
	std::vector<std::shared_ptr<const FBytes>> OwnedBuffers;
	std::size_t Total;
	std::vector<std::vector<std::uint32_t>> MeshPrimitives;
};

void FGltfImport::CheckExtensions()
{
	auto* Raw = Data.get();
	Require(!Raw->animations_count && !Raw->skins_count, "animation/skinning is outside the static importer");
	for (std::size_t Index = 0; Index < Raw->extensions_required_count; ++Index)
	{
		Require(std::string_view(Raw->extensions_required[Index]) == "KHR_materials_unlit",
		        "unsupported required extension: " + Name(Raw->extensions_required[Index]));
	}
	for (std::size_t Index = 0; Index < Raw->extensions_used_count; ++Index)
	{
		if (std::string_view(Raw->extensions_used[Index]) != "KHR_materials_unlit")
		{
			Model->Diagnostics.push_back("Optional extension ignored: " + Name(Raw->extensions_used[Index]));
		}
	}
}

void FGltfImport::LoadBuffers()
{
	auto* Raw = Data.get();
	for (std::size_t Index = 0; Index < Raw->buffers_count; ++Index)
	{
		auto& Buffer = Raw->buffers[Index];
		if (Buffer.uri)
		{
			auto Bytes = ReadUri(Context, Buffer.uri);
			Require(Bytes->size() >= Buffer.size && Bytes->size() <= MaxImportBytes - Total,
			        "buffer size mismatch or import budget exceeded");
			Total += Bytes->size();
			Buffer.data = const_cast<std::byte*>(Bytes->data());
			OwnedBuffers.push_back(std::move(Bytes));
		}
		else
		{
			Require(Index == 0 && Raw->bin && Raw->bin_size >= Buffer.size, "missing GLB binary chunk");
			Buffer.data = const_cast<void*>(Raw->bin);
		}
		Buffer.data_free_method = cgltf_data_free_method_none;
	}
}

void FGltfImport::LoadNodes()
{
	auto* Raw = Data.get();
	Model->Nodes.reserve(Raw->nodes_count);
	for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
	{
		const auto& Source = Raw->nodes[Index];
		FModelNode Node;
		Node.Name = Name(Source.name);
		cgltf_node_transform_local(&Source, Node.Local.Values.data());
		for (std::size_t Child = 0; Child < Source.children_count; ++Child)
		{
			Node.Children.push_back(static_cast<std::uint32_t>(Source.children[Child] - Raw->nodes));
		}
		Model->Nodes.push_back(std::move(Node));
	}
	// Bound depth before the vendor validator walks every parent chain.
	ValidateNodeHierarchy(Model->Nodes);
	Require(cgltf_validate(Raw) == cgltf_result_success, "buffer/accessor validation failed");
}

void FGltfImport::LoadImages()
{
	auto* Raw = Data.get();
	for (std::size_t Index = 0; Index < Raw->images_count; ++Index)
	{
		Context.Cancellation.Check();
		const auto& Image = Raw->images[Index];
		std::shared_ptr<const FBytes> External;
		std::span<const std::byte> Bytes;
		if (Image.uri)
		{
			External = ReadUri(Context, Image.uri);
			Bytes = *External;
		}
		else
		{
			Require(Image.buffer_view != nullptr, "image has no URI or buffer view");
			Bytes = {reinterpret_cast<const std::byte*>(cgltf_buffer_view_data(Image.buffer_view)),
			         Image.buffer_view->size};
		}
		auto Pixels = DecodeImage(Bytes);
		Require(Pixels.Rgba.size() <= MaxImportBytes - Total, "decoded image budget exceeded");
		Total += Pixels.Rgba.size();
		Model->Images.push_back({Name(Image.name), Pixels.Width, Pixels.Height, std::move(Pixels.Rgba)});
	}
}

void FGltfImport::LoadMeshes()
{
	HYP_PERF_SCOPE_C(Assets, ConvertGltfMeshes);
	auto* Raw = Data.get();

	for (std::size_t MeshIndex = 0; MeshIndex < Raw->meshes_count; ++MeshIndex)
	{
		const auto& Mesh = Raw->meshes[MeshIndex];
		for (std::size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh.primitives_count; ++PrimitiveIndex)
		{
			Context.Cancellation.Check();
			auto Primitive = ConvertPrimitive(Mesh.primitives[PrimitiveIndex], *Raw, *Model, Mesh.name, Total);
			MeshPrimitives[MeshIndex].push_back(static_cast<std::uint32_t>(Model->Primitives.size()));
			Model->Primitives.push_back(std::move(Primitive));
		}
	}
	for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
	{
		const auto& Source = Raw->nodes[Index];
		if (Source.mesh)
		{
			Model->Nodes[Index].Primitives = MeshPrimitives[Source.mesh - Raw->meshes];
		}
	}
}

void FGltfImport::SelectRoots()
{
	auto* Raw = Data.get();
	const auto* Scene = Raw->scene ? Raw->scene : Raw->scenes_count ? &Raw->scenes[0] : nullptr;
	if (Scene)
	{
		for (std::size_t Index = 0; Index < Scene->nodes_count; ++Index)
		{
			Model->Roots.push_back(static_cast<std::uint32_t>(Scene->nodes[Index] - Raw->nodes));
		}
	}
	else
	{
		for (std::size_t Index = 0; Index < Raw->nodes_count; ++Index)
		{
			if (!Raw->nodes[Index].parent)
			{
				Model->Roots.push_back(static_cast<std::uint32_t>(Index));
			}
		}
		if (Raw->nodes_count == 0)
		{
			for (const auto& Primitives : MeshPrimitives)
			{
				Model->Roots.push_back(static_cast<std::uint32_t>(Model->Nodes.size()));
				Model->Nodes.push_back({"mesh", Identity(), Primitives, {}});
			}
		}
	}
}

std::shared_ptr<FModelAsset> FGltfImport::Run()
{
	const auto PathName = Context.Path.filename().u8string();
	Model->Name.assign(reinterpret_cast<const char*>(PathName.data()), PathName.size());
	CheckExtensions();
	LoadBuffers();
	ValidateAccessorRanges(*Data);
	LoadNodes();
	LoadImages();
	LoadMaterials(*Data, *Model);
	LoadMeshes();
	SelectRoots();
	ValidateModel(*Model);
	return Model;
}

std::shared_ptr<void> Import(FAssetImportContext& InContext)
{
	HYP_PERF_SCOPE_C(Assets, ImportGltf);
	return FGltfImport(InContext).Run();
}

} // namespace

FModelPrimitive Private::ConvertGltfPrimitive(FAssetImportContext& InContext, std::size_t InMesh,
                                              std::size_t InPrimitive)
{
	return FGltfImport(InContext).Primitive(InMesh, InPrimitive);
}

void RegisterGltfImporter(FAssetImportService& InImports)
{
	InImports.Register({"hyperion.gltf", 1, &RecordType<FModelAsset>(), {".gltf", ".glb"}, Import});
	InImports.Register({"hyperion.native-model-upgrade",
	                    1,
	                    &RecordType<FModelAsset>(),
	                    {".hasset"},
	                    [](FAssetImportContext& InContext)
	                    {
		                    return ReadRecord(RecordType<FModelAsset>(), DecodeAsset(InContext.Bytes).Object);
	                    }});
}
} // namespace Hyperion
