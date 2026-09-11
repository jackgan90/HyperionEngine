#include "Hyperion/IO/IOService.h"

namespace Hyperion
{
namespace
{
class FMemoryWriteLease final : public IFileWriteLease
{
};
} // namespace

std::shared_ptr<IFileWriteLease> IFileSystem::AcquireWriteLease(const std::filesystem::path& InPath)
{
	// Non-local filesystems get process-local exclusion; remote providers may override with their own lease.
	static std::mutex Mutex;
	static std::map<std::pair<const IFileSystem*, std::filesystem::path>, std::weak_ptr<IFileWriteLease>> Leases;
	std::lock_guard Lock(Mutex);
	std::erase_if(Leases,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.expired();
	              });
	const auto Key = std::make_pair(this, std::filesystem::absolute(InPath).lexically_normal());
	if (const auto It = Leases.find(Key); It != Leases.end() && !It->second.expired())
	{
		throw std::runtime_error("Conflicting asset publication: " + InPath.generic_string());
	}
	auto Lease = std::make_shared<FMemoryWriteLease>();
	Leases[Key] = Lease;
	return Lease;
}

TAsyncResult<std::shared_ptr<IFileWriteLease>> FIOService::AcquireWriteLeaseAsync(std::filesystem::path InPath,
                                                                                  FCancellationToken InCancellation)
{
	return DispatchAsync<std::shared_ptr<IFileWriteLease>>(
	    Tasks, {EDomain::Io},
	    [Storage = Files, Path = std::move(InPath)]
	    {
		    return Storage->AcquireWriteLease(Path);
	    },
	    InCancellation);
}
} // namespace Hyperion
