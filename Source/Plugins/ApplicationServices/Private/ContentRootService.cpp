#include "Hyperion/ApplicationServices/ContentRootService.h"
#include "Hyperion/Scene/SceneManifest.h"
#include <algorithm>

namespace Hyperion
{
FContentRootService::FContentRootService(FTaskSystem& InTasks, FMountedFileSystem& InFiles, FAssetService& InAssets)
    : Tasks(InTasks), Files(InFiles), Assets(InAssets)
{
}

std::filesystem::path FContentRootService::Directory() const
{
	for (const auto& Mount : Files.GetMounts())
	{
		if (Mount.Root == "/Game")
		{
			return Mount.Directory;
		}
	}
	return {};
}

FContentRootCandidate FContentRootService::Prepare(const std::filesystem::path& InDirectory)
{
	Tasks.Require({EDomain::Main});
	if (InDirectory.empty() || !std::filesystem::is_directory(InDirectory))
	{
		throw std::invalid_argument("Asset root must be an existing directory");
	}
	FContentRootCandidate Result;
	Result.Directory = std::filesystem::canonical(InDirectory);
	Result.Generation = Generation;
	auto Mounts = Files.GetMounts();
	std::erase_if(Mounts,
	              [](const auto& InMount)
	              {
		              return InMount.Root == "/Game";
	              });
	Mounts.push_back({"/Game", Result.Directory, false});
	Result.Files = std::make_shared<FMountedFileSystem>(std::move(Mounts));
	(void)Result.Files->ListDirectory("/Game");
	Result.IO = std::make_unique<FIOService>(Tasks, Result.Files);
	Result.Assets = std::make_unique<FAssetService>(*Result.IO);
	RegisterSceneAssetTypes(Result.Assets->Types());
	for (const auto& Mount : Result.Files->GetMounts())
	{
		if (Result.Files->Exists(Mount.Root / "Catalog.hasset"))
		{
			const auto Catalog = Result.Assets->LoadAsync<FAssetCatalog>(Mount.Root / "Catalog.hasset").Get(Tasks);
			Result.Assets->AddCatalog(*Catalog, Mount.Root);
		}
	}
	return Result;
}

void FContentRootService::Commit(FContentRootCandidate InCandidate)
{
	Tasks.Require({EDomain::Main});
	if (InCandidate.Generation != Generation || !InCandidate.Files || !InCandidate.Assets)
	{
		throw std::invalid_argument("Stale or invalid content root candidate");
	}
	Assets.ResetContent(*InCandidate.Assets);
	Files.ReplaceExclusive(*InCandidate.Files);
	++Generation;
}
} // namespace Hyperion
