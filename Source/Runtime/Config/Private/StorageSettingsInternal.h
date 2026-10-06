#pragma once
#include "Hyperion/Config/StorageSettings.h"
#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
struct FStorageSettingsRecord
{
	std::uint64_t Revision = 1;
	FStorageRoots Roots;
	std::string PreviousApplication;
};

template<> const FRecordDescriptor& RecordType<FStorageSettingsRecord>();

struct FStorageSettings::FImpl
{
	FStorageLaunchOptions Options;
	FApplicationPaths Active;
	std::filesystem::path Locator;
	FStorageSettingsRecord Saved;
	std::vector<std::filesystem::path> ProtectedDirectories;
	std::function<std::vector<std::filesystem::path>()> ProtectedDirectoryQuery;
	std::thread::id Owner = std::this_thread::get_id();
	FLocalFileSystem Files;
	void RequireOwner() const;
	FApplicationPaths Resolve(const FStorageRoots& InRoots, bool bInOverrides = true) const;
	void Validate(const FApplicationPaths& InPaths);
	FStorageSettingsRecord Read();
	void Write(const FStorageSettingsRecord& InRecord);
	void CopyMissing(const std::filesystem::path& InSource, const std::filesystem::path& InTarget);
	void Relocate();
};
} // namespace Hyperion
