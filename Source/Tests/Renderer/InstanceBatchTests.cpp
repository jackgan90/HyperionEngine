#include "Renderer/InstanceBatchSupport.h"
#include <iostream>

using namespace Hyperion;
using namespace Hyperion::InstanceTests;

namespace
{
void CheckPixels(FFixture& InFixture)
{
	const auto Ordinary = InFixture.Build(false);
	HYP_CHECK(DrawCount(Ordinary) == 8 && InstanceCount(Ordinary) == 8);
	const auto Baseline = InFixture.Draw(Ordinary);
	const auto Batched = InFixture.Build();
	HYP_CHECK(DrawCount(Batched) == 2 && InstanceCount(Batched) == 8);
	HYP_CHECK(InFixture.Statistics.Batches.InstancedItems == 8);
	HYP_CHECK(InFixture.Draw(Batched).Rgba == Baseline.Rgba);
	InFixture.Emission->Count = 9;
	const auto Split = InFixture.Build();
	HYP_CHECK(DrawCount(Split) == 3 && InstanceCount(Split) == 9);
	HYP_CHECK(InFixture.Statistics.Batches.InstancedItems == 8 && InFixture.Statistics.Batches.SingleDraws == 1);
	InFixture.Emission->Count = 8;
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		RunInstanceContractTests(Fixture.Compiler);
		CheckPixels(Fixture);
		RunInstancePlanningTests(Fixture);
		RunInstanceResourceTests(Fixture);
		RunInstanceCacheTests(Fixture);
		RunInstanceFailureTests(Fixture);
		HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		std::cout << "PASS: instance permutations, typed records, batching, cache, coverage and GPU validation\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
