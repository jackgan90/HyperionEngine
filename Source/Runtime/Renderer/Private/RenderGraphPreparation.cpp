#include "Hyperion/Renderer/RenderGraph.h"
#include "RenderGraphBuffers.h"
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

	std::vector<FHazard> Hazards(Resources.size() + Buffers.size());
	std::vector<std::set<std::size_t>> Dependencies(Passes.size());
	std::set<std::string> Names;
	for (std::size_t Index = 0; Index < Passes.size(); ++Index)
	{
		const auto& Pass = Passes[Index];
		ValidateGraphPass(Pass, Resources, Identity);
		ValidateGraphBuffers(Pass, Buffers, Identity);
		if (Pass.Name.empty() || !Names.insert(Pass.Name).second || (Pass.Prepare && !Pass.Batches.empty()) ||
		    (Pass.PrepareCompute && !Pass.Dispatches.empty()))
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
		std::vector<std::size_t> Reads;
		std::vector<std::size_t> Writes;
		for (const auto Read : Pass.Reads)
		{
			Reads.push_back(ResourceIndex(Read));
		}
		for (const auto& Access : Pass.Buffers)
		{
			(Access.State == EResourceState::ShaderRead ? Reads : Writes)
			    .push_back(Resources.size() + Access.Buffer.Index);
		}
		for (const auto Read : Reads)
		{
			auto& Hazard = Hazards[Read];
			if (Hazard.Writer)
			{
				Dependencies[Index].insert(*Hazard.Writer);
			}
			Hazard.Readers.insert(Index);
		}
		for (const auto& Color : Pass.GetColors())
		{
			Writes.push_back(ResourceIndex(Color.Texture));
		}
		for (const auto& Write : Pass.ComputeWrites)
		{
			Writes.push_back(ResourceIndex(Write.Texture));
		}
		if (Pass.DepthStencil)
		{
			Writes.push_back(ResourceIndex(Pass.DepthStencil->Texture));
		}
		for (const auto Write : Writes)
		{
			auto& Hazard = Hazards[Write];
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
