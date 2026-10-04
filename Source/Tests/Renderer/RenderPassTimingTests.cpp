#include "Hyperion/Renderer/RenderGraph.h"
#include "Hyperion/Renderer/RenderPass.h"
#include "Support/TestSupport.h"
#include <algorithm>

using namespace Hyperion;

namespace
{
void CheckTimingIdentity()
{
	HYP_CHECK(EncodeRenderPassTiming({}) == FGpuTimingTag{});
	HYP_CHECK(!DecodeRenderPassTiming({}));
	const FRenderPassTiming Largest{ERenderPassTimingCategory::Shadow, UINT32_MAX};
	HYP_CHECK(DecodeRenderPassTiming(EncodeRenderPassTiming(Largest)) == Largest);
	auto Foreign = EncodeRenderPassTiming(Largest);
	++Foreign.Domain;
	HYP_CHECK(!DecodeRenderPassTiming(Foreign));
	auto Unknown = EncodeRenderPassTiming(Largest);
	Unknown.Value = UINT64_MAX;
	HYP_CHECK(!DecodeRenderPassTiming(Unknown));
	for (const auto Invalid : {ERenderPassTimingCategory::Count, static_cast<ERenderPassTimingCategory>(UINT32_MAX)})
	{
		bool bRejected = false;
		try
		{
			EncodeRenderPassTiming({Invalid});
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected);
	}
	FRenderPassTargets Targets;
	auto Changed = Targets;
	Changed.Timing = Largest;
	HYP_CHECK(Targets != Changed);
}

void CheckTimingAggregation()
{
	auto Foreign = EncodeRenderPassTiming({ERenderPassTimingCategory::Forward});
	++Foreign.Domain;
	auto Unknown = EncodeRenderPassTiming({ERenderPassTimingCategory::Forward});
	Unknown.Value = UINT64_MAX;
	FGpuFrameTiming Frame;
	Frame.Passes = {{"not a shadow name", 1, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 0})},
	                {"second split batch", 2, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 0})},
	                {"Shadow cascade 0/0", 4, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 3})},
	                {"Shadow cascade 2/0", 8, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, UINT32_MAX})},
	                {"Forward/HDR/0", 16},
	                {"Shadow cascade 1/0", 32, Foreign},
	                {"Deferred/Lighting/0", 64, Unknown},
	                {"unrelated label", 128, EncodeRenderPassTiming({ERenderPassTimingCategory::Forward})}};
	const auto Sum = AggregateRenderGpuTimings(Frame);
	HYP_CHECK(Sum.Total == 255 && Sum.Shadow == 15 && Sum.Forward == 128);
	HYP_CHECK((Sum.Cascades == std::array<double, 4>{3, 0, 0, 4}));
	HYP_CHECK(Sum.Lighting == 0 && Sum.HierarchicalDepth == 0 && Sum.LocalLights == 0);
	std::reverse(Frame.Passes.begin(), Frame.Passes.end());
	const auto Reordered = AggregateRenderGpuTimings(Frame);
	HYP_CHECK(Reordered.Total == Sum.Total && Reordered.Cascades == Sum.Cascades);
	HYP_CHECK(AggregateRenderGpuTimings({}).Total == 0);
}

void CheckTimingGraph(bool bInDeferred)
{
	FRenderGraph Graph;
	FGraphicsPass Graphics;
	Graphics.Name = "Custom clear";
	Graphics.Timing = {ERenderPassTimingCategory::Shadow, 2};
	Graphics.Color = FGraphColorAttachment{Graph.ImportBackbuffer(), {EAttachmentLoad::Clear}};
	if (bInDeferred)
	{
		Graphics.Prepare = []
		{
			return std::vector<FGraphicsDrawBatch>(3);
		};
	}
	else
	{
		Graphics.Batches.resize(3);
	}
	const auto GraphicsTag = EncodeRenderPassTiming(Graphics.Timing);
	const auto GraphicsIndex = Graph.Add(std::move(Graphics));
	FComputePass Compute;
	Compute.Name = "Forward/misleading compute";
	Compute.Timing = {ERenderPassTimingCategory::HierarchicalDepth, 7};
	Compute.After = {GraphicsIndex};
	if (bInDeferred)
	{
		Compute.Prepare = []
		{
			return std::vector<FDispatchPacket>{};
		};
	}
	const auto ComputeTag = EncodeRenderPassTiming(Compute.Timing);
	Graph.AddCompute(std::move(Compute));
	FGraphicsPass Unclassified;
	Unclassified.Name = "Shadow cascade 0";
	Graph.Add(std::move(Unclassified));
	const auto Copy = Graph.Compile();
	const auto Consumed = Graph.CompileAndConsume();
	HYP_CHECK(Copy.size() == 5 && Consumed.size() == 5);
	for (std::size_t Index = 0; Index < Copy.size(); ++Index)
	{
		const auto Expected = Index < 3 ? GraphicsTag : Index == 3 ? ComputeTag : FGpuTimingTag{};
		HYP_CHECK(Copy[Index].TimingTag == Expected && Consumed[Index].TimingTag == Expected);
		HYP_CHECK(Copy[Index].Name == Consumed[Index].Name);
	}
}
} // namespace

void CheckRenderPassTiming()
{
	CheckTimingIdentity();
	CheckTimingAggregation();
	CheckTimingGraph(false);
	CheckTimingGraph(true);
}
