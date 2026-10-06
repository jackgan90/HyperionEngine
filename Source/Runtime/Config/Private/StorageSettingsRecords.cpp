#include "StorageSettingsInternal.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FStorageRoots>()
{
	static const auto Type = MakeRecord<FStorageRoots>(
	    "hyperion.storage.roots",
	    {Member("userDataRoot", &FStorageRoots::UserDataRoot,
	            {.Description = "Absolute local user-data root; empty restores the default."}),
	     Member("cacheRoot", &FStorageRoots::CacheRoot,
	            {.Description = "Absolute disposable-cache root; empty uses Cache below the effective user root."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FStorageSettingsRecord>()
{
	static const auto Type = MakeRecord<FStorageSettingsRecord>(
	    "hyperion.storage.settings",
	    {Member("revision", &FStorageSettingsRecord::Revision), Member("roots", &FStorageSettingsRecord::Roots),
	     Member("previousApplication", &FStorageSettingsRecord::PreviousApplication)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FStorageSettingsState>()
{
	static const auto Type = MakeRecord<FStorageSettingsState>(
	    "hyperion.storage.state",
	    {Member("revision", &FStorageSettingsState::Revision),
	     Member("applicationId", &FStorageSettingsState::ApplicationId),
	     Member("profile", &FStorageSettingsState::Profile),
	     Member("settingsFile", &FStorageSettingsState::SettingsFile), Member("saved", &FStorageSettingsState::Saved),
	     Member("active", &FStorageSettingsState::Active), Member("next", &FStorageSettingsState::Next),
	     Member("configDirectory", &FStorageSettingsState::ConfigDirectory),
	     Member("stateDirectory", &FStorageSettingsState::StateDirectory),
	     Member("logDirectory", &FStorageSettingsState::LogDirectory),
	     Member("captureDirectory", &FStorageSettingsState::CaptureDirectory),
	     Member("userDataOverride", &FStorageSettingsState::bUserDataOverride),
	     Member("cacheOverride", &FStorageSettingsState::bCacheOverride),
	     Member("restartRequired", &FStorageSettingsState::bRestartRequired)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FStorageSettingsEdit>()
{
	static const auto Type = MakeRecord<FStorageSettingsEdit>(
	    "hyperion.storage.edit", {Member("revision", &FStorageSettingsEdit::Revision, {.bRequired = true}),
	                              Member("roots", &FStorageSettingsEdit::Roots, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FStorageSettingsQuery>()
{
	static const auto Type = MakeRecord<FStorageSettingsQuery>("hyperion.storage.query", {});
	return Type;
}
} // namespace Hyperion
