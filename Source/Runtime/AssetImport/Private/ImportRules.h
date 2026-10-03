#pragma once
#include <filesystem>
#include <string_view>

namespace Hyperion
{
class IFileSystem;

enum class EImportSourceIdentityError
{
	None,
	Incomplete,
	NonPortable
};

// InOutput is already normalized by the caller's filesystem.
std::filesystem::path NormalizeImportLibrary(IFileSystem& InFiles, const std::filesystem::path& InOutput,
                                             const std::filesystem::path& InLibrary);
bool IsImportAssetOutput(const std::filesystem::path& InOutput);
bool IsSeparateImportOutput(IFileSystem& InFiles, const std::filesystem::path& InSource,
                            const std::filesystem::path& InOutput);
EImportSourceIdentityError CheckImportSourceIdentity(bool bInHasSourceRoot, std::string_view InSourceId);
} // namespace Hyperion
