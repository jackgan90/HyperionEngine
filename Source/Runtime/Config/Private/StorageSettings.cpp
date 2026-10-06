#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Reflection/Json.h"
#include "StorageSettingsInternal.h"
#include <limits>

namespace Hyperion
{
namespace
{
std::string NormalizeRoot(const std::string& InValue)
{
	if (InValue.empty())
	{
		return {};
	}
	const auto Path = PathFromUtf8(InValue);
	if (!Path.is_absolute() || IsPackagePath(Path))
	{
		throw std::invalid_argument("Saved storage roots must be absolute local paths");
	}
	return PathToUtf8(NormalizeFilePath(Path));
}

FStorageRoots RootValues(const FApplicationPaths& InPaths)
{
	return {PathToUtf8(InPaths.UserDataRoot), PathToUtf8(InPaths.CacheRoot)};
}

bool SameRoot(const std::string& InFirst, const std::string& InSecond)
{
	if (InFirst.empty() || InSecond.empty())
	{
		return InFirst.empty() && InSecond.empty();
	}
	return PathCaseKey(PathFromUtf8(InFirst)) == PathCaseKey(PathFromUtf8(InSecond));
}

bool SameRoots(const FStorageRoots& InFirst, const FStorageRoots& InSecond)
{
	return SameRoot(InFirst.UserDataRoot, InSecond.UserDataRoot) && SameRoot(InFirst.CacheRoot, InSecond.CacheRoot);
}
} // namespace

void FStorageSettings::FImpl::RequireOwner() const
{
	if (Owner != std::this_thread::get_id())
	{
		throw std::logic_error("Storage settings must be accessed from the owning Main thread");
	}
}

FApplicationPaths FStorageSettings::FImpl::Resolve(const FStorageRoots& InRoots, bool bInOverrides) const
{
	auto User = PathFromUtf8(InRoots.UserDataRoot);
	if (bInOverrides && !Options.UserDataRoot.empty())
	{
		User = Options.UserDataRoot;
	}
	if (User.empty())
	{
		User = LocalUserDirectory() / "Hyperion";
	}
	auto Cache = PathFromUtf8(InRoots.CacheRoot);
	if (bInOverrides && !Options.CacheRoot.empty())
	{
		Cache = Options.CacheRoot;
	}
	if (Cache.empty())
	{
		Cache = User / "Cache";
	}
	return MakeApplicationPaths(Options, User, Cache);
}

FStorageSettingsRecord FStorageSettings::FImpl::Read()
{
	RequireUnlinkedPath(Locator);
	if (!Files.Exists(Locator))
	{
		return {};
	}
	const auto Bytes = Files.Read(Locator, 64 * 1024);
	const auto Record = ReadValue<FStorageSettingsRecord>(
	    ParseJson(std::string_view(reinterpret_cast<const char*>(Bytes.data()), Bytes.size())));
	if (!Record.Revision || Record.Revision == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::runtime_error("Invalid storage settings revision");
	}
	(void)NormalizeRoot(Record.Roots.UserDataRoot);
	(void)NormalizeRoot(Record.Roots.CacheRoot);
	(void)NormalizeRoot(Record.PreviousApplication);
	return Record;
}

void FStorageSettings::FImpl::Write(const FStorageSettingsRecord& InRecord)
{
	RequireUnlinkedPath(Locator);
	const auto Text = WriteJson(WriteValue(InRecord));
	Files.WriteAtomic(Locator, std::as_bytes(std::span(Text)));
}

void FStorageSettings::FImpl::Validate(const FApplicationPaths& InPaths)
{
	if (PathCaseKey(InPaths.Application) != PathCaseKey(Active.Application) &&
	    (PathContains(InPaths.Application, Active.Application) ||
	     PathContains(Active.Application, InPaths.Application)))
	{
		throw std::invalid_argument("Relocation source and destination application directories overlap");
	}
	auto ProtectedRoots = ProtectedDirectories;
	if (ProtectedDirectoryQuery)
	{
		const auto Current = ProtectedDirectoryQuery();
		ProtectedRoots.insert(ProtectedRoots.end(), Current.begin(), Current.end());
	}
	for (const auto& Directory : {InPaths.Application, InPaths.CacheRoot})
	{
		RequireUnlinkedPath(Directory);
		for (const auto& Protected : ProtectedRoots)
		{
			if (PathContains(Directory, Protected) || PathContains(Protected, Directory))
			{
				throw std::invalid_argument("Storage directory overlaps protected content: " + PathToUtf8(Protected));
			}
		}
		std::filesystem::create_directories(Directory);
		const auto Probe = Directory / (".hyperion-probe-" + std::to_string(ClockNanoseconds()));
		Files.WriteAtomic(Probe, {});
		Files.Remove(Probe);
	}
	if (PathContains(InPaths.CacheRoot, Locator))
	{
		throw std::invalid_argument("Bootstrap settings file must be outside the disposable cache root");
	}
}

FStorageSettings::FStorageSettings(FStorageLaunchOptions InOptions) : Impl(std::make_unique<FImpl>())
{
	ValidateStorageIdentity(InOptions.ApplicationId);
	ValidateStorageIdentity(InOptions.Profile);
	Impl->Options = std::move(InOptions);
	const auto& Options = Impl->Options;
	Impl->Locator = Options.SettingsFile;
	if (Impl->Locator.empty())
	{
		const auto Base = Options.UserDataRoot.empty() ? LocalUserDirectory() / "Hyperion" : Options.UserDataRoot;
		Impl->Locator = Base / "Storage" / Options.ApplicationId / (Options.Profile + ".json");
	}
	Impl->Locator = NormalizeFilePath(Impl->Locator);
	Impl->Saved = Impl->Read();
	Impl->Active = Impl->Resolve(Impl->Saved.Roots);
	RequireUnlinkedPath(Impl->Active.Application);
	RequireUnlinkedPath(Impl->Active.CacheRoot);
	if (PathContains(Impl->Active.CacheRoot, Impl->Locator))
	{
		throw std::invalid_argument("Bootstrap settings file must be outside the disposable cache root");
	}
}

FStorageSettings::~FStorageSettings() = default;

FStorageSettingsState FStorageSettings::Get() const
{
	Impl->RequireOwner();
	FStorageSettingsState Result;
	Result.Revision = Impl->Saved.Revision;
	Result.ApplicationId = Impl->Options.ApplicationId;
	Result.Profile = Impl->Options.Profile;
	Result.SettingsFile = PathToUtf8(Impl->Locator);
	Result.Saved = Impl->Saved.Roots;
	Result.Active = RootValues(Impl->Active);
	Result.Next = RootValues(Impl->Resolve(Impl->Saved.Roots));
	Result.ConfigDirectory = PathToUtf8(Impl->Active.Config);
	Result.StateDirectory = PathToUtf8(Impl->Active.State);
	Result.LogDirectory = PathToUtf8(Impl->Active.Logs);
	Result.CaptureDirectory = PathToUtf8(Impl->Active.Captures);
	Result.bUserDataOverride = !Impl->Options.UserDataRoot.empty();
	Result.bCacheOverride = !Impl->Options.CacheRoot.empty();
	Result.bRestartRequired = !SameRoots(Result.Active, Result.Next);
	return Result;
}

FStorageSettingsState FStorageSettings::Set(const FStorageSettingsEdit& InEdit)
{
	Impl->RequireOwner();
	if (InEdit.Revision != Impl->Saved.Revision)
	{
		throw FStorageSettingsError(StorageErrors::StaleRevision, "Storage settings changed; read before editing");
	}
	try
	{
		auto Candidate = Impl->Saved;
		Candidate.Roots = {NormalizeRoot(InEdit.Roots.UserDataRoot), NormalizeRoot(InEdit.Roots.CacheRoot)};
		Impl->Validate(Impl->Resolve(Candidate.Roots, false));
		Impl->Validate(Impl->Resolve(Candidate.Roots));
		auto Lease = Impl->Files.AcquireWriteLease(Impl->Locator);
		const auto Current = Impl->Read();
		if (Current.Revision != Impl->Saved.Revision || !SameRoots(Current.Roots, Impl->Saved.Roots))
		{
			throw FStorageSettingsError(StorageErrors::StaleRevision,
			                            "Another process changed storage settings; refresh before editing");
		}
		if (SameRoots(Candidate.Roots, Impl->Saved.Roots))
		{
			return Get();
		}
		Candidate.PreviousApplication = PathToUtf8(Impl->Active.Application);
		++Candidate.Revision;
		Impl->Write(Candidate);
		Impl->Saved = std::move(Candidate);
	}
	catch (const FStorageSettingsError&)
	{
		throw;
	}
	catch (const std::invalid_argument& Error)
	{
		throw FStorageSettingsError(StorageErrors::InvalidArguments, Error.what());
	}
	catch (const std::exception& Error)
	{
		throw FStorageSettingsError(StorageErrors::SaveFailed,
		                            "Storage settings were not saved: " + std::string(Error.what()));
	}
	Log(ELogLevel::Info, "Storage settings saved; path='" + PathToUtf8(Impl->Locator) + "'; active roots retained");
	return Get();
}

const FApplicationPaths& FStorageSettings::Paths() const
{
	Impl->RequireOwner();
	return Impl->Active;
}

FStorageSettingsState FStorageSettings::Refresh()
{
	Impl->RequireOwner();
	const auto Candidate = Impl->Read();
	(void)Impl->Resolve(Candidate.Roots);
	Impl->Saved = Candidate;
	return Get();
}

const FStorageLaunchOptions& FStorageSettings::LaunchOptions() const
{
	Impl->RequireOwner();
	return Impl->Options;
}

void FStorageSettings::ProtectDirectory(const std::filesystem::path& InDirectory)
{
	Impl->RequireOwner();
	const auto Directory = NormalizeFilePath(InDirectory);
	for (const auto& Active : {Impl->Active.Application, Impl->Active.CacheRoot})
	{
		if (PathContains(Active, Directory) || PathContains(Directory, Active))
		{
			throw std::invalid_argument("Active storage overlaps protected content: " + PathToUtf8(Directory));
		}
	}
	Impl->ProtectedDirectories.push_back(Directory);
}

void FStorageSettings::SetProtectedDirectoryQuery(std::function<std::vector<std::filesystem::path>()> InQuery)
{
	Impl->RequireOwner();
	Impl->ProtectedDirectoryQuery = std::move(InQuery);
}
} // namespace Hyperion
