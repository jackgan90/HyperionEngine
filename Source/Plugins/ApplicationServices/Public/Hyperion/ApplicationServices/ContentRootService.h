#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/MountedFileSystem.h"

namespace Hyperion
{
struct FContentRootCandidate
{
	FContentRootCandidate() = default;
	FContentRootCandidate(FContentRootCandidate&&) noexcept = default;
	// Memberwise assignment would destroy IO before the asset service borrowing it.
	FContentRootCandidate& operator=(FContentRootCandidate&&) = delete;

	std::filesystem::path Directory;
	std::uint64_t Generation{};
	std::shared_ptr<FMountedFileSystem> Files;
	std::unique_ptr<FIOService> IO;
	std::unique_ptr<FAssetService> Assets;
};

// Main-only coordination owned by the assets plugin. Consumers must retire before Commit.
class FContentRootService
{
public:
	FContentRootService(FTaskSystem& InTasks, FMountedFileSystem& InFiles, FAssetService& InAssets);
	FContentRootCandidate Prepare(const std::filesystem::path& InDirectory);
	void Commit(FContentRootCandidate InCandidate);
	std::filesystem::path Directory() const;
	std::string StartupError;

private:
	FTaskSystem& Tasks;
	FMountedFileSystem& Files;
	FAssetService& Assets;
	std::uint64_t Generation{};
};
} // namespace Hyperion
