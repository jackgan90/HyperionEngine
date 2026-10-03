#pragma once

namespace Hyperion
{
enum class EEditorSurface
{
	ScenePicker,
	Toolbar,
	ContentBrowser,
	SelectionInspector
};

enum class EEditorWidget
{
	FileMenu,
	OpenMenu,
	ImportMenu,
	EditMenu,
	PreferencesMenu,
	CapturePreference,
	CaptureHudPreference,
	PreferencesClose,
	CaptureButton,
	GizmoMode,
	SaveSwitch,
	DiscardChanges,
	CancelChanges,
	DiscardTitle,
	SceneEntry,
	OpenScene,
	CancelOpenScene,
	OutlinerTreeItem,
	OutlinerRow,
	OutlinerSearch,
	ContentTile,
	ComponentHeader,
	ObjectName,
	SaveAs,
	SavePath,
	SaveConfirm,
	RenderSettingsMenu,
	WindowMenu,
	PlacementOpen,
	PlacementTitle,
	PlacementItem,
	LogToggle,
	RenderPipeline,
	RenderPipelineItem,
	GBufferFormat,
	GBufferFormatItem,
	Vsync,
	ReversedZ,
	HudStatusToggle,
	HudProfilingToggle,
	Visualizer,
	VisualizerItem,
	ProfilingCategories,
	ProfilingMenuTitle,
	ProfilingCategory,
	ProfilingGpuCollection,
	Exposure,
	StatusHud,
	ProfilingHud,
	ViewOptions,
	OutlineMode,
	OutlineQuality,
	ReturnToEditorView,
	InitialView,
	CreateCamera,
	PreviewCamera,
	ApplyCamera,
	HierarchyRoot
};
} // namespace Hyperion
