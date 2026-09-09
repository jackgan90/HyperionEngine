#include "Hyperion/Renderer/RenderGraph.h"
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool bInB, const char* InM)
{
	if (!bInB)
	{
		throw std::runtime_error(InM);
	}
}

template<class F> void Rejects(F InF)
{
	bool bFailed = false;
	try
	{
		InF();
	}
	catch (const std::runtime_error&)
	{
		bFailed = true;
	}
	Check(bFailed, "Invalid graph accepted");
}

FColorPass Pass(const char* InName, EColorLoad InLoad = EColorLoad::Load)
{
	FColorPass P;
	P.Commands.Name = InName;
	P.Load = InLoad;
	return P;
}

void CheckPacketOwnership()
{
	FRenderGraph Graph;
	auto Source = Pass("owned packets", EColorLoad::Clear);
	Source.Commands.Draws.resize(8);
	Source.Commands.Draws.front().IndexCount = 17;
	const auto Storage = Source.Commands.Draws.data();
	Graph.Add(std::move(Source));
	auto Borrowed = Graph.Compile();
	Check(Borrowed.front().Draws.data() != Storage, "Borrowed compilation must snapshot packets");
	Borrowed.front().Draws.front().IndexCount = 23;
	auto Owned = Graph.CompileAndConsume();
	Check(Owned.front().Draws.data() == Storage, "Consuming compilation should transfer packet storage");
	Check(Owned.front().Draws.front().IndexCount == 17, "Compilation copies must remain independent");
	Graph.Add(Pass("reused graph", EColorLoad::Clear));
	Check(Graph.Compile().size() == 2, "Consumed graph should be reusable");
	Check(Owned.front().Draws.front().IndexCount == 17, "Compiled packets must outlive the graph");
}

void CheckDeferredPreparation()
{
	FRenderGraph Graph;
	Graph.Add(Pass("clear", EColorLoad::Clear));
	unsigned Prepared{};
	Graph.AddDeferred(
	    [&]
	    {
		    ++Prepared;
		    auto Second = Pass("second");
		    Second.After = {0};
		    return std::vector{Pass("first"), Second};
	    },
	    {0});
	Graph.AddDeferred(
	    []
	    {
		    return std::vector<FColorPass>{};
	    },
	    {1});
	auto Overlay = Pass("overlay");
	Overlay.After = {1, 2};
	Graph.Add(Overlay);
	Check(Prepared == 0, "Deferred work ran before graph compilation");
	const auto Plan = Graph.CompileAndConsume();
	Check(Prepared == 1 && Plan.size() == 5 && Plan[1].Name == "first" && Plan[2].Name == "second" &&
	          Plan[3].Name == "overlay",
	      "Deferred expansion order/dependency remapping");
	FRenderGraph Cycle;
	Cycle.AddDeferred(
	    [&]
	    {
		    ++Prepared;
		    return std::vector{Pass("bad", EColorLoad::Clear)};
	    },
	    {1});
	Cycle.Add(Pass("later"));
	Rejects(
	    [&]
	    {
		    Cycle.Compile();
	    });
	Check(Prepared == 1, "Invalid outer dependency must fail before deferred preparation");
	FRenderGraph InnerCycle;
	InnerCycle.AddDeferred(
	    []
	    {
		    auto First = Pass("bad first", EColorLoad::Clear);
		    First.After = {1};
		    return std::vector{First, Pass("bad second")};
	    });
	Rejects(
	    [&]
	    {
		    InnerCycle.Compile();
	    });
}

void CheckSharedPackets()
{
	FRenderGraph Graph;
	auto Source = Pass("shared", EColorLoad::Clear);
	Source.Commands.Draws.resize(3);
	Source.Commands.Draws[0].IndexCount = 9;
	Source.Commands.ShareDraws();
	const auto Storage = Source.Commands.SharedDraws;
	Graph.Add(std::move(Source));
	auto Copy = Graph.Compile();
	Check(!Copy[0].SharedDraws && Copy[0].Draws[0].IndexCount == 9, "Borrowed compile materializes shared packets");
	Copy[0].Draws[0].IndexCount = 12;
	const auto Owned = Graph.CompileAndConsume();
	Check(Owned[0].SharedDraws == Storage && Owned[0].GetDraws()[0].IndexCount == 9,
	      "Owned compile preserves immutable storage and independent borrowed edits");
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		CheckPacketOwnership();
		CheckDeferredPreparation();
		CheckSharedPackets();
		FRenderGraph Valid;
		Valid.Add(Pass("clear", EColorLoad::Clear));
		Valid.Add(Pass("triangle"));
		auto Plan = Valid.Compile();
		Check(Plan.size() == 3 && Plan[0].bClear && Plan[0].TransitionFrom == EResourceState::Present &&
		          Plan[2].TransitionTo == EResourceState::Present,
		      "Graph transitions");
		FRenderGraph Undefined;
		Undefined.Add(Pass("load"));
		Rejects(
		    [&]
		    {
			    Undefined.Compile();
		    });
		FRenderGraph Cycle;
		auto First = Pass("first", EColorLoad::Clear);
		First.After = {1};
		Cycle.Add(First);
		Cycle.Add(Pass("second"));
		Rejects(
		    [&]
		    {
			    Cycle.Compile();
		    });
		FRenderGraph Duplicate;
		Duplicate.Add(Pass("same", EColorLoad::Clear));
		Duplicate.Add(Pass("same"));
		Rejects(
		    [&]
		    {
			    Duplicate.Compile();
		    });
		FRenderGraph Unknown;
		auto Bad = Pass("bad", EColorLoad::Clear);
		Bad.After = {9};
		Unknown.Add(Bad);
		Rejects(
		    [&]
		    {
			    Unknown.Compile();
		    });
		FRenderGraph Empty;
		Rejects(
		    [&]
		    {
			    Empty.Compile();
		    });
		std::cout << "Graph validation passed\n";
		return 0;
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
