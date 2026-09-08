#include "Hyperion/IO/IOService.h"
#include "Hyperion/Core/Profiling.h"

namespace Hyperion
{
FIOService::FIOService(FTaskSystem& InTasks, std::shared_ptr<IFileSystem> InFiles)
    : Tasks(InTasks), Files(std::move(InFiles))
{
	if (!Files)
	{
		throw std::invalid_argument("Null file system");
	}
}

TAsyncResult<FBytes> FIOService::ReadAsync(std::filesystem::path InPath, FCancellationToken InCancellation,
                                           std::size_t InLimit)
{
	return DispatchAsync<FBytes>(
	    Tasks, {EDomain::Io},
	    [InPath = std::move(InPath), InLimit, Storage = Files, Counters = Stats]
	    {
		    HYP_PERF_SCOPE_C(Assets, ReadAssetBytes);
		    auto Bytes = Storage->Read(InPath, InLimit);
		    Counters->Reads.fetch_add(1);
		    Counters->ReadBytes.fetch_add(Bytes.size());
		    return Bytes;
	    },
	    InCancellation);
}

TAsyncResult<bool> FIOService::WriteAsync(std::filesystem::path InPath, FBytes InBytes,
                                          FCancellationToken InCancellation)
{
	return DispatchAsync<bool>(
	    Tasks, {EDomain::Io},
	    [InPath = std::move(InPath), Bytes = std::move(InBytes), Storage = Files, Counters = Stats]
	    {
		    Storage->WriteAtomic(InPath, Bytes);
		    Counters->Writes.fetch_add(1);
		    Counters->WrittenBytes.fetch_add(Bytes.size());
		    return true;
	    },
	    InCancellation);
}

FBytes FMemoryFileSystem::Read(const std::filesystem::path& InPath, std::size_t InLimit)
{
	std::lock_guard Lock(Mutex);
	auto It = Files.find(InPath.lexically_normal());
	if (It == Files.end())
	{
		throw std::runtime_error("File not found: " + InPath.generic_string());
	}
	if (It->second.size() > InLimit)
	{
		throw std::runtime_error("File exceeds read limit");
	}
	return It->second;
}

void FMemoryFileSystem::WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes)
{
	FBytes Copy(InBytes.begin(), InBytes.end());
	std::lock_guard Lock(Mutex);
	Files[InPath.lexically_normal()] = std::move(Copy);
}
} // namespace Hyperion
