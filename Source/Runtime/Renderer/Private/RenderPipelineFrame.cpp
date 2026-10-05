#include "Hyperion/Renderer/RenderPipelineFrame.h"
#include "Hyperion/Renderer/FullscreenPass.h"

namespace Hyperion
{
FRenderFrameStatistics FForwardFrame::Statistics() const
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
