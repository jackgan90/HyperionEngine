#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Core/Profiling.h"
#include "RenderGraphResources.h"
#include <algorithm>
#include <atomic>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::uint64_t NextGraphIdentity()
{
	static std::atomic<std::uint64_t> Next{1};
	return Next.fetch_add(1, std::memory_order_relaxed);
}

bool SameImport(const FGraphTextureImport& InA, const FGraphTextureImport& InB)
{
	return InA.Target == InB.Target && InA.Size.Width == InB.Size.Width && InA.Size.Height == InB.Size.Height &&
	       InA.DepthFormat == InB.DepthFormat && InA.InitialState == InB.InitialState &&
	       InA.bInitialized == InB.bInitialized && InA.Identity == InB.Identity &&
	       bool(InA.Resolve) == bool(InB.Resolve);
}

std::vector<FPassCommands> PreparePass(FGraphicsPass& InPass, FPassCommands InCommands)
{
	auto Batches = InPass.Prepare ? InPass.Prepare() : std::move(InPass.Batches);
	if (Batches.empty())
	{
		Batches.push_back({}); // Clear/store and resource dependencies survive an empty draw list.
		if (InPass.Color)
		{
			Batches.back().bSrgb = InPass.Color->View == EGraphColorView::Srgb;
		}
	}
	std::vector<FPassCommands> Result;
	Result.reserve(Batches.size());
	for (std::size_t Index = 0; Index < Batches.size(); ++Index)
	{
		auto Commands = InCommands;
		Commands.Name = InPass.Name + "/" + std::to_string(Index);
		static_cast<FDrawCommands&>(Commands) = std::move(Batches[Index].Commands);
		if (Commands.Color)
		{
			if (InPass.Color->View != EGraphColorView::DrawBatch &&
			    Batches[Index].bSrgb != (InPass.Color->View == EGraphColorView::Srgb))
			{
				throw std::invalid_argument("Draw batch uses an undeclared color view");
			}
			Commands.Color->bSrgb = Batches[Index].bSrgb;
			Commands.Color->Actions.Load = Index == 0 ? Commands.Color->Actions.Load : EAttachmentLoad::Load;
			Commands.Color->Actions.Store =
			    Index + 1 == Batches.size() ? Commands.Color->Actions.Store : EAttachmentStore::Store;
		}
		if (Commands.DepthStencil)
		{
			for (auto* Actions : {&Commands.DepthStencil->Depth, &Commands.DepthStencil->Stencil})
			{
				if (*Actions)
				{
					(*Actions)->Load = Index == 0 ? (*Actions)->Load : EAttachmentLoad::Load;
					(*Actions)->Store = Index + 1 == Batches.size() ? (*Actions)->Store : EAttachmentStore::Store;
				}
			}
		}
		if (Index)
		{
			Commands.Transitions.clear();
		}
		(void)Commands.GetDraws();
		if (!Commands.Color && !Commands.DepthStencil && !Commands.GetDraws().empty())
		{
			throw std::invalid_argument("Draw batch requires declared attachments");
		}
		Result.push_back(std::move(Commands));
	}
	return Result;
}
} // namespace

FRenderGraph::FRenderGraph() : Identity(NextGraphIdentity())
{
}

FRenderGraph::FRenderGraph(const FRenderGraph& InOther) : FRenderGraph()
{
	InOther.CheckMutable();
	Identity = InOther.Identity;
	Resources = InOther.Resources;
	Exports = InOther.Exports;
	Passes = InOther.Passes;
}

FRenderGraph::FRenderGraph(FRenderGraph&& InOther) : FRenderGraph()
{
	*this = std::move(InOther);
}

FRenderGraph& FRenderGraph::operator=(const FRenderGraph& InOther)
{
	CheckMutable();
	InOther.CheckMutable();
	if (this != &InOther)
	{
		auto Copy = InOther;
		*this = std::move(Copy);
	}
	return *this;
}

FRenderGraph& FRenderGraph::operator=(FRenderGraph&& InOther)
{
	CheckMutable();
	InOther.CheckMutable();
	if (this != &InOther)
	{
		Identity = InOther.Identity;
		Resources = std::move(InOther.Resources);
		Exports = std::move(InOther.Exports);
		Passes = std::move(InOther.Passes);
		InOther.Reset();
	}
	return *this;
}

std::size_t FRenderGraph::ResourceIndex(FGraphTexture InTexture) const
{
	if (InTexture.Graph != Identity || InTexture.Index >= Resources.size())
	{
		throw std::invalid_argument("Foreign or stale graph texture handle");
	}
	return InTexture.Index;
}

FGraphTexture FRenderGraph::Import(FGraphTextureImport InResource)
{
	CheckMutable();
	ValidateGraphImport(InResource);
	for (std::size_t Index = 0; Index < Resources.size(); ++Index)
	{
		const auto& Existing = Resources[Index];
		const bool bSameIdentity = InResource.Target.Kind == Existing.Target.Kind &&
		                           (InResource.Target.Kind != ERenderTargetKind::Texture ||
		                            (InResource.Identity ? InResource.Identity == Existing.Identity
		                                                 : InResource.Target.Texture == Existing.Target.Texture));
		if (bSameIdentity)
		{
			if (!SameImport(Existing, InResource))
			{
				throw std::invalid_argument("Conflicting graph import description");
			}
			return {Identity, Index};
		}
	}
	Resources.push_back(std::move(InResource));
	return {Identity, Resources.size() - 1};
}

FGraphTexture FRenderGraph::ImportBackbuffer(FSize InSize)
{
	return Import({"Backbuffer", FRenderTarget::Backbuffer(), InSize, ERHIDepthFormat::None, EResourceState::Present});
}

FGraphTexture FRenderGraph::ImportFrameDepth(ERHIDepthFormat InFormat, FSize InSize)
{
	return Import({"Frame depth", FRenderTarget::FrameDepth(), InSize, InFormat, EResourceState::DepthWrite});
}

void FRenderGraph::Export(FGraphTexture InTexture, EResourceState InState)
{
	CheckMutable();
	ValidateGraphState(Resources[ResourceIndex(InTexture)], InState);
	for (const auto& Existing : Exports)
	{
		if (Existing.first == InTexture)
		{
			if (Existing.second != InState)
			{
				throw std::invalid_argument("Conflicting graph export states");
			}
			return;
		}
	}
	Exports.emplace_back(InTexture, InState);
}

std::size_t FRenderGraph::Add(FGraphicsPass InPass)
{
	CheckMutable();
	Passes.push_back(std::move(InPass));
	return Passes.size() - 1;
}

std::vector<FPassCommands> FRenderGraph::Compile() const
{
	CheckMutable();
	auto Copy = *this;
	auto Result = Copy.CompileAndConsume();
	for (auto& Pass : Result)
	{
		Pass.MaterializeDraws();
	}
	return Result;
}

void FRenderGraph::CheckMutable() const
{
	if (bCompiling)
	{
		throw std::logic_error("Graph declarations cannot change during compilation");
	}
}

void FRenderGraph::Reset()
{
	Passes.clear();
	Resources.clear();
	Exports.clear();
	Identity = NextGraphIdentity();
}

std::vector<FPassCommands> FRenderGraph::CompileAndConsume()
{
	HYP_PERF_SCOPE_C(Render, CompileRenderGraph);
	CheckMutable();

	struct FCompileGuard
	{
		bool& bActive;

		~FCompileGuard()
		{
			bActive = false;
		}
	} Guard{bCompiling};

	bCompiling = true;
	const auto Sequence = Order(); // Validate topology and attachments before running any preparation.
	// Validate content lifetime independently of native resource resolution and deferred callbacks.
	std::vector<FGraphResourceState> Validation;
	for (const auto& Resource : Resources)
	{
		Validation.push_back({Resource.Target,
		                      Resource.InitialState,
		                      {Resource.bInitialized},
		                      {Resource.bInitialized},
		                      {Resource.bInitialized}});
	}
	for (const auto Index : Sequence)
	{
		FPassCommands Unused;
		ApplyGraphPass(Passes[Index], Resources, Validation, Unused);
		FinishGraphAttachments(Passes[Index], Resources, Validation);
	}
	auto States = ResolveGraphResources(Resources);
	std::vector<FPassCommands> Result;
	for (const auto Index : Sequence)
	{
		FPassCommands Commands;
		ApplyGraphPass(Passes[Index], Resources, States, Commands);
		auto Batches = PreparePass(Passes[Index], std::move(Commands));
		Result.insert(Result.end(), std::make_move_iterator(Batches.begin()), std::make_move_iterator(Batches.end()));
		FinishGraphAttachments(Passes[Index], Resources, States);
	}
	FPassCommands Final;
	Final.Name = "Graph exports";
	for (const auto& [Texture, State] : Exports)
	{
		TransitionGraphResource(States[ResourceIndex(Texture)], State, Final);
	}
	if (!Final.Transitions.empty())
	{
		Result.push_back(std::move(Final));
	}
	Reset();
	return Result;
}
} // namespace Hyperion
