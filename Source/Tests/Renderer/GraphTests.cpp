#include "Hyperion/Renderer/RenderGraph.h"
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool InB, const char* InM)
{
	if (!InB)
	{
		throw std::runtime_error(InM);
	}
}

template<class F> void Rejects(F InF)
{
	bool Failed = false;
	try
	{
		InF();
	}
	catch (const std::runtime_error&)
	{
		Failed = true;
	}
	Check(Failed, "Invalid graph accepted");
}

FColorPass Pass(const char* InName, EColorLoad InLoad = EColorLoad::Load)
{
	FColorPass P;
	P.Commands.Name = InName;
	P.Load = InLoad;
	return P;
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		FRenderGraph Valid;
		Valid.Add(Pass("clear", EColorLoad::Clear));
		Valid.Add(Pass("triangle"));
		auto Plan = Valid.Compile();
		Check(Plan.size() == 3 && Plan[0].Clear && Plan[0].TransitionFrom == EResourceState::Present &&
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
