#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include <stdexcept>

namespace Hyperion
{
std::size_t FRenderGraph::AddDeferred(std::function<std::vector<FColorPass>()> InPrepare,
                                      std::vector<std::size_t> InAfter)
{
	if (!InPrepare)
	{
		throw std::invalid_argument("Deferred graph preparation requires an owned function");
	}
	FColorPass Entry;
	Entry.After = std::move(InAfter);
	Preparations.emplace_back(Passes.size(), std::move(InPrepare));
	try
	{
		return Add(std::move(Entry));
	}
	catch (...)
	{
		Preparations.pop_back();
		throw;
	}
}

void FRenderGraph::ExpandPreparations()
{
	if (Preparations.empty())
	{
		return;
	}
	HYP_PERF_SCOPE_C(Rhi, PrepareGraphPasses);
	for (std::size_t Index = 0; Index < Passes.size(); ++Index)
	{
		for (const auto Dependency : Passes[Index].After)
		{
			// Pipeline order makes a self/forward dependency cyclic, including an empty expansion.
			if (Dependency >= Index)
			{
				throw std::runtime_error("Unknown or cyclic deferred graph dependency");
			}
		}
	}
	std::vector<FColorPass> Expanded;
	std::vector<std::optional<std::size_t>> Ends;
	auto Preparation = Preparations.begin();
	for (std::size_t Index = 0; Index < Passes.size(); ++Index)
	{
		auto& Entry = Passes[Index];
		std::vector<std::size_t> Dependencies;
		for (const auto Dependency : Entry.After)
		{
			if (Ends[Dependency])
			{
				Dependencies.push_back(*Ends[Dependency]);
			}
		}
		if (Preparation != Preparations.end() && Preparation->first == Index)
		{
			auto Group = Preparation++->second();
			const auto Base = Expanded.size();
			for (auto& Pass : Group)
			{
				for (auto& Dependency : Pass.After)
				{
					if (Dependency >= Group.size())
					{
						throw std::runtime_error("Unknown dependency within deferred graph preparation");
					}
					Dependency += Base;
				}
				Pass.After.insert(Pass.After.end(), Dependencies.begin(), Dependencies.end());
				Expanded.push_back(std::move(Pass));
			}
		}
		else
		{
			Entry.After = std::move(Dependencies);
			Expanded.push_back(std::move(Entry));
		}
		Ends.push_back(Expanded.empty() ? std::nullopt : std::optional(Expanded.size() - 1));
	}
	Passes = std::move(Expanded);
	Preparations.clear();
}
} // namespace Hyperion
