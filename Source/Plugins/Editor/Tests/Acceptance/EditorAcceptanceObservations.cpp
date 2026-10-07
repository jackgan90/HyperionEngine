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

constexpr EEditorWidget Widgets[]{
    EEditorWidget::SaveAs,
    EEditorWidget::SavePath,
    EEditorWidget::SaveConfirm,
    EEditorWidget::RenderSettingsMenu,
    EEditorWidget::WindowMenu,
    EEditorWidget::PlacementOpen,
    EEditorWidget::PlacementTitle,
    EEditorWidget::LogToggle,
    EEditorWidget::RenderPipeline,
    EEditorWidget::GBufferFormat,
    EEditorWidget::Vsync,
    EEditorWidget::ReversedZ,
    EEditorWidget::HudStatusToggle,
    EEditorWidget::HudProfilingToggle,
    EEditorWidget::Visualizer,
    EEditorWidget::ProfilingCategories,
    EEditorWidget::ProfilingMenuTitle,
    EEditorWidget::ProfilingGpuCollection,
    EEditorWidget::Exposure,
    EEditorWidget::StatusHud,
    EEditorWidget::ProfilingHud,
    EEditorWidget::ViewOptions,
    EEditorWidget::CullingMode,
    EEditorWidget::OutlineMode,
    EEditorWidget::OutlineQuality,
    EEditorWidget::ReturnToEditorView,
    EEditorWidget::InitialView,
    EEditorWidget::CreateCamera,
    EEditorWidget::PreviewCamera,
    EEditorWidget::ApplyCamera,
    EEditorWidget::HierarchyRoot,
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
				Scenario.Bounds.Clear();
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
	for (const auto Widget : Widgets)
	{
		if (Widget == InWidget)
		{
			Scenario.Bounds.Set(Widget, InBounds);
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
		case EEditorWidget::OutlinerTreeToggle:
			if (Options.bExerciseSelectionShortcuts)
			{
				Scenario.ShortcutTreeToggles[std::string(InId)] = InBounds;
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
				Scenario.Bounds.Set(InWidget, InBounds);
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
				Scenario.Bounds.Set(FWidgetKey::WithId(InWidget, InId), InBounds);
			}
			break;
		case EEditorWidget::ObjectName:
			if (Options.bExerciseSelectionShortcuts)
			{
				Scenario.Bounds.Set(InWidget, InBounds);
			}
			break;
		case EEditorWidget::PlacementItem:
			Scenario.Bounds.Set(FWidgetKey::WithId(InWidget, InId), InBounds);
			break;
		case EEditorWidget::RenderPipelineItem:
			Scenario.Bounds.Set(FWidgetKey::WithId(InWidget, InId), InBounds);
			break;
		case EEditorWidget::GBufferFormatItem:
			Scenario.Bounds.Set(FWidgetKey::WithId(InWidget, InId), InBounds);
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
			Scenario.Bounds.Set(FWidgetKey::WithValue(InWidget, InIndex), InBounds);
			break;
		case EEditorWidget::VisualizerItem:
			Scenario.Bounds.Set(FWidgetKey::WithValue(InWidget, InIndex), InBounds);
			break;
		case EEditorWidget::CullingModeItem:
			Scenario.Bounds.Set(FWidgetKey::WithValue(InWidget, InIndex), InBounds);
			break;
		case EEditorWidget::OutlineModeItem:
			Scenario.Bounds.Set(FWidgetKey::WithValue(InWidget, InIndex), InBounds);
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
		Scenario.Bounds.Set(FPropertyKey{Component, std::string(InField)}, InBounds);
	};
}
} // namespace Hyperion
