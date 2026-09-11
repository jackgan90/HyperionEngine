#include "RenderResourcesInternal.h"

namespace Hyperion
{
void FRenderResourceCoordinator::CloseNativeResources()
{
	Tasks.Require({EDomain::Rhi, 0});
	Device.WaitIdle();
	Device.CollectCompletedResources();
	// Dropping a material lease schedules progress. Keep these drops outside the coordinator lock.
	std::vector<std::shared_ptr<const FRenderMaterial>> ReleasedMaterials;
	{
		std::lock_guard Lock(Mutex);
		PreparedDraws.clear();
		PreparedViews.clear();
		BatchAdmissions.clear();
		Fullscreen = {};
		for (const auto& [Key, Entry] : Entries)
		{
			if (!Entry.Record->CanRelease())
			{
				throw std::logic_error("Close requires released frame packets");
			}
		}
		for (const auto& [Key, Entry] : Entries)
		{
			auto Released = Entry.Record->Release();
			ReleasedMaterials.insert(ReleasedMaterials.end(), std::make_move_iterator(Released.begin()),
			                         std::make_move_iterator(Released.end()));
			++Stats.Retired;
		}
		Entries.clear();
		for (const auto& Entry : MaterialEntries)
		{
			Entry.Record->Release();
		}
		MaterialEntries.clear();
		Programs.clear();
		if (MaterialGpu)
		{
			MaterialGpu->ClearOwners();
			if (!MaterialGpu->IsEmpty())
			{
				throw std::logic_error("Close requires released material frame packets");
			}
			Stats.Materials = MaterialGpu->Statistics();
			MaterialGpu.reset();
		}
		if (MaterialConstants)
		{
			MaterialConstants->Clear();
			if (!MaterialConstants->CanRelease())
			{
				throw std::logic_error("Close requires released material constant slices");
			}
			Stats.Constants = MaterialConstants->Statistics();
			Stats.Constants.LivePages = 0;
			Stats.Constants.PageBytes = 0;
			MaterialConstants.reset();
		}
		Stats.LiveResources = 0;
		bNativeClosed = true;
	}
}
} // namespace Hyperion
