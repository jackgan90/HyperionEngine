#include "EditorAcceptanceReport.h"
#include <ostream>

namespace Hyperion
{
void WriteEditorAcceptanceReport(std::ostream& InStream, const FEditorAcceptanceReport& InReport)
{
	InStream << "\"content_verified\": " << InReport.bContent << ",\n"
	         << "\"document_verified\": " << InReport.bDocument << ",\n"
	         << "\"scene_lifecycle_verified\": " << InReport.bSceneLifecycle << ",\n"
	         << "\"views_verified\": " << InReport.bViews << ",\n"
	         << "\"render_controls_verified\": " << InReport.bRenderControls << ",\n"
	         << "\"gizmo_verified\": " << InReport.bGizmo << ",\n"
	         << "\"multiselect_verified\": " << InReport.bMultiSelection << ",\n"
	         << "\"selection_shortcuts_verified\": " << InReport.bSelectionShortcuts << ",\n"
	         << "\"clipboard_verified\": " << InReport.bClipboard << ",\n"
	         << "\"framing_verified\": " << InReport.bFraming << ",\n"
	         << "\"reparent_verified\": " << InReport.bReparent << ",\n"
	         << "\"picking_verified\": " << InReport.bPicking << ",\n"
	         << "\"outlines_verified\": " << InReport.bOutlines << ",\n"
	         << "\"placement_verified\": " << InReport.bPlacement << ",\n"
	         << "\"model_placement_verified\": " << InReport.bModelPlacement << ",\n"
	         << "\"interaction_verified\": " << InReport.bInteraction << ",\n"
	         << "\"movement\": " << InReport.bMovement << ",\n"
	         << "\"movement_gate\": " << InReport.bMovementGate << ",\n"
	         << "\"right_release\": " << InReport.bRightRelease << ",\n"
	         << "\"look\": " << InReport.bLook << ",\n"
	         << "\"dolly\": " << InReport.bDolly << ",\n"
	         << "\"wheel_speed\": " << InReport.bSpeed << ",\n"
	         << "\"input_isolation\": " << InReport.bInputIsolation << "\n";
}
} // namespace Hyperion
