#include "EditorApplication.h"

namespace Hyperion
{
FEditorShortcutInteraction FEditorPlugin::CaptureShortcutInteraction(std::span<const FInputEvent> InEvents) const
{
	auto Result = CaptureInteractionPolicy().Shortcuts();
	for (const auto& Event : InEvents)
	{
		Result.bFocusLost |= Event.Type == EEventType::Focus && !Event.bDown;
		Result.bOwnershipTransition |= Event.Type == EEventType::MouseButton || Event.Type == EEventType::Focus ||
		                               Event.Type == EEventType::Text ||
		                               (Event.Type == EEventType::Key && Event.Key == EKey::Tab);
		// Any navigation press in this batch owns input, even when released before routing.
		Result.bGesture |= Event.Type == EEventType::MouseButton && Event.Button == InputButtons::Right;
	}
	return Result;
}
} // namespace Hyperion
