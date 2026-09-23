#include "Hyperion/Automation/Session.h"

namespace Hyperion
{
namespace
{
FArchiveNode Complete(FArchiveNode InValue)
{
	try
	{
		(void)WriteJson(InValue);
	}
	catch (const std::exception& Error)
	{
		throw FAutomationError(
		    "result_unavailable",
		    std::string("Operation ran but its result cannot be encoded; inspect current state before "
		                "any retry: ") +
		        Error.what());
	}
	return FArchiveNode(
	    FArchiveNode::FObject{{"status", WriteValue(std::string("completed"))}, {"result", std::move(InValue)}});
}
} // namespace

FAutomationSession::FAutomationSession(const FOperationCatalog& InCatalog, FAutomationLimits InLimits)
    : Catalog(InCatalog), Limits(InLimits)
{
	if (!Catalog.IsSealed() || !Limits.MaxRunningJobs || Limits.MaxRetainedJobs < Limits.MaxRunningJobs)
	{
		throw std::invalid_argument("Session requires a sealed catalog and valid job limits");
	}
	Id = CreateAutomationIdentity();
}

void FAutomationSession::MakeRoom()
{
	if (PendingCount() >= Limits.MaxRunningJobs)
	{
		throw FAutomationError("busy", "Running job limit reached; wait for a job before retrying");
	}
	if (Jobs.size() >= Limits.MaxRetainedJobs)
	{
		const auto It = std::find_if(Jobs.begin(), Jobs.end(),
		                             [](const auto& InEntry)
		                             {
			                             return InEntry.second.State != "running";
		                             });
		if (It != Jobs.end())
		{
			Jobs.erase(It);
		}
	}
}

FArchiveNode FAutomationSession::Call(std::string_view InOperation, const FArchiveNode& InArguments)
{
	Catalog.RequireOwner();
	if (bInvoking)
	{
		return AutomationFailure("busy", "Reentrant operation invocation is not supported");
	}
	try
	{
		if (bStopped)
		{
			throw FAutomationError("session_closed", "Session no longer accepts operations");
		}
		const auto& Operation = Catalog.Find(InOperation);
		if (!Operation.Info.Unavailable.empty())
		{
			throw FAutomationError("unavailable", Operation.Info.Unavailable);
		}
		const auto Request = ReadRecordWire(*Operation.Request, InArguments);
		std::optional<std::uint64_t> JobKey;
		if (Operation.bAsynchronous)
		{
			MakeRoom();
			JobKey = ++NextJob;
			Jobs.emplace(*JobKey, FJob{Id + "/job/" + std::to_string(*JobKey), std::string(InOperation)});
		}
		bInvoking = true;
		try
		{
			auto Task = Operation.Invoke(Request.get());
			bInvoking = false;
			if (!Operation.bAsynchronous)
			{
				if (!Task.Completed)
				{
					throw std::logic_error("Synchronous operation returned no result");
				}
				return Complete(std::move(*Task.Completed));
			}
			Jobs.at(*JobKey).Task = std::move(Task);
			return DescribeJob(Jobs.at(*JobKey));
		}
		catch (...)
		{
			bInvoking = false;
			if (JobKey)
			{
				Jobs.erase(*JobKey);
			}
			throw;
		}
	}
	catch (...)
	{
		return CurrentAutomationFailure();
	}
}

void FAutomationSession::Poll()
{
	Catalog.RequireOwner();
	if (bInvoking)
	{
		throw std::logic_error("Cannot poll jobs inside an operation");
	}
	bInvoking = true;
	for (auto& [Key, Job] : Jobs)
	{
		if (Job.State != "running")
		{
			continue;
		}
		try
		{
			const auto Result = Job.Task.Completed ? Job.Task.Completed : Job.Task.Poll();
			if (!Result)
			{
				continue;
			}
			Job.Outcome = Complete(*Result);
			Job.State = "completed";
		}
		catch (...)
		{
			Job.Outcome = CurrentAutomationFailure();
			Job.State = "failed";
		}
		Job.Task = {};
	}
	bInvoking = false;
}

FAutomationSession::FJob& FAutomationSession::FindJob(std::string_view InId)
{
	Catalog.RequireOwner();
	for (auto& [Key, Job] : Jobs)
	{
		if (Job.Id == InId)
		{
			return Job;
		}
	}
	throw FAutomationError("not_found", "Unknown, expired or foreign-session job", "job");
}

FArchiveNode FAutomationSession::DescribeJob(const FJob& InJob) const
{
	FArchiveNode::FObject Result{{"status", WriteValue(InJob.State)},
	                             {"job", WriteValue(InJob.Id)},
	                             {"operation", WriteValue(InJob.Operation)},
	                             {"cancellable", WriteValue(InJob.State == "running" && bool(InJob.Task.Cancel))}};
	if (InJob.State == "running")
	{
		Result.emplace("pollAfterMs", WriteValue(std::uint32_t(20)));
	}
	else
	{
		Result.emplace("outcome", InJob.Outcome);
	}
	return FArchiveNode(std::move(Result));
}

FArchiveNode FAutomationSession::GetJob(std::string_view InId)
{
	Poll();
	return DescribeJob(FindJob(InId));
}

FArchiveNode FAutomationSession::CancelJob(std::string_view InId)
{
	Poll();
	auto& Job = FindJob(InId);
	if (Job.State != "running")
	{
		return DescribeJob(Job);
	}
	if (!Job.Task.Cancel)
	{
		throw FAutomationError("not_cancellable", "This operation cannot be cancelled after admission", "job");
	}
	Job.Task.Cancel();
	Job.State = "cancelled";
	Job.Outcome = AutomationFailure("cancelled", "Operation cancelled; provider cleanup is still drained at shutdown");
	Job.Task = {};
	return DescribeJob(Job);
}

void FAutomationSession::StopAdmission()
{
	Catalog.RequireOwner();
	bStopped = true;
}

std::size_t FAutomationSession::PendingCount() const
{
	Catalog.RequireOwner();
	return std::count_if(Jobs.begin(), Jobs.end(),
	                     [](const auto& InEntry)
	                     {
		                     return InEntry.second.State == "running";
	                     });
}

const std::string& FAutomationSession::GetId() const
{
	Catalog.RequireOwner();
	return Id;
}

const FOperationCatalog& FAutomationSession::GetCatalog() const
{
	Catalog.RequireOwner();
	return Catalog;
}
} // namespace Hyperion
