#pragma once
#include "Hyperion/Tasks/AsyncResult.h"
#include <filesystem>
#include <map>
#include <mutex>

namespace Hyperion
{
using FBytes = std::vector<std::byte>;

// Synchronous storage primitive. Runtime callers schedule it through FIOService.
class IFileSystem
{
public:
	virtual ~IFileSystem() = default;
	virtual FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) = 0;
	virtual void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) = 0;
};

class FLocalFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override;
	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override;
};

class FMemoryFileSystem final : public IFileSystem
{
public:
	FBytes Read(const std::filesystem::path& InPath, std::size_t InLimit) override;
	void WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes) override;

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
	TAsyncResult<bool> WriteAsync(std::filesystem::path InPath, FBytes InBytes, FCancellationToken InCancellation = {});

	const FIOStats& Statistics() const
	{
		return *Stats;
	}

	FTaskSystem& TaskSystem() const
	{
		return Tasks;
	}

private:
	FTaskSystem& Tasks;
	std::shared_ptr<IFileSystem> Files;
	std::shared_ptr<FIOStats> Stats = std::make_shared<FIOStats>();
};
} // namespace Hyperion
