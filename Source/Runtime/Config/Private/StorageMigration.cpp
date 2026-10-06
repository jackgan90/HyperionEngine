#include "Hyperion/IO/Path.h"
#include "StorageSettingsInternal.h"

namespace Hyperion
{
void FStorageSettings::FImpl::CopyMissing(const std::filesystem::path& InSource, const std::filesystem::path& InTarget)
{
	RequireUnlinkedPath(InSource);
	RequireUnlinkedPath(InTarget);
	if (Files.Exists(InSource) && !Files.Exists(InTarget))
	{
		const auto Bytes = Files.Read(InSource, 16 * 1024 * 1024);
		Files.WriteAtomic(InTarget, Bytes);
	}
}

void FStorageSettings::FImpl::Relocate()
{
	if (Saved.PreviousApplication.empty() || !Options.UserDataRoot.empty())
	{
		return;
	}
	const auto Previous = PathFromUtf8(Saved.PreviousApplication);
	if (PathCaseKey(Previous) != PathCaseKey(Active.Application))
	{
		if (PathContains(Previous, Active.Application) || PathContains(Active.Application, Previous))
		{
			throw std::invalid_argument("Relocation source and destination application directories overlap");
		}
		for (const auto* Kind : {"Config", "State"})
		{
			const auto Source = Previous / Kind;
			RequireUnlinkedPath(Source);
			if (!std::filesystem::is_directory(Source))
			{
				continue;
			}
			for (const auto& Entry : std::filesystem::recursive_directory_iterator(Source))
			{
				RequireUnlinkedPath(Entry.path());
				if (Entry.is_regular_file())
				{
					CopyMissing(Entry.path(), Active.Application / Kind / Entry.path().lexically_relative(Source));
				}
			}
		}
	}
	Saved.PreviousApplication.clear();
	++Saved.Revision;
	Write(Saved);
}

void FStorageSettings::Prepare()
{
	Impl->RequireOwner();
	for (const auto& Directory : {Impl->Active.Config, Impl->Active.State, Impl->Active.Logs})
	{
		RequireUnlinkedPath(Directory);
		std::filesystem::create_directories(Directory);
	}
	if (!Impl->Options.bIsolated && !Impl->Saved.PreviousApplication.empty())
	{
		auto Lease = Impl->Files.AcquireWriteLease(Impl->Locator);
		Impl->Saved = Impl->Read();
		if (PathCaseKey(Impl->Resolve(Impl->Saved.Roots).Application) != PathCaseKey(Impl->Active.Application))
		{
			throw FStorageSettingsError(StorageErrors::StaleRevision,
			                            "Storage changed during startup; retry launching");
		}
		Impl->Relocate();
	}
}

void FStorageSettings::ImportLegacyFiles(
    std::span<const std::pair<std::filesystem::path, std::filesystem::path>> InFiles)
{
	Impl->RequireOwner();
	if (Impl->Options.bIsolated)
	{
		return;
	}
	const auto Marker = Impl->Active.State / "LegacyStorageImported";
	RequireUnlinkedPath(Marker);
	auto Lease = Impl->Files.AcquireWriteLease(Marker);
	if (Impl->Files.Exists(Marker))
	{
		return;
	}
	for (const auto& [Source, Target] : InFiles)
	{
		if (!PathContains(Impl->Active.Config, Target) && !PathContains(Impl->Active.State, Target))
		{
			throw std::invalid_argument("Legacy migration target is outside configuration/state");
		}
		Impl->CopyMissing(Source, Target);
	}
	Impl->Files.WriteAtomic(Marker, {});
}
} // namespace Hyperion
