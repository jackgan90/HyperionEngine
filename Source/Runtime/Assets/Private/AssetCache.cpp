#include "AssetServiceInternal.h"
#include <algorithm>

namespace Hyperion
{
void FAssetService::FImpl::ScheduleTrim(FTaskHandle InTask)
{
	// Admission holds Mutex and reserves both task slots before dispatching work.
	const auto Cleanup = IO.TaskSystem().Dispatch({EDomain::Worker},
	                                              [this, InTask]
	                                              {
		                                              try
		                                              {
			                                              IO.TaskSystem().Wait(InTask);
		                                              }
		                                              catch (...)
		                                              {
		                                              }
		                                              std::lock_guard Lock(Mutex);
		                                              Trim();
	                                              });
	Pending.push_back(Cleanup);
}

FAssetCacheStats FAssetService::FImpl::Trim()
{
	std::erase_if(Pending,
	              [](const FTaskHandle& InTask)
	              {
		              return InTask.Ready();
	              });
	std::erase_if(Operations,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Ready();
	              });
	for (auto It = Writes.begin(); It != Writes.end();)
	{
		if (It->second.Ready())
		{
			try
			{
				(void)It->second.GetReady();
				It = Writes.erase(It);
				continue;
			}
			catch (...)
			{
			}
		}
		++It;
	}
	// A failed write barrier remains observable until retry/invalidation, with bounded diagnostic retention.
	while (Writes.size() > Options.MaxEntries + Options.MaxInFlight)
	{
		auto It = std::find_if(Writes.begin(), Writes.end(),
		                       [](const auto& InEntry)
		                       {
			                       return InEntry.second.Ready();
		                       });
		if (It == Writes.end())
		{
			break;
		}
		Writes.erase(It);
	}
	FAssetCacheStats Stats;
	std::size_t Completed{};
	for (const auto& [Path, Entry] : Cache)
	{
		if (Entry.Request.Ready())
		{
			++Completed;
			try
			{
				Stats.RetainedBytes += Entry.Request.GetReady()->RetainedBytes;
			}
			catch (...)
			{
			}
		}
	}
	while (Completed > Options.MaxEntries || Stats.RetainedBytes > Options.MaxBytes)
	{
		auto Victim = Cache.end();
		for (auto It = Cache.begin(); It != Cache.end(); ++It)
		{
			if (It->second.Request.Ready() && (Victim == Cache.end() || It->second.LastUse < Victim->second.LastUse))
			{
				Victim = It;
			}
		}
		if (Victim == Cache.end())
		{
			break;
		}
		try
		{
			Stats.RetainedBytes -= Victim->second.Request.GetReady()->RetainedBytes;
		}
		catch (...)
		{
		}
		Cache.erase(Victim);
		--Completed;
	}
	Stats.Entries = Cache.size();
	Stats.InFlight = Pending.size();
	return Stats;
}

void FAssetService::Invalidate(const std::filesystem::path& InPath)
{
	auto& P = *Impl;
	const auto Path = NormalizeAssetPath(InPath);
	std::lock_guard Lock(P.Mutex);
	P.RequireOpen();
	P.Cache.erase(Path);
	if (const auto It = P.Writes.find(Path); It != P.Writes.end() && It->second.Ready())
	{
		P.Writes.erase(It);
	}
	P.Trim();
}

void FAssetService::ClearCache()
{
	auto& P = *Impl;
	std::lock_guard Lock(P.Mutex);
	P.RequireOpen();
	// An in-flight producer remains shared; explicit per-asset invalidation requests a new revision.
	std::erase_if(P.Cache,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Request.Ready();
	              });
	P.Trim();
}

FAssetCacheStats FAssetService::Statistics()
{
	std::lock_guard Lock(Impl->Mutex);
	return Impl->Trim();
}
} // namespace Hyperion
