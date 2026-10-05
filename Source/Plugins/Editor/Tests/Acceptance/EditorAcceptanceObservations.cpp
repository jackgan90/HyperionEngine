#include "EditorAcceptanceHarness.h"
#include <array>

namespace Hyperion
{
namespace
{
struct FWidgetMember
{
	EEditorWidget Widget;
	FVec4 FEditorAcceptanceState::* Member;
};

constexpr FWidgetMember Members[]{
    {EEditorWidget::FileMenu, &FEditorAcceptanceState::FileMenuBounds},
    {EEditorWidget::OpenMenu, &FEditorAcceptanceState::OpenMenuBounds},
    {EEditorWidget::ImportMenu, &FEditorAcceptanceState::ImportMenuBounds},
    {EEditorWidget::EditMenu, &FEditorAcceptanceState::EditMenuBounds},
    {EEditorWidget::PreferencesMenu, &FEditorAcceptanceState::PreferencesMenuBounds},
    {EEditorWidget::CapturePreference, &FEditorAcceptanceState::CapturePreferenceBounds},
    {EEditorWidget::CaptureHudPreference, &FEditorAcceptanceState::CaptureHudPreferenceBounds},
    {EEditorWidget::PreferencesClose, &FEditorAcceptanceState::PreferencesCloseBounds},
    {EEditorWidget::CaptureButton, &FEditorAcceptanceState::CaptureButtonBounds},
    {EEditorWidget::SaveSwitch, &FEditorAcceptanceState::SaveSwitchBounds},
    {EEditorWidget::DiscardChanges, &FEditorAcceptanceState::DiscardChangesBounds},
    {EEditorWidget::CancelChanges, &FEditorAcceptanceState::CancelChangesBounds},
    {EEditorWidget::DiscardTitle, &FEditorAcceptanceState::DiscardTitleBounds},
    {EEditorWidget::OpenScene, &FEditorAcceptanceState::OpenButtonBounds},
    {EEditorWidget::CancelOpenScene, &FEditorAcceptanceState::CancelButtonBounds},
};

struct FWidgetKey
{
	EEditorWidget Widget;
	std::string_view Key;
};

constexpr FWidgetKey Keys[]{
    {EEditorWidget::SaveAs, "document/save-as"},
    {EEditorWidget::SavePath, "document/save-path"},
    {EEditorWidget::SaveConfirm, "document/save-confirm"},
    {EEditorWidget::RenderSettingsMenu, "render/settings-menu"},
    {EEditorWidget::WindowMenu, "placement/window-menu"},
    {EEditorWidget::PlacementOpen, "placement/open-panel"},
    {EEditorWidget::PlacementTitle, "placement/title"},
    {EEditorWidget::LogToggle, "log/toggle"},
    {EEditorWidget::RenderPipeline, "render/pipeline"},
    {EEditorWidget::GBufferFormat, "render/gbuffer"},
    {EEditorWidget::Vsync, "render/vsync"},
    {EEditorWidget::ReversedZ, "render/reversed-z"},
    {EEditorWidget::HudStatusToggle, "hud/status-toggle"},
    {EEditorWidget::HudProfilingToggle, "hud/profiling-toggle"},
    {EEditorWidget::Visualizer, "hud/visualizer"},
    {EEditorWidget::ProfilingCategories, "hud/categories"},
    {EEditorWidget::ProfilingMenuTitle, "hud/menu-title"},
    {EEditorWidget::ProfilingGpuCollection, "hud/collect-gpu"},
    {EEditorWidget::Exposure, "hud/exposure"},
    {EEditorWidget::StatusHud, "hud/status"},
    {EEditorWidget::ProfilingHud, "hud/profiling"},
    {EEditorWidget::ViewOptions, "view/options"},
    {EEditorWidget::CullingMode, "view/culling"},
    {EEditorWidget::OutlineMode, "outline/mode"},
    {EEditorWidget::OutlineQuality, "outline/quality"},
    {EEditorWidget::ReturnToEditorView, "view/return"},
    {EEditorWidget::InitialView, "view/initial"},
    {EEditorWidget::CreateCamera, "view/create"},
    {EEditorWidget::PreviewCamera, "view/preview"},
    {EEditorWidget::ApplyCamera, "view/apply"},
    {EEditorWidget::HierarchyRoot, "hierarchy/root"},
};
} // namespace

void FEditorAcceptanceHarness::BeginSurface(EEditorSurface InSurface)
{
	switch (InSurface)
	{
		case EEditorSurface::ScenePicker:
			Scenario.SponzaBounds = Scenario.OpenButtonBounds = Scenario.CancelButtonBounds = {};
			break;
		case EEditorSurface::Toolbar:
			Scenario.CaptureButtonBounds = {};
			break;
		case EEditorSurface::ContentBrowser:
			Scenario.ContentTileBounds.clear();
			break;
		case EEditorSurface::SelectionInspector:
			if (Editor.Options.bExerciseMultiSelection)
			{
				Scenario.InspectionBounds.clear();
			}
			break;
	}
}

void FEditorAcceptanceHarness::ObserveWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId)
{
	for (const auto& Entry : Members)
	{
		if (Entry.Widget == InWidget)
		{
			Scenario.*(Entry.Member) = InBounds;
			return;
		}
	}
	for (const auto& Entry : Keys)
	{
		if (Entry.Widget == InWidget)
		{
			Scenario.InspectionBounds[std::string(Entry.Key)] = InBounds;
			return;
		}
	}
	ObserveIdentifiedWidget(InWidget, InBounds, InId);
}

void FEditorAcceptanceHarness::ObserveIdentifiedWidget(EEditorWidget InWidget, FVec4 InBounds, std::string_view InId)
{
	const auto& Options = Editor.Options;
	switch (InWidget)
	{
		case EEditorWidget::SceneEntry:
			if (InId.ends_with("/Sponza.hasset"))
			{
				Scenario.SponzaBounds = InBounds;
			}
			break;
		case EEditorWidget::OutlinerTreeItem:
			if (Options.bExercisePicking && InId == "light-courtyard-3")
			{
				Scenario.PickingLightBounds = InBounds;
			}
			break;
		case EEditorWidget::OutlinerRow:
			if (Options.bExerciseMultiSelection || Options.bExerciseFraming || Options.bExerciseSelectionShortcuts ||
			    !Options.ExerciseReparent.empty())
			{
				Scenario.MultiSelectionRows[std::string(InId)] = InBounds;
			}
			break;
		case EEditorWidget::OutlinerSearch:
			if (Options.bExerciseClipboard || Options.bExerciseFraming || Options.bExerciseSelectionShortcuts)
			{
				Scenario.InspectionBounds["clipboard/search"] = InBounds;
			}
			break;
		case EEditorWidget::ContentTile:
			if (!Options.ExerciseContent.empty() || !Options.ExerciseAssets.empty() ||
			    !Options.ExerciseModelPlacement.empty())
			{
				Scenario.ContentTileBounds[std::string(InId)] = InBounds;
			}
			break;
		case EEditorWidget::ComponentHeader:
			if (!Options.ExerciseDocument.empty() || !Options.ExerciseRenderControls.empty())
			{
				Scenario.InspectionBounds[std::string(InId) + "/header"] = InBounds;
			}
			break;
		case EEditorWidget::ObjectName:
			if (Options.bExerciseSelectionShortcuts)
			{
				Scenario.InspectionBounds["shortcut/name"] = InBounds;
			}
			break;
		case EEditorWidget::PlacementItem:
			Scenario.InspectionBounds["placement/" + std::string(InId)] = InBounds;
			break;
		case EEditorWidget::RenderPipelineItem:
			Scenario.InspectionBounds["render/pipeline/" + std::string(InId)] = InBounds;
			break;
		case EEditorWidget::GBufferFormatItem:
			Scenario.InspectionBounds["render/gbuffer/" + std::string(InId)] = InBounds;
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ObserveIndexedWidget(EEditorWidget InWidget, FVec4 InBounds, std::size_t InIndex)
{
	switch (InWidget)
	{
		case EEditorWidget::GizmoMode:
			Scenario.GizmoButtonBounds.at(InIndex) = InBounds;
			break;
		case EEditorWidget::ProfilingCategory:
			Scenario.InspectionBounds["hud/category/" + std::to_string(InIndex)] = InBounds;
			break;
		case EEditorWidget::VisualizerItem:
			Scenario.InspectionBounds["hud/visualizer/" + std::to_string(InIndex)] = InBounds;
			break;
		case EEditorWidget::CullingModeItem:
			Scenario.InspectionBounds["view/culling/" + std::to_string(InIndex)] = InBounds;
			break;
		case EEditorWidget::OutlineModeItem:
			Scenario.InspectionBounds["outline/mode/" + std::to_string(InIndex)] = InBounds;
			break;
		default:
			break;
	}
}

std::function<void(std::string_view, FVec4)> FEditorAcceptanceHarness::PropertyObserver(std::string_view InComponent,
                                                                                        bool bInSelection)
{
	if (!bInSelection && Editor.Options.ExerciseDocument.empty() && Editor.Options.ExerciseRenderControls.empty())
	{
		return {};
	}
	return [this, Component = std::string(InComponent)](std::string_view InField, FVec4 InBounds)
	{
		Scenario.InspectionBounds[Component + "/" + std::string(InField)] = InBounds;
	};
}
} // namespace Hyperion
