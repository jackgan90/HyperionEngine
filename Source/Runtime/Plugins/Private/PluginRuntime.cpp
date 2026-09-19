#include "Hyperion/Plugins/PluginRuntime.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hyperion
{
FPluginSet::~FPluginSet()
{
	Stop();
}

FPluginSet::FPluginSet(FPluginSet&&) noexcept = default;

std::span<const std::unique_ptr<FPlugin>> FPluginSet::GetInstances() const
{
	return Instances;
}

const std::vector<std::string>& FPluginSet::GetOrder() const
{
	return Order;
}

const std::vector<FPluginDiagnostic>& FPluginSet::GetDiagnostics() const
{
	return Diagnostics;
}

bool FPluginSet::IsActive(const std::string& InId) const
{
	return std::find(Order.begin(), Order.end(), InId) != Order.end();
}

void FPluginSet::RequireOwner() const
{
	if (Services)
	{
		Services->RequireOwner();
	}
}

void FPluginSet::Update(const FPluginUpdate& InUpdate)
{
	RequireOwner();
	if (bQuiesced || !std::isfinite(InUpdate.DeltaSeconds) || InUpdate.DeltaSeconds < 0 ||
	    !std::isfinite(InUpdate.ElapsedSeconds) || InUpdate.ElapsedSeconds < 0)
	{
		throw std::logic_error("Invalid update or stopped plugin set");
	}
	for (const auto& Instance : Instances)
	{
		Instance->Update(InUpdate);
	}
}

void FPluginSet::Quiesce() noexcept
{
	if (bQuiesced || Instances.empty())
	{
		return;
	}
	RequireOwner();
	bQuiesced = true;
	for (const auto& Context : Contexts)
	{
		Context->Disconnect();
	}
	for (auto It = Instances.rbegin(); It != Instances.rend(); ++It)
	{
		(*It)->Quiesce();
	}
}

void FPluginSet::Stop() noexcept
{
	Quiesce();
	while (!Instances.empty())
	{
		Contexts.back()->Drain();
		Instances.back()->Stop();
		Contexts.back()->Close();
		if (!StopFailure)
		{
			StopFailure = Contexts.back()->GetCleanupFailure();
		}
		Instances.pop_back();
		Contexts.pop_back();
		Order.pop_back();
	}
}

std::exception_ptr FPluginSet::GetStopFailure() const
{
	return StopFailure;
}

void FPluginRegistry::Add(FPluginDescriptor InDescriptor)
{
	if (InDescriptor.Id.empty() || (!InDescriptor.Create && !InDescriptor.CreateWithContext))
	{
		throw std::invalid_argument("Plugin requires an ID and factory");
	}
	if (Find(InDescriptor.Id))
	{
		throw std::invalid_argument("Duplicate plugin: " + InDescriptor.Id);
	}
	Descriptors.push_back(std::move(InDescriptor));
}

const FPluginDescriptor* FPluginRegistry::Find(const std::string& InId) const
{
	const auto Found = std::find_if(Descriptors.begin(), Descriptors.end(),
	                                [&](const auto& InDescriptor)
	                                {
		                                return InDescriptor.Id == InId;
	                                });
	return Found == Descriptors.end() ? nullptr : &*Found;
}

FPluginSet FPluginRegistry::Activate(std::span<const std::string> InRequested) const
{
	FPluginSelection Selection;
	Selection.Requested.assign(InRequested.begin(), InRequested.end());
	Selection.FailurePolicy = EPluginFailurePolicy::Strict;
	return Activate(Selection, std::make_shared<FPluginServices>());
}

FPluginSet FPluginRegistry::Activate(const FPluginSelection& InSelection,
                                     std::shared_ptr<FPluginServices> InServices) const
{
	if (!InServices)
	{
		throw std::invalid_argument("Plugin activation requires a service scope");
	}
	const auto PlanResult = Plan(InSelection, *InServices);
	FPluginSet Result;
	Result.Services = std::move(InServices);
	Result.Instances.reserve(PlanResult.Entries.size());
	Result.Contexts.reserve(PlanResult.Entries.size());
	Result.Order.reserve(PlanResult.Entries.size());
	for (const auto& Entry : PlanResult.Entries)
	{
		bool bStartupFailure = false;
		std::string Error = Entry.Unavailable;
		for (const auto& Required : Entry.RequiredPlugins)
		{
			if (!Result.IsActive(Required))
			{
				Error = "Unavailable dependency: " + Required;
				break;
			}
		}
		if (Error.empty())
		{
			const auto& Descriptor = *Find(Entry.Id);
			auto Context = std::make_unique<FPluginContext>(*Result.Services, Descriptor.Id, Descriptor.Provides,
			                                                Descriptor.Requires, Descriptor.Optional);
			std::unique_ptr<FPlugin> Instance;
			try
			{
				Instance = Descriptor.CreateWithContext ? Descriptor.CreateWithContext(*Context) : Descriptor.Create();
				if (!Instance)
				{
					throw std::runtime_error("Null plugin factory result");
				}
				Instance->Start(*Context);
				for (const auto Type : Descriptor.Provides)
				{
					if (!Result.Services->Contains(Type))
					{
						throw std::logic_error("Plugin did not publish a declared service");
					}
				}
				Result.Order.push_back(Entry.Id);
				Result.Instances.push_back(std::move(Instance));
				Result.Contexts.push_back(std::move(Context));
				continue;
			}
			catch (const std::exception& Failure)
			{
				bStartupFailure = true;
				Error = Failure.what();
			}
			catch (...)
			{
				bStartupFailure = true;
				Error = "Unknown startup failure";
			}
			Context->Disconnect();
			if (Instance)
			{
				Instance->Quiesce();
			}
			Context->Drain();
			if (Instance)
			{
				Instance->Stop();
			}
			Context->Close();
			if (!Result.StopFailure)
			{
				Result.StopFailure = Context->GetCleanupFailure();
			}
		}
		Result.Diagnostics.push_back({Entry.Id, Error, bStartupFailure});
		if (InSelection.FailurePolicy == EPluginFailurePolicy::Strict)
		{
			throw std::runtime_error(Entry.Id + ": " + Error);
		}
	}
	return Result;
}
} // namespace Hyperion
