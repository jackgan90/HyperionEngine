#include "Hyperion/Renderer/RenderPipelineFrame.h"
#include "Hyperion/Renderer/FullscreenPass.h"

namespace Hyperion
{
FSceneVisibilityStats FForwardPipelineStatistics::MainView() const
{
	FSceneVisibilityStats Result;
	bool bFirst = true;
	for (const auto& View : Views)
	{
		if (View.Usage == "ShadowDepth")
		{
			continue;
		}
		if (bFirst)
		{
			Result = View.Visibility;
			Result.VisibleItems = 0;
			Result.Draws = 0;
			Result.Batches = {};
			bFirst = false;
		}
		Result.VisibleItems += View.Visibility.VisibleItems;
		Result.Draws += View.Visibility.Draws;
		Result.Batches += View.Visibility.Batches;
	}
	return Result;
}

FForwardPipelineStatistics FForwardFrame::Statistics() const
{
	auto Result = Base;
	if (bDeferred)
	{
		const auto Family = Preparation.Statistics();
		Result.PreparationMilliseconds += Family.Milliseconds;
		Result.Views = Family.Views;
	}
	if (Fullscreen)
	{
		Result.FullscreenPreparationMilliseconds = Fullscreen->Milliseconds;
		Result.FullscreenDraws = Fullscreen->Draws;
		Result.PreparationMilliseconds += Fullscreen->Milliseconds;
	}
	return Result;
}
} // namespace Hyperion
