#pragma once
#include "Hyperion/Automation/Catalog.h"

namespace Hyperion
{
struct FAutomationLimits
{
	std::size_t MaxRunningJobs = 32;
	std::size_t MaxRetainedJobs = 128;
};

// Stop admission, pump/Poll admitted jobs, then drain providers before destroying captured state.
// Does not own provider services. IDs and retained results expire with this session.
class FAutomationSession
{
public:
	explicit FAutomationSession(const FOperationCatalog& InCatalog, FAutomationLimits InLimits = {});
	FArchiveNode Call(std::string_view InOperation, const FArchiveNode& InArguments);
	void Poll();
	FArchiveNode GetJob(std::string_view InId);
	FArchiveNode CancelJob(std::string_view InId);
	void StopAdmission();
	std::size_t PendingCount() const;
	const std::string& GetId() const;
	const FOperationCatalog& GetCatalog() const;

private:
	struct FJob
	{
		std::string Id;
		std::string Operation;
		std::string State = "running";
		FOperationTask Task;
		FArchiveNode Outcome;
	};

	FJob& FindJob(std::string_view InId);
	FArchiveNode DescribeJob(const FJob& InJob) const;
	void MakeRoom();
	const FOperationCatalog& Catalog;
	FAutomationLimits Limits;
	std::string Id;
	std::uint64_t NextJob{};
	std::map<std::uint64_t, FJob> Jobs;
	bool bStopped{};
	bool bInvoking{};
};
} // namespace Hyperion
