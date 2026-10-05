#include "Hyperion/RenderControls/RenderStatistics.h"

namespace Hyperion
{
FSceneVisibilityStats FForwardPipelineStatistics::MainView() const
{
	FSceneVisibilityStats Result;
	bool bFirst = true;
	for (const auto& View : Views)
	{
		if (View.StatsCategory != ERenderViewStatsCategory::Main)
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

} // namespace Hyperion
