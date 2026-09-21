#include "Hyperion/IO/IOService.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>

namespace Hyperion
{
FBytes IFileSystem::ReadRange(const std::filesystem::path& InPath, std::size_t InOffset, std::size_t InSize)
{
	const auto Bytes = Read(InPath, 512u * 1024u * 1024u);
	if (InOffset > Bytes.size() || InSize > Bytes.size() - InOffset)
	{
		throw std::runtime_error("Invalid file read range");
	}
	return {Bytes.begin() + InOffset, Bytes.begin() + InOffset + InSize};
}

void IFileSystem::Remove(const std::filesystem::path&)
{
	throw std::logic_error("Storage backend does not support file removal");
}

void FMemoryFileSystem::Remove(const std::filesystem::path& InPath)
{
	std::lock_guard Lock(Mutex);
	Files.erase(InPath.lexically_normal());
}

std::vector<FDirectoryEntry> FMemoryFileSystem::ListDirectory(const std::filesystem::path& InDirectory)
{
	std::lock_guard Lock(Mutex);
	std::map<std::filesystem::path, bool> Entries;
	for (const auto& [Path, Bytes] : Files)
	{
		const auto Relative = Path.lexically_relative(InDirectory);
		if (!Relative.empty() && *Relative.begin() != ".." && Relative != ".")
		{
			Entries[InDirectory / *Relative.begin()] = Relative.has_parent_path();
		}
	}
	std::vector<FDirectoryEntry> Result;
	for (const auto& [Path, bDirectory] : Entries)
	{
		Result.push_back({Path, bDirectory});
	}
	return Result;
}

std::vector<FFileContents> IFileSystem::ReadTree(const std::filesystem::path& InDirectory,
                                                 std::span<const std::string_view> InExtensions, std::size_t InLimit)
{
	std::vector<FFileContents> Result;
	for (const auto& Path : Enumerate(InDirectory, true))
	{
		if (InExtensions.empty() ||
		    std::find(InExtensions.begin(), InExtensions.end(), Path.extension().string()) != InExtensions.end())
		{
			Result.push_back({Path, Read(Path, InLimit)});
		}
	}
	return Result;
}

std::filesystem::path IFileSystem::Normalize(const std::filesystem::path& InPath) const
{
	return NormalizeFilePath(InPath);
}

bool IFileSystem::Exists(const std::filesystem::path& InPath)
{
	try
	{
		(void)Read(InPath, 512u * 1024u * 1024u);
		return true;
	}
	catch (const FFileNotFound&)
	{
		return false;
	}
}

std::vector<std::filesystem::path> IFileSystem::Enumerate(const std::filesystem::path&, bool)
{
	throw std::logic_error("Storage backend does not support enumeration");
}

bool FMemoryFileSystem::Exists(const std::filesystem::path& InPath)
{
	std::lock_guard Lock(Mutex);
	return Files.contains(InPath.lexically_normal());
}

std::vector<std::filesystem::path> FMemoryFileSystem::Enumerate(const std::filesystem::path& InDirectory,
                                                                bool bInRecursive)
{
	std::lock_guard Lock(Mutex);
	std::vector<std::filesystem::path> Result;
	for (const auto& [Path, Bytes] : Files)
	{
		const auto Relative = Path.lexically_relative(InDirectory);
		if (!Relative.empty() && *Relative.begin() != ".." && (bInRecursive || Path.parent_path() == InDirectory))
		{
			Result.push_back(Path);
		}
	}
	return Result;
}

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

TAsyncResult<std::optional<FBytes>> FIOService::TryReadAsync(std::filesystem::path InPath,
                                                             FCancellationToken InCancellation, std::size_t InLimit)
{
	return DispatchAsync<std::optional<FBytes>>(
	    Tasks, {EDomain::Io},
	    [Path = std::move(InPath), InLimit, Storage = Files, Counters = Stats]() -> std::optional<FBytes>
	    {
		    try
		    {
			    auto Bytes = Storage->Read(Path, InLimit);
			    Counters->Reads.fetch_add(1);
			    Counters->ReadBytes.fetch_add(Bytes.size());
			    return Bytes;
		    }
		    catch (const FFileNotFound&)
		    {
			    return {};
		    }
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
		throw FFileNotFound("File not found: " + InPath.generic_string());
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
