#include "Hyperion/Content/ContentRootService.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
void IndexContent(FTaskSystem& InTasks, FMountedFileSystem& InFiles, FAssetService& InAssets)
{
	for (const auto& Mount : InFiles.GetMounts())
	{
		const auto Discovery = DispatchAsync<FAssetDiscovery>(InTasks, {EDomain::Io},
		                                                      [&InFiles, Root = Mount.Root]
		                                                      {
			                                                      return DiscoverAssets(InFiles, Root);
		                                                      })
		                           .Get(InTasks);
		InAssets.AddAssetIndex(BuildAssetIndex(Discovery->Entries), Mount.Root);
		for (const auto& [Path, Error] : Discovery->Errors)
		{
			Log(ELogLevel::Warning, "Asset unavailable: " + PathToUtf8(Path) + ": " + Error);
		}
	}
}
} // namespace

std::shared_ptr<FMountedFileSystem> CreateContentFileSystem(const std::filesystem::path& InEngineDirectory,
                                                            bool bInAuthoring)
{
	if (InEngineDirectory.empty() || !std::filesystem::is_directory(InEngineDirectory))
	{
		throw std::invalid_argument("Engine content must be an existing directory");
	}
	return std::make_shared<FMountedFileSystem>(
	    std::vector<FContentMount>{{"/Engine", InEngineDirectory, !bInAuthoring}});
}

FContentRootService::FContentRootService(FTaskSystem& InTasks, FMountedFileSystem& InFiles, FAssetService& InAssets)
    : Tasks(InTasks), Files(InFiles), Assets(InAssets)
{
	Tasks.Require({EDomain::Main});
	IndexContent(Tasks, Files, Assets);
}

void FContentRootService::RequireReady() const
{
	Tasks.Require({EDomain::Main});
	if (bFailed)
	{
		throw FContentRootError("content_failed",
		                        "Content transition failed during consumer retirement; restart the host");
	}
	if (bChanging)
	{
		throw FContentRootError("busy", "Content root transition is already running");
	}
}

FContentRootInfo FContentRootService::Info() const
{
	Tasks.Require({EDomain::Main});
	if (bFailed)
	{
		throw FContentRootError("content_failed", "Content transition failed; restart the host");
	}
	FContentRootInfo Result{{}, Generation, false};
	for (const auto& Mount : Files.GetMounts())
	{
		if (Mount.Root == "/Game")
		{
			Result.Directory = PathToUtf8(Mount.Directory);
			Result.bReadOnly = Mount.bReadOnly;
		}
	}
	return Result;
}

std::filesystem::path FContentRootService::Directory() const
{
	return PathFromUtf8(Info().Directory);
}

FContentRootCandidate FContentRootService::Prepare(const std::filesystem::path& InDirectory, bool bInReadOnly)
{
	RequireReady();
	FContentRootCandidate Result;
	Result.Owner = this;
	Result.Generation = Generation;
	Result.bReadOnly = !InDirectory.empty() && bInReadOnly;
	if (!InDirectory.empty())
	{
		if (!std::filesystem::is_directory(InDirectory))
		{
			throw FContentRootError("invalid_root", "Asset root must be an existing directory");
		}
		Result.Directory = std::filesystem::canonical(InDirectory);
	}
	auto Mounts = Files.GetMounts();
	std::erase_if(Mounts,
	              [](const auto& InMount)
	              {
		              return InMount.Root == "/Game";
	              });
	if (!Result.Directory.empty())
	{
		Mounts.push_back({"/Game", Result.Directory, Result.bReadOnly});
	}
	Result.Files = std::make_shared<FMountedFileSystem>(std::move(Mounts));
	if (!Result.Directory.empty())
	{
		(void)Result.Files->ListDirectory("/Game");
	}
	Result.IO = std::make_unique<FIOService>(Tasks, Result.Files);
	Result.Assets = std::make_unique<FAssetService>(*Result.IO);
	IndexContent(Tasks, *Result.Files, *Result.Assets);
	return Result;
}

void FContentRootService::Commit(FContentRootCandidate InCandidate, bool bInDiscard)
{
	RequireGeneration(InCandidate.Generation);
	if (InCandidate.Owner != this || !InCandidate.Files || !InCandidate.Assets)
	{
		throw FContentRootError("invalid_root", "Invalid content root candidate");
	}
	const auto Current = Info();
	std::error_code Error;
	const bool bSameDirectory =
	    PathFromUtf8(Current.Directory) == InCandidate.Directory ||
	    (!Current.Directory.empty() && !InCandidate.Directory.empty() &&
	     std::filesystem::equivalent(PathFromUtf8(Current.Directory), InCandidate.Directory, Error));
	if (bSameDirectory && Current.bReadOnly == InCandidate.bReadOnly)
	{
		return;
	}
	for (const auto* Participant : Participants)
	{
		const auto State = Participant->ContentRootState();
		if (State.bBusy)
		{
			throw FContentRootError("busy", "Wait for pending content edits and saves before changing the root");
		}
		if (State.bDirty && !bInDiscard)
		{
			throw FContentRootError("dirty_document",
			                        "Save modified documents or explicitly discard before changing the root");
		}
	}
	bChanging = true;
	try
	{
		for (auto* Participant : Participants)
		{
			Participant->ReleaseContentRoot();
		}
		Assets.ResetContent(*InCandidate.Assets);
		Files.ReplaceExclusive(*InCandidate.Files);
		++Generation;
		for (auto* Participant : Participants)
		{
			Participant->ContentRootChanged();
		}
		StartupError.clear();
		bChanging = false;
	}
	catch (...)
	{
		bChanging = false;
		bFailed = true;
		throw FContentRootError("content_failed",
		                        "Content consumer retirement or reinitialization failed; restart the host");
	}
}

void FContentRootService::Change(const std::filesystem::path& InDirectory, bool bInReadOnly, bool bInDiscard)
{
	Commit(Prepare(InDirectory, bInReadOnly), bInDiscard);
}

void FContentRootService::RequireGeneration(std::uint64_t InGeneration) const
{
	RequireReady();
	if (InGeneration != Generation)
	{
		throw FContentRootError("stale_revision",
		                        "Content root changed; query content.root.get and retry with its generation");
	}
}

FContentRootInfo FContentRootService::Set(const FContentRootRequest& InRequest)
{
	RequireGeneration(InRequest.Generation);
	if (InRequest.Directory.empty())
	{
		throw FContentRootError("invalid_root", "Specify a directory; use content.root.clear to unmount Game");
	}
	Change(PathFromUtf8(InRequest.Directory), InRequest.bReadOnly, InRequest.bDiscard);
	return Info();
}

FContentRootInfo FContentRootService::Clear(const FContentRootClearRequest& InRequest)
{
	RequireGeneration(InRequest.Generation);
	Change({}, false, InRequest.bDiscard);
	return Info();
}

void FContentRootService::RegisterParticipant(IContentRootParticipant& InParticipant)
{
	RequireReady();
	if (std::find(Participants.begin(), Participants.end(), &InParticipant) != Participants.end())
	{
		throw std::invalid_argument("Content participant is already registered");
	}
	Participants.push_back(&InParticipant);
}

void FContentRootService::UnregisterParticipant(IContentRootParticipant& InParticipant)
{
	Tasks.Require({EDomain::Main});
	if (bChanging)
	{
		throw std::logic_error("Cannot remove a participant during content transition");
	}
	std::erase(Participants, &InParticipant);
}
} // namespace Hyperion
