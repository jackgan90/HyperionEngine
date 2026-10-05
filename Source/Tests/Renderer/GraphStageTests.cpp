#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

enum class EFailureStage
{
	Declaration,
	Content,
	Physical,
	Work,
	None
};

struct FStageCounts
{
	unsigned Resolved{};
	unsigned Prepared{};
	unsigned Begun{};
	unsigned Submitted{};
};

class FStageTexture final : public IRHITexture
{
public:
	explicit FStageTexture(bool bInMismatch) : bMismatch(bInMismatch)
	{
	}

	const void* GetDeviceIdentity() const noexcept override
	{
		return this;
	}

	FRHITextureInfo GetInfo() const noexcept override
	{
		return {bMismatch ? 16U : 8U,
		        8,
		        ERHIDepthFormat::None,
		        ERHIColorFormat::R32Float,
		        false,
		        ERHITextureDimension::Texture2D,
		        1,
		        true};
	}

private:
	bool bMismatch;
};

class FStageSwapchain final : public IRHISwapchain
{
public:
	explicit FStageSwapchain(FStageCounts& InCounts) : Counts(InCounts)
	{
		Capabilities.MaxRecordingContexts = 1;
	}

	const FRHICapabilities& GetCapabilities() const noexcept override
	{
		return Capabilities;
	}

	void BeginFrame(FSize) override
	{
		++Counts.Begun;
	}

	FRecordedList Record(std::uint32_t, const FPassCommands& InCommands) override
	{
		HYP_CHECK(InCommands.bCompute && InCommands.Dispatches.size() == 1);
		return {};
	}

	FImage EndFrame(std::span<const FRecordedList>, bool, bool) override
	{
		++Counts.Submitted;
		return {};
	}

	void WaitIdle() override
	{
	}

	void CancelFrame() override
	{
		throw std::runtime_error("Invalid graph acquired a native frame");
	}

private:
	FStageCounts& Counts;
	FRHICapabilities Capabilities;
};

void CheckStage(FTaskSystem& InTasks, EFailureStage InStage)
{
	FStageCounts Counts;
	FStageSwapchain Swapchain(Counts);
	FRenderGraph Graph;
	FGraphTextureImport Import;
	Import.Name = "deferred storage";
	Import.Target = FRenderTarget::FromTexture({});
	Import.Size = {8, 8};
	Import.ColorFormat = ERHIColorFormat::R32Float;
	Import.bStorage = true;
	Import.Identity = std::make_shared<unsigned>(0);
	Import.Resolve = [&]
	{
		++Counts.Resolved;
		return FTexture{std::make_shared<FStageTexture>(InStage == EFailureStage::Physical)};
	};
	const auto Texture = Graph.Import(std::move(Import));
	FComputePass Pass;
	Pass.Name = "producer";
	Pass.Writes = {{Texture, InStage != EFailureStage::Content}};
	Pass.Prepare = [&]
	{
		++Counts.Prepared;
		std::vector<FDispatchPacket> Dispatches(1);
		if (InStage == EFailureStage::Work)
		{
			Dispatches[0].Groups[0] = 0;
		}
		return Dispatches;
	};
	Graph.AddCompute(std::move(Pass));
	if (InStage == EFailureStage::Declaration)
	{
		FComputePass Invalid;
		Invalid.Name = "later invalid declaration";
		Invalid.After = {17};
		Graph.AddCompute(std::move(Invalid));
	}
	bool bRejected{};
	try
	{
		ExecuteGraph(std::move(Graph), InTasks, Swapchain, {8, 8}, false, false);
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected == (InStage != EFailureStage::None));
	HYP_CHECK(Counts.Resolved ==
	          (InStage == EFailureStage::Declaration || InStage == EFailureStage::Content ? 0U : 1U));
	HYP_CHECK(Counts.Prepared == (InStage == EFailureStage::Work || InStage == EFailureStage::None ? 1U : 0U));
	HYP_CHECK(Counts.Begun == (InStage == EFailureStage::None ? 1U : 0U));
	HYP_CHECK(Counts.Submitted == Counts.Begun);
}
} // namespace

void CheckGraphStages()
{
	FTaskSystem Tasks(1, 1);
	for (const auto Stage : {EFailureStage::Declaration, EFailureStage::Content, EFailureStage::Physical,
	                         EFailureStage::Work, EFailureStage::None})
	{
		CheckStage(Tasks, Stage);
	}
}
