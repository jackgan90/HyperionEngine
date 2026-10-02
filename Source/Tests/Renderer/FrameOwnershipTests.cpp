#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Renderer/FramePipeline.h"
#include "Renderer/InstanceBatchSupport.h"
#include <array>
#include <future>
#include <iostream>

using namespace Hyperion;

void RunViewPolicyOwnershipTests(FTaskSystem& InTasks, IRHIDevice& InDevice, IRHISwapchain& InSwapchain);

namespace
{
struct FRelease
{
	std::promise<void> Promise;
	bool bReleased{};

	void Open()
	{
		if (!bReleased)
		{
			Promise.set_value();
			bReleased = true;
		}
	}

	~FRelease()
	{
		Open();
	}
};

struct FFrameResults
{
	std::array<FForwardFrame, 2> Prepared;
	std::array<FForwardPipelineStatistics, 2> Statistics;
	std::array<FImage, 2> Images;
};

void CheckLimitPixels(InstanceTests::FFixture& InFixture)
{
	FImage Reference;
	for (const auto Limits : {FFramePipelineLimits{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 3}})
	{
		FForwardRenderPipeline Forward(*InFixture.Session);
		FFramePipeline Pipeline(InFixture.Tasks, Limits);
		std::array<FImage, 8> Images;
		for (std::size_t Index = 0; Index < Images.size(); ++Index)
		{
			if (Index == 4)
			{
				Pipeline.Skip();
			}
			const auto Frame = InFixture.Session->FreezeFrame(0);
			const auto View = InFixture.View;
			Pipeline.Submit(
			    [Fixture = &InFixture, Forward = &Forward, Frame, View, Index, Images = &Images]
			    {
				    FRenderGraph Graph;
				    FCascadedShadowSettings Shadows;
				    Shadows.bEnabled = false;
				    Forward->Build(Graph, View, Frame, Shadows, {.1f, .1f, .1f, 1}, {}, true);
				    return [Fixture, Index, Images, Graph = std::move(Graph)]() mutable
				    {
					    (*Images)[Index] = ExecuteGraphOnRhi(std::move(Graph), Fixture->Tasks, *Fixture->Swapchain,
					                                         {128, 96}, false, true);
				    };
			    });
		}
		Pipeline.Drain();
		if (Reference.Rgba.empty())
		{
			Reference = Images.front();
		}
		for (const auto& Image : Images)
		{
			HYP_CHECK(Image.Rgba == Reference.Rgba && Image.Width == 128);
		}
		HYP_CHECK(Pipeline.Progress().Submitted == 9);
	}
}

void CheckQueuedRemoval(InstanceTests::FFixture& InFixture)
{
	FForwardRenderPipeline Forward(*InFixture.Session);
	FFramePipeline Pipeline(InFixture.Tasks, {3, 3});
	// This releases before Pipeline's destructor if an assertion throws.
	FRelease Release;
	const auto Gate = Release.Promise.get_future().share();
	auto Results = std::make_shared<FFrameResults>();
	std::array<FFrameTicket, 2> Tickets;
	for (std::size_t Index = 0; Index < 2; ++Index)
	{
		const auto Frame = InFixture.Session->FreezeFrame(float(Index));
		auto View = InFixture.View;
		View.bInstanceBatching = false;
		View.Revision = Index + 1;
		Tickets[Index] = Pipeline.Submit(
		    [Fixture = &InFixture, Forward = &Forward, Frame, View, Index, Results, Gate]
		    {
			    FRenderGraph Graph;
			    FCascadedShadowSettings Shadows;
			    Shadows.bEnabled = false;
			    Forward->Build(Graph, View, Frame, Shadows, Index ? FVec4{0, 0, .6f, 1} : FVec4{.3f, 0, 0, 1}, {},
			                   true);
			    Results->Prepared[Index] = Forward->GetFrame();
			    return [Fixture, Index, Results, Gate, Graph = std::move(Graph)]() mutable
			    {
				    if (Index == 0)
				    {
					    Gate.get();
				    }
				    Results->Images[Index] = ExecuteGraphOnRhi(std::move(Graph), Fixture->Tasks, *Fixture->Swapchain,
				                                               {128, 96}, false, true);
				    Results->Statistics[Index] = Results->Prepared[Index].Statistics();
			    };
		    });
		// Both graphs build before the first graph can prepare resources or record.
		InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Render},
		                                              []
		                                              {
		                                              }));
		if (Index == 0)
		{
			InFixture.Binding.Remove();
		}
	}
	HYP_CHECK(!Tickets[0].Ready() && !Tickets[1].Ready());
	bool bRejected{};
	try
	{
		Results->Prepared[0].Statistics();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
	Release.Open();
	Pipeline.Drain();
	HYP_CHECK(Results->Statistics[0].Views.back().Visibility.VisibleItems == 8);
	HYP_CHECK(Results->Statistics[0].Views.back().Visibility.Draws == 8);
	HYP_CHECK(Results->Statistics[1].Views.back().Visibility.VisibleItems == 0);
	HYP_CHECK(Results->Statistics[1].Views.back().Visibility.Draws == 0);
	HYP_CHECK(Results->Prepared[0].Statistics().Views.back().Visibility.Draws == 8);
	const auto& First = Results->Images[0];
	const auto& Second = Results->Images[1];
	HYP_CHECK(First.Width == 128 && Second.Width == 128);
	HYP_CHECK(First.Rgba[0] > .25f && First.Rgba[2] < .01f);
	HYP_CHECK(Second.Rgba[0] < .01f && Second.Rgba[2] > .5f);
	InFixture.Tasks.Wait(InFixture.Tasks.Dispatch({EDomain::Rhi, 0},
	                                              [Device = InFixture.Device.get()]
	                                              {
		                                              Device->WaitIdle();
		                                              HYP_CHECK(Device->Statistics().ValidationErrors == 0);
	                                              }));
}
} // namespace

int main()
{
	try
	{
		InstanceTests::FFixture Fixture;
		CheckLimitPixels(Fixture);
		CheckQueuedRemoval(Fixture);
		RunViewPolicyOwnershipTests(Fixture.Tasks, *Fixture.Device, *Fixture.Swapchain);
		std::cout << "Queued frame ownership and removal passed\n";
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
