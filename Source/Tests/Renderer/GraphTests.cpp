#include "Support/GraphTestSupport.h"
#include "Support/TestSupport.h"
#include <iostream>

using namespace Hyperion;

namespace
{
template<class Callable> void Rejects(const Callable& InAction)
{
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		return;
	}
	throw std::runtime_error("Invalid graph accepted");
}

class FTestTexture final : public IRHITexture
{
public:
	FRHITextureInfo GetInfo() const noexcept override
	{
		return {64, 64, ERHIDepthFormat::D32};
	}

	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}
};

FGraphTexture Depth(FRenderGraph& InGraph, bool bInInitialized = false)
{
	return InGraph.Import({"depth",
	                       FRenderTarget::FromTexture({std::make_shared<FTestTexture>()}),
	                       {64, 64},
	                       ERHIDepthFormat::D32,
	                       EResourceState::ShaderRead,
	                       bInInitialized});
}

FGraphicsPass DepthPass(FGraphTexture InTexture, std::string InName, EAttachmentLoad InLoad)
{
	FGraphicsPass Pass;
	Pass.Name = std::move(InName);
	Pass.DepthStencil = FGraphDepthStencilAttachment{InTexture, FAttachmentActions{InLoad}};
	return Pass;
}

void CheckPacketOwnership()
{
	FRenderGraph Graph;
	auto Source = MakeColorPass(Graph, "owned", EAttachmentLoad::Clear);
	auto& Draws = Source.Batches[0].Commands.Draws;
	Draws.resize(8);
	Draws[0].IndexCount = 17;
	const auto Storage = Draws.data();
	Graph.Add(std::move(Source));
	auto Borrowed = Graph.Compile();
	HYP_CHECK(Borrowed[0].Draws.data() != Storage);
	Borrowed[0].Draws[0].IndexCount = 23;
	auto Owned = Graph.CompileAndConsume();
	HYP_CHECK(Owned[0].Draws.data() == Storage && Owned[0].Draws[0].IndexCount == 17);
	Graph.Add(MakeColorPass(Graph, "reuse", EAttachmentLoad::Clear));
	HYP_CHECK(Graph.Compile().size() == 2 && Owned[0].Draws[0].IndexCount == 17);
	Graph = {};
	Source = MakeColorPass(Graph, "shared", EAttachmentLoad::Clear);
	Source.Batches[0].Commands.Draws.resize(3);
	Source.Batches[0].Commands.Draws[0].IndexCount = 9;
	Source.Batches[0].Commands.ShareDraws();
	const auto Shared = Source.Batches[0].Commands.SharedDraws;
	Graph.Add(std::move(Source));
	Borrowed = Graph.Compile();
	HYP_CHECK(!Borrowed[0].SharedDraws && Borrowed[0].Draws[0].IndexCount == 9);
	Borrowed[0].Draws[0].IndexCount = 12;
	Owned = Graph.CompileAndConsume();
	HYP_CHECK(Owned[0].SharedDraws == Shared && Owned[0].GetDraws()[0].IndexCount == 9);
}

void CheckDeferredPreparation()
{
	FRenderGraph Graph;
	unsigned Prepared{};
	auto Deferred = MakeColorPass(Graph, "deferred", EAttachmentLoad::Clear);
	Deferred.Color->View = EGraphColorView::DrawBatch;
	Deferred.Color->Actions.Store = EAttachmentStore::Discard;
	Deferred.Batches.clear();
	Deferred.Prepare = [&]
	{
		++Prepared;
		std::vector<FGraphicsDrawBatch> Batches(2);
		Batches[1].bSrgb = true;
		return Batches;
	};
	Graph.Add(Deferred);
	HYP_CHECK(Prepared == 0);
	const auto Plan = Graph.Compile();
	HYP_CHECK(Prepared == 1 && Plan.size() == 3);
	HYP_CHECK(Plan[0].Color->Actions.Load == EAttachmentLoad::Clear);
	HYP_CHECK(Plan[0].Color->Actions.Store == EAttachmentStore::Store);
	HYP_CHECK(Plan[1].Color->Actions.Load == EAttachmentLoad::Load && Plan[1].IsSrgb());
	HYP_CHECK(Plan[1].Color->Actions.Store == EAttachmentStore::Discard);
	Graph.Add(MakeColorPass(Graph, "undefined"));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	HYP_CHECK(Prepared == 1); // Content errors must precede any resource or draw preparation.
	Graph = {};
	Deferred = MakeColorPass(Graph, "cycle", EAttachmentLoad::Clear);
	Deferred.After = {1};
	Deferred.Batches.clear();
	Deferred.Prepare = [&]
	{
		++Prepared;
		return std::vector<FGraphicsDrawBatch>{};
	};
	Graph.Add(Deferred);
	Graph.Add(MakeColorPass(Graph, "later"));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	HYP_CHECK(Prepared == 1);
}

void CheckResourceHazards()
{
	FRenderGraph Graph;
	const auto Texture = Depth(Graph);
	Graph.Add(DepthPass(Texture, "producer", EAttachmentLoad::Clear));
	auto Read = MakeColorPass(Graph, "consumer", EAttachmentLoad::Clear);
	Read.Reads = {Texture};
	Graph.Add(Read);
	Graph.Export(Texture, EResourceState::ShaderRead);
	auto Plan = Graph.Compile();
	HYP_CHECK(!Plan[0].HasColor() && Plan[0].HasDepth());
	HYP_CHECK(Plan[0].Transitions[0].After == EResourceState::DepthWrite);
	HYP_CHECK(Plan[1].SampledTextures.size() == 1);
	HYP_CHECK(Plan[1].Transitions.back().After == EResourceState::ShaderRead);
	HYP_CHECK(Plan.back().Transitions[0].After == EResourceState::Present);
	// WAR: after a read, a later write must stay behind it even if extra edges try to reverse them.
	auto Write = DepthPass(Texture, "rewrite", EAttachmentLoad::Clear);
	Graph.Add(Write);
	Read.Name = "cyclic read";
	Read.After = {4};
	Graph.Add(Read);
	Write.Name = "last write";
	Graph.Add(Write);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	// Independent targets can be explicitly reordered without an artificial chain.
	Graph = {};
	const auto A = Depth(Graph);
	const auto B = Depth(Graph);
	auto First = DepthPass(A, "first", EAttachmentLoad::Clear);
	First.After = {1};
	Graph.Add(First);
	Graph.Add(DepthPass(B, "second", EAttachmentLoad::Clear));
	Plan = Graph.Compile();
	HYP_CHECK(Plan[0].Name == "second/0" && Plan[1].Name == "first/0");
}

void CheckContents()
{
	FRenderGraph Graph;
	const auto Texture = Depth(Graph);
	auto Producer = DepthPass(Texture, "left", EAttachmentLoad::Clear);
	Producer.Viewport = FViewport{0, 0, 32, 64};
	Graph.Add(Producer);
	auto Read = MakeColorPass(Graph, "read", EAttachmentLoad::Clear);
	Read.Reads = {Texture};
	auto Invalid = Graph;
	Invalid.Add(Read);
	Rejects(
	    [&]
	    {
		    Invalid.Compile();
	    });
	Producer.Name = "right";
	Producer.Viewport = FViewport{32, 0, 32, 64};
	Graph.Add(Producer);
	Graph.Add(Read);
	HYP_CHECK(Graph.Compile().size() == 4); // Both halves prove whole-resource initialization.
	Producer.Name = "discard right";
	Producer.DepthStencil->Depth->Load = EAttachmentLoad::Load;
	Producer.DepthStencil->Depth->Store = EAttachmentStore::Discard;
	Graph.Add(Producer);
	auto LoadLeft = DepthPass(Texture, "load left", EAttachmentLoad::Load);
	LoadLeft.Viewport = FViewport{0, 0, 32, 64};
	Graph.Add(LoadLeft);
	HYP_CHECK(!Graph.Compile().empty()); // Discarding the right half preserves the left.
	Read.Name = "discarded read";
	Graph.Add(Read);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	auto Discard = MakeColorPass(Graph, "discard", EAttachmentLoad::Discard);
	Graph.Add(Discard);
	Graph.Add(MakeColorPass(Graph, "undefined load"));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}

void CheckFractionalRegions()
{
	FRenderGraph Graph;
	const auto Color = Graph.ImportBackbuffer({64, 64});
	FGraphicsPass Pass;
	Pass.Name = "Clear all";
	Pass.Color = FGraphColorAttachment{Color, {EAttachmentLoad::Clear}};
	Graph.Add(Pass);
	Pass.Name = "Discard fractional area";
	Pass.Color->Actions = {EAttachmentLoad::Discard, EAttachmentStore::Discard};
	Pass.Viewport = FViewport{.5f, 0, 31, 64};
	Graph.Add(Pass);
	Pass.Name = "Load discarded edge pixel";
	Pass.Color->Actions = {EAttachmentLoad::Load};
	Pass.Viewport = FViewport{0, 0, .25f, 64};
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	auto Invalid = MakeColorPass(Graph, "Invalid viewport", EAttachmentLoad::Clear);
	Invalid.Viewport = FViewport{0, 0, -1, 64};
	Graph.Add(Invalid);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
}

void CheckCompilationMutation()
{
	FRenderGraph Graph;
	auto Pass = MakeColorPass(Graph, "mutation", EAttachmentLoad::Clear);
	Pass.Batches.clear();
	Pass.Prepare = [&]() -> std::vector<FGraphicsDrawBatch>
	{
		Rejects(
		    [&]
		    {
			    Graph.Add({});
		    });
		Rejects(
		    [&]
		    {
			    Graph.ImportBackbuffer();
		    });
		Rejects(
		    [&]
		    {
			    Graph.CompileAndConsume();
		    });
		FRenderGraph Other;
		Rejects(
		    [&]
		    {
			    Graph = Other;
		    });
		Rejects(
		    [&]
		    {
			    Graph = FRenderGraph{};
		    });
		Rejects(
		    [&]
		    {
			    Other = Graph;
		    });
		Rejects(
		    [&]
		    {
			    Other = std::move(Graph);
		    });
		Rejects(
		    [&]
		    {
			    FRenderGraph Copy(Graph);
		    });
		Rejects(
		    [&]
		    {
			    FRenderGraph Moved(std::move(Graph));
		    });
		return {};
	};
	Graph.Add(std::move(Pass));
	HYP_CHECK(Graph.CompileAndConsume().size() == 2);
	Graph.Add(MakeColorPass(Graph, "again", EAttachmentLoad::Clear));
	HYP_CHECK(Graph.CompileAndConsume().size() == 2);
}

void CheckPhysicalImports()
{
	for (const bool bDeferred : {false, true})
	{
		FRenderGraph Graph;
		const FTexture Native{std::make_shared<FTestTexture>()};
		FGraphTextureImport Import{"mismatched physical extent",
		                           FRenderTarget::FromTexture(Native),
		                           {32, 64},
		                           ERHIDepthFormat::D32,
		                           EResourceState::ShaderRead};
		if (bDeferred)
		{
			Import.Target.Texture = {};
			Import.Identity = Native.Payload;
			Import.Resolve = [Native]
			{
				return Native;
			};
		}
		auto Pass = DepthPass(Graph.Import(Import), "clear declared extent", EAttachmentLoad::Clear);
		bool bPrepared = false;
		Pass.Prepare = [&]
		{
			bPrepared = true;
			return std::vector<FGraphicsDrawBatch>{};
		};
		Graph.Add(Pass);
		Rejects(
		    [&]
		    {
			    Graph.CompileAndConsume();
		    });
		HYP_CHECK(!bPrepared);
	}
}

void CheckGraphMoves()
{
	FRenderGraph Original;
	const auto Color = Original.ImportBackbuffer();
	Original.Add(MakeColorPass(Original, "move", EAttachmentLoad::Clear));
	FRenderGraph Moved(std::move(Original));
	HYP_CHECK(Moved.Compile().size() == 2);
	HYP_CHECK(Original.ImportBackbuffer().Graph != Color.Graph);
	FRenderGraph Assigned;
	Assigned = std::move(Moved);
	HYP_CHECK(Assigned.CompileAndConsume().size() == 2);
	HYP_CHECK(Moved.ImportBackbuffer().Graph != Color.Graph);
}

void CheckInvalidDeclarations()
{
	FRenderGraph Graph;
	Graph.Add(MakeColorPass(Graph, "undefined"));
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	FRenderGraph Foreign;
	auto Wrong = MakeColorPass(Foreign, "foreign", EAttachmentLoad::Clear);
	Graph.Add(Wrong);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	auto Pass = MakeColorPass(Graph, "same", EAttachmentLoad::Clear);
	Graph.Add(Pass);
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Pass = MakeColorPass(Graph, "unknown", EAttachmentLoad::Clear);
	Pass.After = {9};
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	const auto Texture = Depth(Graph);
	Pass = DepthPass(Texture, "feedback", EAttachmentLoad::Clear);
	Pass.Reads = {Texture};
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Pass = MakeColorPass(Graph, "stale", EAttachmentLoad::Clear);
	Graph.Add(Pass);
	Graph.CompileAndConsume();
	Graph.Add(Pass);
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	Graph = {};
	Rejects(
	    [&]
	    {
		    Graph.Compile();
	    });
	const auto Color = Graph.ImportBackbuffer({64, 64});
	Rejects(
	    [&]
	    {
		    Graph.ImportBackbuffer({32, 64});
	    });
	Rejects(
	    [&]
	    {
		    Graph.Export(Color, EResourceState::DepthWrite);
	    });
}
} // namespace

int main()
{
	try
	{
		CheckPacketOwnership();
		CheckDeferredPreparation();
		CheckResourceHazards();
		CheckContents();
		CheckInvalidDeclarations();
		CheckCompilationMutation();
		CheckPhysicalImports();
		CheckGraphMoves();
		CheckFractionalRegions();
		std::cout << "Explicit graph attachments, hazards, content lifetime and packet ownership passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
