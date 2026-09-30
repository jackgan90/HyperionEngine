#pragma once
#include "Hyperion/Platform/Window.h"

namespace Hyperion
{
enum class EEditorShortcut
{
	Delete,
	SelectAll,
	Clipboard,
	FrameSelection,
	History,
	Save
};

struct FEditorShortcutInteraction
{
	bool bSceneFocus{};
	bool bDetailsFocus{};
	bool bText{};
	bool bPopup{};
	bool bGesture{};
	bool bBusy{};
	bool bInspector{};
	bool bReady{};
	bool bFocusLost{};
	bool bOwnershipTransition{};

	bool Allows(EEditorShortcut InCommand) const
	{
		if (!bReady || bFocusLost || bPopup || bGesture || bBusy)
		{
			return false;
		}
		if (InCommand == EEditorShortcut::Save)
		{
			return !bOwnershipTransition;
		}
		if (InCommand == EEditorShortcut::History)
		{
			return !bOwnershipTransition && (!bText || bInspector);
		}
		if (bText || bInspector)
		{
			return false;
		}
		return bSceneFocus || (bDetailsFocus && (InCommand == EEditorShortcut::Clipboard ||
		                                         InCommand == EEditorShortcut::FrameSelection));
	}
};
} // namespace Hyperion
