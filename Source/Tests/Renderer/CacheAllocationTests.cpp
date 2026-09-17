#include "Renderer/InstanceBatchSupport.h"
#include "Support/DispatchAllocationFailure.h"
#include <iostream>

using namespace Hyperion;
using namespace Hyperion::InstanceTests;

namespace
{
void AdvanceGeneration(FRenderSceneSnapshot& InSnapshot)
{
	for (auto& Item : InSnapshot.Items)
	{
		++Item.Primitive.Generation;
	}
}

void CheckPlanningFailure(FFixture& InFixture, const char* InFunction, const char* InEntry)
{
	std::cout << "Injecting " << InFunction << std::endl;
	auto Source = Snapshot(InFixture, 4);
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {4, 1, 1024 * 1024});
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Render},
	    [&]
	    {
		    {
			    Tests::FDispatchAllocationFailure Failure(0, InFunction, InEntry);
			    try
			    {
				    (void)Batches.Build(Source);
			    }
			    catch (const std::bad_alloc&)
			    {
			    }
			    HYP_CHECK(Failure.WasInjected());
		    }
		    for (unsigned Index = 0; Index < 4; ++Index)
		    {
			    AdvanceGeneration(Source);
			    const auto Plan = Batches.Build(Source);
			    HYP_CHECK(Plan->Batches.size() == 1 && Plan->Batches.front().Instances &&
			              Plan->Batches.front().Instances->InstanceCount == 4);
			    HYP_CHECK(Plan->Statistics.CachedChunks <= 1 && Plan->Statistics.CachedBytes <= 1024 * 1024);
		    }
		    Batches.Clear();
	    }));
}

void CheckConstantFailure(FFixture& InFixture)
{
	std::cout << "Injecting FMaterialConstantCache::BindInstances" << std::endl;
	auto Source = Snapshot(InFixture, 4);
	FRenderBatchSystem Batches(InFixture.Tasks, InFixture.Device->GetCapabilities(), {4, 1, 1024 * 1024});
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Source.Batches = Batches.Build(Source);
	                                              }));
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&]
	    {
		    const auto Before = InFixture.Session->GetResources().Statistics().Constants;
		    {
			    Tests::FDispatchAllocationFailure Failure(0, "Hyperion::FMaterialConstantCache::BindInstances",
			                                              "FMaterialConstantCache::FImpl::FInstanceEntry");
			    (void)InFixture.Session->GetResources().BuildDraws(Source);
			    HYP_CHECK(Failure.WasInjected());
		    }
		    const auto Failed = InFixture.Session->GetResources().Statistics().Constants;
		    HYP_CHECK(Failed.InstanceBlocks == Before.InstanceBlocks && Failed.InstanceBytes == Before.InstanceBytes);
	    }));
	// Force the default 512-block GPU LRU to evict after recovery, retaining old commands independently.
	const auto Retained = Prepare(InFixture, Batches, Source);
	const auto Pixels = InFixture.Draw(Retained);
	for (unsigned Index = 0; Index < 514; ++Index)
	{
		AdvanceGeneration(Source);
		(void)Prepare(InFixture, Batches, Source);
	}
	HYP_CHECK(InFixture.Session->GetResources().Statistics().Constants.InstanceBlocks <= 512);
	HYP_CHECK(InFixture.Draw(Retained).Rgba == Pixels.Rgba);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
	                                              [&]
	                                              {
		                                              Batches.Clear();
	                                              }));
}
} // namespace

int main()
{
	try
	{
		FFixture Fixture;
		CheckPlanningFailure(Fixture, "Hyperion::FInstanceDataCache::FImpl::Record",
		                     "FInstanceDataCache::FImpl::FRecord");
		CheckPlanningFailure(Fixture, "Hyperion::FInstanceDataCache::FImpl::Block",
		                     "FInstanceDataCache::FImpl::FBlock");
		CheckPlanningFailure(Fixture, "Hyperion::FRenderBatchSystem::FImpl::Describe",
		                     "FRenderBatchSystem::FImpl::FItemEntry");
		CheckPlanningFailure(Fixture, "Hyperion::FRenderBatchSystem::FImpl::Data",
		                     "FRenderBatchSystem::FImpl::FChunkEntry");
		CheckConstantFailure(Fixture);
		HYP_CHECK(Fixture.Device->Statistics().ValidationErrors == 0);
		std::cout << "Five cache insertion failures, retry, eviction and retained GPU frame passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
