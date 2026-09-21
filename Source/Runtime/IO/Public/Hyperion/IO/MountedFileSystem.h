#pragma once
#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
struct FContentMount
{
	std::filesystem::path Root;
	std::filesystem::path Directory;
	bool bReadOnly = true;
};

// Frozen during requests. Exclusive replacement requires all consumers and IO to be quiescent.
class FMountedFileSystem final : public IFileSystem
{
public:
	explicit FMountedFileSystem(std::vector<FContentMount> InMounts);
	void ReplaceExclusive(FMountedFileSystem& InPrepared) noexcept;
	std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InDirectory) override;

	const std::vector<FContentMount>& GetMounts() const
	{
		return Mounts;
	}

	std::filesystem::path Normalize(const std::filesystem::path& InPath) const override;
	std::filesystem::path Resolve(const std::filesystem::path& InPath, bool bInWrite = false) const;
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override;
	std::vector<FFileContents> ReadTree(const std::filesystem::path& InDirectory,
	                                    std::span<const std::string_view> InExtensions, std::size_t InLimit) override;
	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override;
	std::shared_ptr<IFileWriteLease> AcquireWriteLease(const std::filesystem::path& InPath) override;
	bool Exists(const std::filesystem::path& InPath) override;
	std::vector<std::filesystem::path> Enumerate(const std::filesystem::path& InDirectory, bool bInRecursive) override;

private:
	std::vector<FContentMount> Mounts;
	FLocalFileSystem Local;
};

std::shared_ptr<FMountedFileSystem> LoadContentMounts(const std::filesystem::path& InConfiguration,
                                                      bool bInAuthoring = false);
} // namespace Hyperion
