#include "Hyperion/Core/Profiling.h"
#include "RenderResourcesInternal.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <thread>

namespace Hyperion
{
FRenderResource::FRenderResource(std::shared_ptr<FRenderResourceRecord> InRecord, std::function<void()> InReleased)
    : Record(std::move(InRecord)), Released(std::move(InReleased))
{
}

FRenderResource::~FRenderResource()
{
	Record.reset(); // Drop the thread-neutral lease before notifying the coordinator.
	Released();
}

ERenderResourceStatus FRenderResource::GetStatus() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Status;
}

std::string FRenderResource::GetError() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Error;
}

std::uint64_t FRenderResource::GetIdentity() const
{
	return Record->Identity;
}

std::shared_ptr<const FRenderResourceDesc> FRenderResource::GetDescription() const
{
	std::lock_guard Lock(Record->Publication);
	return Record->Status == ERenderResourceStatus::Ready ? Record->Description : nullptr;
}

FRenderResourceRecord::~FRenderResourceRecord()
{
	if ((!Vertices.empty() || !Indices.empty()) && !Tasks.IsCurrent({EDomain::Rhi, 0}))
	{
		std::terminate();
	}
}

void FRenderResourceRecord::Publish(ERenderResourceStatus InStatus, std::string InError)
{
	std::lock_guard Lock(Publication);
	const bool bChanged = Status != InStatus || Error != InError;
	Status = InStatus;
	Error = std::move(InError);
	if (bChanged)
	{
		Owner->PublicationRevision.fetch_add(1, std::memory_order_release);
	}
}

bool FRenderResourceRecord::CanRelease() const
{
	for (const auto& Buffer : Vertices)
	{
		if (Buffer.Payload.use_count() > 1)
		{
			return false;
		}
	}
	for (const auto& Buffer : Indices)
	{
		if (Buffer.Payload.use_count() > 1)
		{
			return false;
		}
	}
	return true;
}

std::vector<std::shared_ptr<const FRenderMaterial>> FRenderResourceRecord::Release()
{
	Tasks.Require({EDomain::Rhi, 0});
	std::vector<std::shared_ptr<const FRenderMaterial>> ReleasedMaterials;
	{
		std::lock_guard Lock(Publication);
		ReleasedMaterials = std::move(Materials);
	}
	Vertices.clear();
	Indices.clear();
	Publish(ERenderResourceStatus::Retired);
	return ReleasedMaterials;
}

std::shared_ptr<const FRenderResource> FRenderResourceCoordinator::MakeLease(FEntry& InEntry)
{
	if (auto Existing = InEntry.Lease.lock())
	{
		return Existing;
	}
	const std::weak_ptr<FRenderResourceCoordinator> Weak = shared_from_this();
	auto Lease = std::shared_ptr<const FRenderResource>(new FRenderResource(InEntry.Record,
	                                                                        [Weak]
	                                                                        {
		                                                                        if (auto Owner = Weak.lock())
		                                                                        {
			                                                                        Owner->Schedule();
		                                                                        }
	                                                                        }));
	InEntry.Lease = Lease;
	return Lease;
}

void FRenderResourceCoordinator::Schedule()
{
	std::lock_guard Lock(Mutex);
	if (bClosed || bScheduled)
	{
		return;
	}
	bScheduled = true;
	++Stats.MaintenanceTasks;
	try
	{
		Progress = Tasks.Dispatch({EDomain::Worker},
		                          [Owner = shared_from_this()]
		                          {
			                          // Only pending uploads/retirement poll. Ready static resources have no idle
			                          // polling job.
			                          std::this_thread::sleep_for(std::chrono::milliseconds(2));
			                          std::lock_guard Admission(Owner->Mutex);
			                          if (Owner->bClosed)
			                          {
				                          Owner->bScheduled = false;
				                          return;
			                          }
			                          Owner->Progress = Owner->Tasks.Dispatch({EDomain::Rhi, 0},
			                                                                  [Owner]
			                                                                  {
				                                                                  Owner->Tick();
			                                                                  });
		                          });
	}
	catch (...)
	{
		bScheduled = false;
		throw;
	}
}

bool FRenderResourceCoordinator::Process(FEntry& InEntry)
{
	auto& Record = *InEntry.Record;
	if (Record.Status == ERenderResourceStatus::Failed && Record.Preparation.Task())
	{
		if (!Record.Preparation.Ready())
		{
			return true;
		}
		Record.Preparation = {};
	}
	try
	{
		if (Record.Status == ERenderResourceStatus::Preparing)
		{
			if (!Record.Preparation.Ready())
			{
				return true;
			}
			Record.Description = Record.Preparation.GetReady();
			Record.Preparation = {};
			if (InEntry.Lease.expired())
			{
				return false;
			}
			Upload(Record);
		}
	}
	catch (const std::exception& Error)
	{
		Record.Preparation = {};
		Record.Publish(ERenderResourceStatus::Failed, Error.what());
	}
	catch (...)
	{
		Record.Preparation = {};
		Record.Publish(ERenderResourceStatus::Failed, "Unknown resource preparation/upload failure");
	}
	return (InEntry.Lease.expired() && (!Record.CanRelease() || InEntry.Record.use_count() != 1));
}

bool FRenderResourceCoordinator::CollectCompleted()
{
	try
	{
		Device.CollectCompletedResources();
		return true;
	}
	catch (const std::exception& Error)
	{
		for (const auto& [Key, Entry] : Entries)
		{
			Entry.Record->Publish(ERenderResourceStatus::Failed, Error.what());
		}
		for (auto& Entry : MaterialEntries)
		{
			Entry.Record->Publish(ERenderMaterialStatus::Failed, Error.what());
		}
	}
	catch (...)
	{
		for (const auto& [Key, Entry] : Entries)
		{
			Entry.Record->Publish(ERenderResourceStatus::Failed, "GPU completion query failed");
		}
	}
	for (auto& Entry : MaterialEntries)
	{
		if (Entry.Record->Status != ERenderMaterialStatus::Failed)
		{
			Entry.Record->Publish(ERenderMaterialStatus::Failed, "GPU completion query failed");
		}
	}
	// Failed queries prove no completion. Keep ownership and allow a later collection or shutdown drain.
	return false;
}

void FRenderResourceCoordinator::Tick()
{
	HYP_PERF_SCOPE_C(Rhi, ResourceMaintenance);
	Tasks.Require({EDomain::Rhi, 0});
	bool bAgain = false;
	std::vector<std::shared_ptr<const FRenderMaterial>> ReleasedMaterials;
	{
		std::lock_guard Lock(Mutex);
		bScheduled = false;
		if (bClosed)
		{
			return;
		}
		++Stats.MaintenanceTicks;
		const bool bCompleted = CollectCompleted();
		CollectPreparedDraws();
		CollectViewPasses();
		bAgain = !bCompleted;
		if (bCompleted)
		{
			bAgain |= ProcessMaterials();
		}
		for (auto It = Entries.begin(); It != Entries.end();)
		{
			const bool bPending = !bCompleted || Process(It->second);
			if (!bPending && It->second.Lease.expired() && It->second.Record.use_count() == 1)
			{
				auto Released = It->second.Record->Release();
				ReleasedMaterials.insert(ReleasedMaterials.end(), std::make_move_iterator(Released.begin()),
				                         std::make_move_iterator(Released.end()));
				It = Entries.erase(It);
				++Stats.Retired;
			}
			else
			{
				bAgain |= bPending;
				++It;
			}
		}
		bAgain |= std::any_of(MaterialEntries.begin(), MaterialEntries.end(),
		                      [](const FMaterialEntry& InEntry)
		                      {
			                      return InEntry.Record->Status == ERenderMaterialStatus::Preparing ||
			                             InEntry.Record->Status == ERenderMaterialStatus::Uploading;
		                      });
		Stats.LiveResources = Entries.size();
	}
	if (bAgain)
	{
		Schedule();
	}
}
} // namespace Hyperion
