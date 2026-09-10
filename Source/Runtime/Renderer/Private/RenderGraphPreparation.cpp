#include "Hyperion/Renderer/RenderGraph.h"
#include "RenderGraphResources.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace Hyperion
{
std::vector<std::size_t> FRenderGraph::Order() const
{
	if (Passes.empty())
	{
		throw std::invalid_argument("Graph requires at least one graphics pass");
	}

	struct FHazard
	{
		std::optional<std::size_t> Writer;
		std::set<std::size_t> Readers;
	};

	std::vector<FHazard> Hazards(Resources.size());
	std::vector<std::set<std::size_t>> Dependencies(Passes.size());
	std::set<std::string> Names;
	for (std::size_t Index = 0; Index < Passes.size(); ++Index)
	{
		const auto& Pass = Passes[Index];
		ValidateGraphPass(Pass, Resources, Identity);
		if (Pass.Name.empty() || !Names.insert(Pass.Name).second || (Pass.Prepare && !Pass.Batches.empty()))
		{
			throw std::invalid_argument("Invalid graph pass identity or mixed draw preparation");
		}
		for (const auto After : Pass.After)
		{
			if (After >= Passes.size())
			{
				throw std::invalid_argument("Unknown graph pass dependency");
			}
			Dependencies[Index].insert(After);
		}
		for (const auto Read : Pass.Reads)
		{
			auto& Hazard = Hazards[ResourceIndex(Read)];
			if (Hazard.Writer)
			{
				Dependencies[Index].insert(*Hazard.Writer);
			}
			Hazard.Readers.insert(Index);
		}
		std::vector<FGraphTexture> Writes;
		if (Pass.Color)
		{
			Writes.push_back(Pass.Color->Texture);
		}
		if (Pass.DepthStencil)
		{
			Writes.push_back(Pass.DepthStencil->Texture);
		}
		for (const auto Write : Writes)
		{
			auto& Hazard = Hazards[ResourceIndex(Write)];
			if (Hazard.Writer)
			{
				Dependencies[Index].insert(*Hazard.Writer);
			}
			Dependencies[Index].insert(Hazard.Readers.begin(), Hazard.Readers.end());
			Hazard.Readers.clear();
			Hazard.Writer = Index;
		}
	}
	std::vector<std::size_t> Result;
	std::vector<bool> Visited(Passes.size());
	while (Result.size() < Passes.size())
	{
		const auto Before = Result.size();
		for (std::size_t Index = 0; Index < Passes.size(); ++Index)
		{
			if (!Visited[Index] && std::all_of(Dependencies[Index].begin(), Dependencies[Index].end(),
			                                   [&](auto InDependency)
			                                   {
				                                   return Visited[InDependency];
			                                   }))
			{
				Visited[Index] = true;
				Result.push_back(Index);
				break; // Stable choice among all currently runnable passes.
			}
		}
		if (Before == Result.size())
		{
			throw std::invalid_argument("Graph dependency cycle");
		}
	}
	return Result;
}
} // namespace Hyperion
