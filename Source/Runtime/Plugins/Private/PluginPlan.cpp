#include "Hyperion/Plugins/PluginRuntime.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
using FEntries = std::map<std::string, FPluginPlanEntry>;
using FPredecessors = std::map<std::string, std::vector<std::string>>;
using FProviders = std::map<std::type_index, std::string>;

void ConnectServices(const FPluginDescriptor& InDescriptor, FPluginPlanEntry& InEntry, const FProviders& InProviders,
                     const FPluginServices& InServices, std::vector<std::string>& OutPrevious)
{
	for (const auto Type : InDescriptor.Requires)
	{
		if (const auto Provider = InProviders.find(Type); Provider != InProviders.end())
		{
			InEntry.RequiredPlugins.push_back(Provider->second);
		}
		else if (!InServices.Contains(Type))
		{
			InEntry.Unavailable = std::string("Missing required service: ") + Type.name();
		}
	}
	OutPrevious.insert(OutPrevious.end(), InEntry.RequiredPlugins.begin(), InEntry.RequiredPlugins.end());
	for (const auto Type : InDescriptor.Optional)
	{
		if (const auto Provider = InProviders.find(Type);
		    Provider != InProviders.end() && Provider->second != InEntry.Id)
		{
			OutPrevious.push_back(Provider->second);
		}
	}
}

void ConnectOrdering(const FPluginDescriptor& InDescriptor, const FEntries& InEntries, FPredecessors& OutPredecessors)
{
	for (const auto& Conflict : InDescriptor.Conflicts)
	{
		if (InEntries.contains(Conflict) && InEntries.at(Conflict).Unavailable.empty())
		{
			throw std::invalid_argument("Conflicting plugins: " + InDescriptor.Id + " and " + Conflict);
		}
	}
	for (const auto& After : InDescriptor.After)
	{
		if (InEntries.contains(After))
		{
			OutPredecessors[InDescriptor.Id].push_back(After);
		}
	}
	for (const auto& Before : InDescriptor.Before)
	{
		if (InEntries.contains(Before))
		{
			OutPredecessors[Before].push_back(InDescriptor.Id);
		}
	}
}

FPluginPlan SortPlan(const FPluginSelection& InSelection, const FEntries& InEntries, FPredecessors InPredecessors)
{
	FPluginPlan Result;
	std::map<std::string, int> Marks;
	std::function<void(const std::string&)> Visit = [&](const std::string& InId)
	{
		if (Marks[InId] == 2)
		{
			return;
		}
		if (Marks[InId] == 1)
		{
			throw std::runtime_error("Plugin dependency cycle at: " + InId);
		}
		Marks[InId] = 1;
		for (const auto& Previous : InPredecessors[InId])
		{
			Visit(Previous);
		}
		Marks[InId] = 2;
		Result.Entries.push_back(InEntries.at(InId));
	};
	for (const auto& Id : InSelection.Requested)
	{
		Visit(Id);
	}
	return Result;
}
} // namespace

FPluginPlan FPluginRegistry::Plan(const FPluginSelection& InSelection, const FPluginServices& InServices) const
{
	InServices.RequireOwner();
	FEntries Entries;
	FPredecessors Predecessors;
	const std::set<std::string> Disabled(InSelection.Disabled.begin(), InSelection.Disabled.end());
	std::function<void(const std::string&)> Select = [&](const std::string& InId)
	{
		if (Entries.contains(InId))
		{
			return;
		}
		auto& Entry = Entries[InId];
		Entry.Id = InId;
		const auto* Descriptor = Find(InId);
		if (Disabled.contains(InId) || !Descriptor)
		{
			Entry.Unavailable = Disabled.contains(InId) ? "Plugin explicitly disabled" : "Missing plugin: " + InId;
			return;
		}
		Entry.RequiredPlugins = Descriptor->Dependencies;
		for (const auto& Dependency : Descriptor->Dependencies)
		{
			Select(Dependency);
		}
	};
	for (const auto& Id : InSelection.Requested)
	{
		Select(Id);
	}
	FProviders Providers;
	for (const auto& [Id, Entry] : Entries)
	{
		if (!Entry.Unavailable.empty())
		{
			continue;
		}
		for (const auto Type : Find(Id)->Provides)
		{
			if (InServices.Contains(Type) || !Providers.emplace(Type, Id).second)
			{
				throw std::invalid_argument("Conflicting plugin service provider: " + Id);
			}
		}
	}
	for (auto& [Id, Entry] : Entries)
	{
		if (!Entry.Unavailable.empty())
		{
			continue;
		}
		const auto& Descriptor = *Find(Id);
		ConnectServices(Descriptor, Entry, Providers, InServices, Predecessors[Id]);
		ConnectOrdering(Descriptor, Entries, Predecessors);
	}
	auto Result = SortPlan(InSelection, Entries, std::move(Predecessors));
	if (InSelection.FailurePolicy == EPluginFailurePolicy::Strict)
	{
		for (const auto& Entry : Result.Entries)
		{
			if (!Entry.Unavailable.empty())
			{
				throw std::runtime_error(Entry.Id + ": " + Entry.Unavailable);
			}
		}
	}
	return Result;
}
} // namespace Hyperion
