#include "ImportRules.h"
#include "AssetImportInternal.h"
#include "Hyperion/Scene/Model.h"
#include "Hyperion/Textures/TextureAsset.h"

namespace Hyperion
{
std::filesystem::path NormalizeImportLibrary(IFileSystem& InFiles, const std::filesystem::path& InOutput,
                                             const std::filesystem::path& InLibrary)
{
	return InFiles.Normalize(InLibrary.empty() ? InOutput.parent_path() : InLibrary);
}

bool IsImportAssetOutput(const std::filesystem::path& InOutput)
{
	return ImportExtension(InOutput) == ".hasset";
}

bool IsSeparateImportOutput(IFileSystem& InFiles, const std::filesystem::path& InSource,
                            const std::filesystem::path& InOutput)
{
	return InFiles.Normalize(InSource) != InOutput && IsImportAssetOutput(InOutput);
}

EImportSourceIdentityError CheckImportSourceIdentity(bool bInHasSourceRoot, std::string_view InSourceId)
{
	if (bInHasSourceRoot != !InSourceId.empty())
	{
		return EImportSourceIdentityError::Incomplete;
	}
	if (InSourceId.find_first_of(":\\") != std::string_view::npos || InSourceId.starts_with('/') ||
	    InSourceId.find("..") != std::string_view::npos)
	{
		return EImportSourceIdentityError::NonPortable;
	}
	return EImportSourceIdentityError::None;
}

void ApplyRequestedImportName(FConvertedAsset& InAsset, std::string_view InName, bool bInScene)
{
	if (InName.empty())
	{
		return;
	}
	if (!bInScene && InAsset.Type->CppType == typeid(FModelAsset))
	{
		auto Model = std::make_shared<FModelAsset>(*std::static_pointer_cast<const FModelAsset>(InAsset.Object));
		Model->Name = InName;
		InAsset.Object = std::move(Model);
	}
	else if (InAsset.Type->CppType == typeid(FTextureAsset))
	{
		auto Texture = std::make_shared<FTextureAsset>(*std::static_pointer_cast<const FTextureAsset>(InAsset.Object));
		Texture->Name = InName;
		InAsset.Object = std::move(Texture);
	}
}
} // namespace Hyperion
