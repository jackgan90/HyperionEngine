#pragma once
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/Assets/Assets.h"
#include <cgltf.h>

namespace Hyperion::Private
{
inline constexpr std::size_t MaxImportBytes = 512u * 1024u * 1024u;
void Require(bool bInValue, const std::string& InMessage);
std::string Name(const char* InName);
std::shared_ptr<const FBytes> ReadUri(FAssetImportContext& InContext, const char* InUri);
FModelPrimitive ConvertGltfPrimitive(FAssetImportContext& InContext, std::size_t InMesh, std::size_t InPrimitive);
void ValidateAccessorRanges(const cgltf_data& InData);
std::vector<float> Attribute(const cgltf_primitive& InPrimitive, cgltf_attribute_type InType, int InSet,
                             unsigned InComponents, std::size_t InCount);
std::vector<std::uint32_t> Indices(const cgltf_accessor* InAccessor, std::size_t InVertices);
void LoadMaterials(const cgltf_data& InData, FModelAsset& OutModel);
FModelPrimitive ConvertPrimitive(const cgltf_primitive& InSource, const cgltf_data& InData, const FModelAsset& InModel,
                                 const char* InName, std::size_t& InTotal);
} // namespace Hyperion::Private
