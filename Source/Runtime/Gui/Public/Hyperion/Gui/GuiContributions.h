#pragma once
#include "Hyperion/Gui/Gui.h"

namespace Hyperion
{
// Synchronous Main notification between BeginFrame and Render. Never retain Gui beyond this call.
struct FGuiPanelEvent
{
	FGui& Gui;
};
} // namespace Hyperion
