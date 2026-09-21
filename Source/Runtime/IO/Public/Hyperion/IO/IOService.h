#pragma once
#include "Hyperion/Tasks/AsyncResult.h"
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string_view>

namespace Hyperion
{
class FFileNotFound : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

using FBytes = std::vector<std::byte>;

struct FFileContents
{
	std::filesystem::path Path;
	FBytes Bytes;
};

struct FDirectoryEntry
{
	std::filesystem::path Path;
	bool bDirectory{};
	std::string Error;
};

class IFileWriteLease
{
public:
	virtual ~IFileWriteLease() = 0;
};

inline IFileWriteLease::~IFileWriteLease() = default;

// Synchronous storage primitive. Runtime callers schedule it through FIOService.
class IFileSystem
{
public:
	virtual ~IFileSystem() = default;
	virtual std::shared_ptr<IFileWriteLease> AcquireWriteLease(const std::filesystem::path& InPath);
	virtual FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) = 0;
	virtual std::vector<FFileContents> ReadTree(const std::filesystem::path& InDirectory,
	                                            std::span<const std::string_view> InExtensions, std::size_t InLimit);
	virtual void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) = 0;
	virtual std::filesystem::path Normalize(const std::filesystem::path& InPath) const;
	virtual bool Exists(const std::filesystem::path& InPath);
	virtual std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InDirectory);
	virtual std::vector<std::filesystem::path> Enumerate(const std::filesystem::path& InDirectory, bool bInRecursive);
};

class FLocalFileSystem final : public IFileSystem
{
public:
	std::filesystem::path Normalize(const std::filesystem::path& InPath) const override;
	std::shared_ptr<IFileWriteLease> AcquireWriteLease(const std::filesystem::path& InPath) override;
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override;
	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override;
	bool Exists(const std::filesystem::path& InPath) override;
	std::vector<FDirectoryEntry> ListDirectory(const std::filesystem::path& InDirectory) override;
	std::vector<std::filesystem::path> Enumerate(const std::filesystem::path& InDirectory, bool bInRecursive) override;
};

class FMemoryFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override;
	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override;
	bool Exists(const std::filesystem::path& InPath) override;
	std::vector<std::filesystem::path> Enumerate(const std::filesystem::path& InDirectory, bool bInRecursive) override;

private:
	std::mutex Mutex;
	std::map<std::filesystem::path, FBytes> Files;
};

struct FIOStats
{
	std::atomic_uint64_t Reads{};
	std::atomic_uint64_t Writes{};
	std::atomic_uint64_t ReadBytes{};
	std::atomic_uint64_t WrittenBytes{};
};

class FIOService
{
public:
	explicit FIOService(FTaskSystem& InTasks,
	                    std::shared_ptr<IFileSystem> InFiles = std::make_shared<FLocalFileSystem>());
	TAsyncResult<FBytes> ReadAsync(std::filesystem::path InPath, FCancellationToken InCancellation = {},
	                               std::size_t InLimit = 512u * 1024u * 1024u);
	TAsyncResult<std::optional<FBytes>> TryReadAsync(std::filesystem::path InPath,
	                                                 FCancellationToken InCancellation = {},
	                                                 std::size_t InLimit = 512u * 1024u * 1024u);
	TAsyncResult<bool> WriteAsync(std::filesystem::path InPath, FBytes InBytes, FCancellationToken InCancellation = {});

	TAsyncResult<std::shared_ptr<IFileWriteLease>> AcquireWriteLeaseAsync(std::filesystem::path InPath,
	                                                                      FCancellationToken InCancellation = {});

	const FIOStats& Statistics() const
	{
		return *Stats;
	}

	FTaskSystem& TaskSystem() const
	{
		return Tasks;
	}

	const std::shared_ptr<IFileSystem>& FileSystem() const
	{
		return Files;
	}

private:
	FTaskSystem& Tasks;
	std::shared_ptr<IFileSystem> Files;
	std::shared_ptr<FIOStats> Stats = std::make_shared<FIOStats>();
};
} // namespace Hyperion
