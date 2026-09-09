#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "RenderGraphResources.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FAttachmentInitialization
{
	bool bFull{};
	std::vector<FViewport> Regions;

	void Use(const std::optional<FViewport>& InViewport, bool bInInitialize, const char* InName)
	{
		if (bInInitialize)
		{
			if (InViewport)
			{
				Regions.push_back(*InViewport);
			}
			else
			{
				bFull = true;
			}
			return;
		}
		if (bFull)
		{
			return;
		}
		if (InViewport && std::any_of(Regions.begin(), Regions.end(),
		                              [&](const FViewport& InRegion)
		                              {
			                              return InRegion.X <= InViewport->X && InRegion.Y <= InViewport->Y &&
			                                     InRegion.X + InRegion.Width >= InViewport->X + InViewport->Width &&
			                                     InRegion.Y + InRegion.Height >= InViewport->Y + InViewport->Height;
		                              }))
		{
			return;
		}
		throw std::runtime_error(std::string("Graph loads undefined ") + InName);
	}
};

struct FGraphAttachments
{
	FAttachmentInitialization Color;
	FAttachmentInitialization Depth;
	FAttachmentInitialization Stencil;
	std::optional<std::pair<std::uint64_t, ERHIDepthFormat>> DepthOwner;

	void Validate(const FColorPass& InPass)
	{
		const auto& Commands = InPass.Commands;
		if ((Commands.bClearDepth && !Commands.bUseDepth) || (Commands.bClearStencil && !Commands.bUseStencil) ||
		    (Commands.bUseDepth && Commands.DepthFormat == ERHIDepthFormat::None) ||
		    (Commands.bUseStencil && Commands.DepthFormat != ERHIDepthFormat::D32S8))
		{
			throw std::runtime_error("Invalid graph depth/stencil attachment or clear");
		}
		const auto Key = std::make_pair(Commands.DepthDomain, Commands.DepthFormat);
		if ((Commands.bUseDepth || Commands.bUseStencil) && DepthOwner != Key)
		{
			Depth = {};
			Stencil = {};
			DepthOwner = Key;
		}
		if (Commands.bUseDepth && !Commands.DepthTarget)
		{
			Depth.Use(Commands.Viewport, Commands.bClearDepth, "depth");
		}
		if (Commands.bUseStencil)
		{
			Stencil.Use(Commands.Viewport, Commands.bClearStencil, "stencil");
		}
		if (Commands.bUseColor)
		{
			Color.Use(Commands.Viewport, InPass.Load != EColorLoad::Load, "color contents");
		}
		else if (InPass.Load != EColorLoad::Load)
		{
			throw std::invalid_argument("Color load operation requires a color attachment");
		}
	}
};

} // namespace

std::size_t FRenderGraph::Add(FColorPass InPass)
{
	Passes.push_back(std::move(InPass));
	return Passes.size() - 1;
}

void FRenderGraph::ImportDepth(FTexture InTexture)
{
	if (!InTexture)
	{
		throw std::invalid_argument("Cannot import an empty graph texture");
	}
	ImportedDepth.push_back(std::move(InTexture));
}

std::vector<FPassCommands> FRenderGraph::Compile() const
{
	auto Copy = *this;
	auto Result = Copy.CompileAndConsume();
	for (auto& Pass : Result)
	{
		Pass.MaterializeDraws();
	}
	return Result;
}

std::vector<FPassCommands> FRenderGraph::CompileAndConsume()
{
	HYP_PERF_SCOPE_C(Render, CompileRenderGraph);
	ExpandPreparations();
	if (Passes.empty())
	{
		throw std::runtime_error("Graph requires at least one color pass");
	}
	const auto Count = Passes.size();
	std::vector<std::set<std::size_t>> Deps(Count);
	std::set<std::string> Names;
	for (std::size_t I = 0; I < Count; ++I)
	{
		if (Passes[I].Commands.Name.empty() || !Names.insert(Passes[I].Commands.Name).second)
		{
			throw std::runtime_error("Graph pass names must be unique and nonempty");
		}
		if (Passes[I].Commands.TransitionFrom || Passes[I].Commands.TransitionTo ||
		    !Passes[I].Commands.TextureTransitions.empty())
		{
			throw std::runtime_error("Graph owns resource transitions");
		}
		if (I)
		{
			Deps[I].insert(I - 1); // Preserve pipeline order, including explicit depth producer/consumer hazards.
		}
		for (auto Dependency : Passes[I].After)
		{
			if (Dependency >= Count)
			{
				throw std::runtime_error("Unknown graph dependency");
			}
			Deps[I].insert(Dependency);
		}
	}
	std::vector<FPassCommands> Result;
	Result.reserve(Count + 1);
	std::vector<bool> Visited(Count);
	FGraphAttachments Attachments;
	FGraphDepthResources DepthResources(ImportedDepth);
	bool bColorStarted = false;
	while (Result.size() < Count)
	{
		bool bProgress = false;
		for (std::size_t I = 0; I < Count; ++I)
		{
			if (Visited[I] || !std::all_of(Deps[I].begin(), Deps[I].end(),
			                               [&](auto InD)
			                               {
				                               return Visited[InD];
			                               }))
			{
				continue;
			}
			auto& Pass = Passes[I];
			Attachments.Validate(Pass);
			auto Commands = std::move(Pass.Commands);
			Commands.bClear = Pass.Load == EColorLoad::Clear;
			DepthResources.Compile(Commands);
			if (Commands.bUseColor && !bColorStarted)
			{
				bColorStarted = true;
				Commands.TransitionFrom = EResourceState::Present;
				Commands.TransitionTo = EResourceState::RenderTarget;
			}
			Result.push_back(std::move(Commands));
			Visited[I] = true;
			bProgress = true;
		}
		if (!bProgress)
		{
			throw std::runtime_error("Graph dependency cycle");
		}
	}
	FPassCommands Present;
	Present.Name = "Present transition";
	Present.bUseColor = false;
	if (bColorStarted)
	{
		Present.TransitionFrom = EResourceState::RenderTarget;
		Present.TransitionTo = EResourceState::Present;
	}
	DepthResources.Finish(Present);
	Result.push_back(std::move(Present));
	Passes.clear();
	ImportedDepth.clear();
	return Result;
}

} // namespace Hyperion
