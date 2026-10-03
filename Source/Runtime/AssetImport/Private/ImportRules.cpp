#include "ImportRules.h"
#include "AssetImportInternal.h"

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
} // namespace Hyperion
