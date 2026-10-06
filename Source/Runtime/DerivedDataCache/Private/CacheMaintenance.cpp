#include "Hyperion/DerivedDataCache/DerivedDataCache.h"
#include "Hyperion/IO/ApplicationPaths.h"
#include <algorithm>
#include <vector>

namespace Hyperion
{
FCacheMaintenanceResult FDerivedDataCache::Maintain()
{
	std::lock_guard Lock(MaintenanceMutex);
	FCacheMaintenanceResult Result;

	struct FCandidate
	{
		std::filesystem::path Path;
		std::filesystem::file_time_type Time;
		std::uint64_t Size;
	};

	std::vector<FCandidate> Candidates;
	try
	{
		RequireUnlinkedPath(Directory);
		if (!std::filesystem::exists(Directory))
		{
			return Result;
		}
		if (MaintenanceCursor == std::filesystem::directory_iterator{})
		{
			MaintenanceBytes = 0;
			MaintenanceCursor = std::filesystem::directory_iterator(Directory);
		}
		while (MaintenanceCursor != std::filesystem::directory_iterator{} &&
		       Result.Scanned < Policy.MaintenanceScanLimit)
		{
			const auto Entry = *MaintenanceCursor++;
			++Result.Scanned;
			try
			{
				RequireUnlinkedPath(Entry.path());
				if (!Entry.is_regular_file() || Entry.path().extension() != ".ddc" ||
				    RecordPath(Entry.path().stem().string()) != Entry.path())
				{
					continue;
				}
				const auto Size = Entry.file_size();
				Candidates.push_back({Entry.path(), Entry.last_write_time(), Size});
				MaintenanceBytes += Size;
			}
			catch (const std::exception&)
			{
				// Unknown names, links and records concurrently removed by another process are not cleanup targets.
			}
		}
		Result.bComplete = MaintenanceCursor == std::filesystem::directory_iterator{};
		std::sort(Candidates.begin(), Candidates.end(),
		          [](const auto& InFirst, const auto& InSecond)
		          {
			          return InFirst.Time < InSecond.Time;
		          });
		const auto Oldest = std::filesystem::file_time_type::clock::now() - Policy.MaximumAge;
		for (const auto& Candidate : Candidates)
		{
			if (Candidate.Time >= Oldest && MaintenanceBytes <= Policy.MaximumBytes)
			{
				break;
			}
			RequireUnlinkedPath(Candidate.Path);
			std::error_code Error;
			if (std::filesystem::last_write_time(Candidate.Path, Error) != Candidate.Time || Error)
			{
				continue;
			}
			if (std::filesystem::remove(Candidate.Path, Error))
			{
				++Result.Removed;
				MaintenanceBytes -= Candidate.Size;
			}
		}
		Result.ObservedBytes = MaintenanceBytes;
	}
	catch (const std::exception& Error)
	{
		MaintenanceCursor = {};
		Result.bComplete = false;
		ReportFailure("maintenance", Error.what());
	}
	return Result;
}
} // namespace Hyperion
