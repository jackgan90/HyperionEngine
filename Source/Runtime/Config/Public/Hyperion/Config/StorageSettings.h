#pragma once
#include "Hyperion/Core/Errors/ErrorCode.h"
#include "Hyperion/IO/ApplicationPaths.h"
#include "Hyperion/Reflection/RecordValue.h"
#include <functional>
#include <thread>

namespace Hyperion
{
struct FStorageRoots
{
	std::string UserDataRoot;
	std::string CacheRoot;
	bool operator==(const FStorageRoots&) const = default;
};

struct FStorageSettingsState
{
	std::uint64_t Revision = 1;
	std::string ApplicationId;
	std::string Profile;
	std::string SettingsFile;
	FStorageRoots Saved;
	FStorageRoots Active;
	FStorageRoots Next;
	std::string ConfigDirectory;
	std::string StateDirectory;
	std::string LogDirectory;
	std::string CaptureDirectory;
	bool bUserDataOverride = false;
	bool bCacheOverride = false;
	bool bRestartRequired = false;
};

struct FStorageSettingsEdit
{
	std::uint64_t Revision{};
	FStorageRoots Roots;
};

struct FStorageSettingsQuery
{
};

namespace StorageErrors
{
inline constexpr FErrorCodeId StaleRevision{"stale_revision"};
inline constexpr FErrorCodeId InvalidArguments{"invalid_arguments"};
inline constexpr FErrorCodeId SaveFailed{"save_failed"};
} // namespace StorageErrors

class FStorageSettingsError : public FCodedError
{
public:
	FStorageSettingsError(FErrorCode InCode, std::string InMessage)
	    : FCodedError(std::move(InCode), std::move(InMessage))
	{
	}
};

// Main-thread service. Active paths are immutable; edits only change the persisted next-launch selection.
class FStorageSettings
{
public:
	explicit FStorageSettings(FStorageLaunchOptions InOptions);
	~FStorageSettings();
	FStorageSettings(const FStorageSettings&) = delete;
	FStorageSettings& operator=(const FStorageSettings&) = delete;
	FStorageSettingsState Get() const;
	FStorageSettingsState Refresh();
	FStorageSettingsState Set(const FStorageSettingsEdit& InEdit);
	const FApplicationPaths& Paths() const;
	const FStorageLaunchOptions& LaunchOptions() const;
	void Prepare();
	// Copy configuration/state once on first use; never overwrites targets or removes sources.
	void ImportLegacyFiles(std::span<const std::pair<std::filesystem::path, std::filesystem::path>> InFiles);
	void ProtectDirectory(const std::filesystem::path& InDirectory);
	// Main-only scoped composition hook for mutable domain roots. Clear before its owner is destroyed.
	void SetProtectedDirectoryQuery(std::function<std::vector<std::filesystem::path>()> InQuery);

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

template<> const FRecordDescriptor& RecordType<FStorageRoots>();
template<> const FRecordDescriptor& RecordType<FStorageSettingsState>();
template<> const FRecordDescriptor& RecordType<FStorageSettingsEdit>();
template<> const FRecordDescriptor& RecordType<FStorageSettingsQuery>();
} // namespace Hyperion
