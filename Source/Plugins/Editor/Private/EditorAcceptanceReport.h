#pragma once
#include <iosfwd>

namespace Hyperion
{
struct FEditorAcceptanceReport
{
	bool bContent{};
	bool bDocument{};
	bool bSceneLifecycle{};
	bool bViews{};
	bool bRenderControls{};
	bool bGizmo{};
	bool bMultiSelection{};
	bool bSelectionShortcuts{};
	bool bClipboard{};
	bool bFraming{};
	bool bReparent{};
	bool bPicking{};
	bool bOutlines{};
	bool bPlacement{};
	bool bModelPlacement{};
	bool bInteraction{};
	bool bMovement{};
	bool bMovementGate{};
	bool bRightRelease{};
	bool bLook{};
	bool bDolly{};
	bool bSpeed{};
	bool bInputIsolation{};
};

void WriteEditorAcceptanceReport(std::ostream& InStream, const FEditorAcceptanceReport& InReport);
} // namespace Hyperion
