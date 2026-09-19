#include "Hyperion/Core/Core.h"
#include "ViewerApplication.h"

namespace Hyperion
{
void FViewerPlugin::ExerciseContactInput(std::vector<FInputEvent>& InEvents)
{
	if (!Options.bExerciseContactShadows || bContactExerciseCompleted || !ScenePlugin->Ready() ||
	    !SceneStatistics.Draws)
	{
		return;
	}
	const bool bExpected = ContactToggleCount % 2 != 0;
	const bool bMatched = Settings.bContactShadows == bExpected && PipelineStatistics.bContactShadows == bExpected &&
	                      (PipelineStatistics.HierarchicalDepth.Dispatches > 0) == bExpected &&
	                      (PipelineStatistics.HierarchicalDepth.Bytes > 0) == bExpected;
	if (!bContactMouseDown)
	{
		ContactStableFrames = bMatched ? ContactStableFrames + 1 : 0;
		if (ContactStableFrames < 30)
		{
			return;
		}
		Log(ELogLevel::Info, "Contact GUI verified: enabled=" + std::to_string(bExpected) +
		                         " dispatches=" + std::to_string(PipelineStatistics.HierarchicalDepth.Dispatches));
		if (ContactToggleCount == 3)
		{
			bContactExerciseCompleted = true;
			return;
		}
	}
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = (ContactShadowBounds.X + ContactShadowBounds.Z) * .5f;
	Move.Y = (ContactShadowBounds.Y + ContactShadowBounds.W) * .5f;
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = !bContactMouseDown;
	InEvents.push_back(Move);
	InEvents.push_back(Button);
	bContactMouseDown = Button.bDown;
	if (!bContactMouseDown)
	{
		++ContactToggleCount;
		ContactStableFrames = 0;
	}
}
} // namespace Hyperion
