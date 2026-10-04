#include "EditorApplication.h"
#include "EditorTextFilter.h"
#include "Hyperion/Content/ContentPaths.h"
#include "Hyperion/Gui/GuiContributions.h"
#include "Hyperion/Gui/PathDisplay.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include <algorithm>
#include <array>
#include <vector>

namespace Hyperion
{
void FEditorPlugin::ShowOpenScene()
{
	RefreshContent();
	Acceptance.BeginSurface(EEditorSurface::ScenePicker);
	bRequestOpen = true;
	bOpenDialog = true;
	Camera.Reset();
	Viewport.bCameraDragging = false;
	if (OpenPath.empty() && !ScenePaths.empty())
	{
		OpenPath = ScenePaths.front();
	}
}

void FEditorPlugin::DrawApplicationScale()
{
	if (!Gui->BeginMenu("Application Scale"))
	{
		return;
	}
	constexpr std::array Presets{1.f, 1.25f, 1.5f, 1.75f, 2.f};
	constexpr std::array Labels{"100%", "125%", "150%", "175%", "200%"};
	for (std::size_t Index = 0; Index < Presets.size(); ++Index)
	{
		if (Gui->MenuItem(Labels[Index], nullptr, Gui->ApplicationScale() == Presets[Index]))
		{
			Gui->SetApplicationScale(Presets[Index]);
		}
	}
	Gui->Separator();
	Gui->SetNextItemWidth(160);
	Gui->ApplicationScaleControl("Custom");
	if (Gui->MenuItem("Reset to default (125%)"))
	{
		Gui->SetApplicationScale(1.25f);
	}
	Gui->EndMenu();
}

void FEditorPlugin::DrawRootMenu()
{
	Gui->BeginDisabled(!CaptureInteractionPolicy().AllowsScenePanels());
	if (Gui->MenuItem("Open..."))
	{
		bRequestRootDialog = true;
	}
	if (Gui->BeginMenu("Recent"))
	{
		if (Options.Preferences.RecentRoots.empty())
		{
			Gui->Text("No recent asset roots");
		}
		for (const auto& Root : Options.Preferences.RecentRoots)
		{
			if (Gui->MenuItem(PathToUtf8(Root).c_str()))
			{
				QueueContentRoot(Root);
			}
		}
		Gui->EndMenu();
	}
	Gui->EndDisabled();
}

void FEditorPlugin::DrawMenus()
{
	const auto Attempt = [&](const auto& InAction)
	{
		try
		{
			InAction();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	};
	if (Gui->BeginMenuBar())
	{
		const bool bFileOpen = Gui->BeginMenu("File");
		Acceptance.ObserveWidget(EEditorWidget::FileMenu, Gui->LastItemBounds());
		if (bFileOpen)
		{
			Gui->BeginDisabled(AssetWorkspace->HasPendingEdits());
			if (Gui->MenuItem("Save All Assets"))
			{
				AssetWorkspace->SaveAll();
			}
			Gui->EndDisabled();
			DrawRootMenu();
			if (Gui->MenuItem("Import Asset..."))
			{
				ImportPanel->Show();
			}
			Acceptance.ObserveWidget(EEditorWidget::ImportMenu, Gui->LastItemBounds());
			Gui->Separator();
			if (Gui->MenuItem("Open Scene..."))
			{
				ShowOpenScene();
			}
			Acceptance.ObserveWidget(EEditorWidget::OpenMenu, Gui->LastItemBounds());
			Gui->BeginDisabled(CurrentPath.empty() || !Scene->GetStatus().bReady || PendingSave.has_value());
			if (Gui->MenuItem("Save Scene", "Ctrl+S"))
			{
				Attempt(
				    [&]
				    {
					    SaveScene(CurrentPath);
				    });
			}
			Gui->EndDisabled();
			Gui->BeginDisabled(!Scene->GetStatus().bReady || PendingSave.has_value());
			if (Gui->MenuItem("Save Scene As..."))
			{
				SavePath = CurrentPath;
				bSaveDialog = bRequestSaveDialog = true;
			}
			Acceptance.ObserveWidget(EEditorWidget::SaveAs, Gui->LastItemBounds());
			Gui->EndDisabled();
			Gui->Separator();
			if (Gui->MenuItem("Exit"))
			{
				Window->RequestClose();
			}
			Gui->EndMenu();
		}
		DrawEditMenu();
		DrawWindowMenu();
		if (Gui->BeginMenu("Help"))
		{
			Gui->Text("Click the viewport to focus");
			Gui->Text("Hold right mouse + W A S D: move  |  Q E: down / up");
			Gui->Text("Right drag: look around  |  Wheel: dolly  |  Home: frame scene");
			Gui->Text("Right mouse + Wheel: adjust camera speed (scene units / second)");
			Gui->EndMenu();
		}
		Gui->SameLine();
		Gui->Text("    " +
		          (CurrentPath.empty() ? std::string("Untitled") : std::filesystem::path(CurrentPath).stem().string()) +
		          (IsDirty() ? " *" : ""));
		Gui->EndMenuBar();
	}
}

void FEditorPlugin::DrawEditMenu()
{
	const auto Attempt = [this](const auto& InAction)
	{
		try
		{
			InAction();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	};
	const bool bOpen = Gui->BeginMenu("Edit");
	Acceptance.ObserveWidget(EEditorWidget::EditMenu, Gui->LastItemBounds());
	if (bOpen)
	{
		Gui->BeginDisabled(HistoryCursor == 0);
		if (Gui->MenuItem("Undo", "Ctrl+Z"))
		{
			Attempt(
			    [&]
			    {
				    Undo();
			    });
		}
		Gui->EndDisabled();
		Gui->BeginDisabled(HistoryCursor == History.size());
		if (Gui->MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z"))
		{
			Attempt(
			    [&]
			    {
				    Redo();
			    });
		}
		Gui->EndDisabled();
		Gui->Separator();
		if (Gui->MenuItem("Editor preference"))
		{
			bPreferencesDialog = bRequestPreferences = true;
		}
		Acceptance.ObserveWidget(EEditorWidget::PreferencesMenu, Gui->LastItemBounds());
		if (Gui->MenuItem("Render settings", nullptr, bShowRenderSettings))
		{
			bShowRenderSettings = true;
		}
		Acceptance.ObserveWidget(EEditorWidget::RenderSettingsMenu, Gui->LastItemBounds());
		Gui->EndMenu();
	}
}

void FEditorPlugin::DrawWindowMenu()
{
	const bool bOpen = Gui->BeginMenu("Window");
	Acceptance.ObserveWidget(EEditorWidget::WindowMenu, Gui->LastItemBounds());
	if (!bOpen)
	{
		return;
	}
	DrawApplicationScale();

	if (Gui->MenuItem("Asset Editor"))
	{
		try
		{
			EnsureAssetWindow();
			AssetWindow->Activate();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
	if (Gui->MenuItem("Place Object", nullptr, bShowPlacement))
	{
		bShowPlacement = true;
		bFocusPlacement = true;
	}
	Acceptance.ObserveWidget(EEditorWidget::PlacementOpen, Gui->LastItemBounds());
	for (const auto& [Label, Visible] : {std::pair{"Viewport", &bShowViewport},
	                                     {"Outliner", &bShowOutliner},
	                                     {"Details", &bShowDetails},
	                                     {"Content Browser", &bShowBrowser}})
	{
		if (Gui->MenuItem(Label, nullptr, *Visible))
		{
			*Visible = !*Visible;
		}
	}
	if (Gui->MenuItem("Log", nullptr, bShowLog))
	{
		bShowLog = !bShowLog;
		bFocusLog = bShowLog;
	}
	Acceptance.ObserveWidget(EEditorWidget::LogToggle, Gui->LastItemBounds());
	Gui->Separator();
	if (Gui->MenuItem("Reset Layout"))
	{
		bResetLayout = true;
		bShowViewport = bShowOutliner = bShowDetails = bShowBrowser = bShowPlacement = true;
	}
	Gui->EndMenu();
}

void FEditorPlugin::DrawToolbar()
{
	if (Gui->BeginToolbar())
	{
		if (Gui->Button("Open Scene"))
		{
			ShowOpenScene();
		}
		Gui->SameLine();
		Gui->Text("  |  Scene Editor");
	}
	Gui->EndToolbar();
}

void FEditorPlugin::DrawOpenDialog()
{
	if (bRequestOpen)
	{
		Gui->OpenPopup("Open Scene");
		bRequestOpen = false;
	}
	if (Gui->BeginModal("Open Scene", bOpenDialog))
	{
		bool bOpenSelected{};
		if (Gui->Button("Refresh"))
		{
			RefreshContent();
		}
		Gui->Text(Browser->IsScanning() ? "Scanning scenes..." : "Scene scan complete");
		Gui->Text("Choose a scene from the current asset root");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##FilterScenes", SceneFilter, false);
		Gui->BeginScrollRegion("SceneList", 250);
		for (const auto& Path : ScenePaths)
		{
			const auto Label = FGuiPathDisplay{GameContentRoot}.Value(Path);
			if (!MatchesEditorFilter(Label, SceneFilter))
			{
				continue;
			}
			bool bDoubleClicked{};
			if (Gui->Selectable(Label.c_str(), OpenPath == Path, 0, &bDoubleClicked))
			{
				OpenPath = Path;
				bOpenSelected = bDoubleClicked;
			}
			Acceptance.ObserveWidget(EEditorWidget::SceneEntry, Gui->LastItemBounds(), Path);
		}
		if (ScenePaths.empty())
		{
			Gui->TextWrapped("No scenes found under the current asset root.");
		}
		Gui->EndScrollRegion();
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##ScenePath", OpenPath, false, true);
		if (Gui->Button("Open", !OpenPath.empty()) || bOpenSelected)
		{
			OpenScene(OpenPath);
			bOpenDialog = false;
			Gui->ClosePopup();
		}
		Acceptance.ObserveWidget(EEditorWidget::OpenScene, Gui->LastItemBounds());
		Gui->SameLine();
		if (Gui->Button("Cancel"))
		{
			bOpenDialog = false;
			Gui->ClosePopup();
		}
		Acceptance.ObserveWidget(EEditorWidget::CancelOpenScene, Gui->LastItemBounds());
		if (!CatalogError.empty())
		{
			Gui->TextWrapped(CatalogError);
		}
		Gui->EndModal();
	}
}

void FEditorPlugin::DrawViewport(float InDelta, std::span<const FInputEvent> InEvents)
{
	Viewport.bViewportVisible = false;
	Viewport.ViewportRegion = {};
	if (!bShowViewport)
	{
		return;
	}
	if (Gui->BeginWindow("Viewport", bShowViewport))
	{
		DrawGizmoToolbar();
		Gui->SameLine();
		DrawViewControls();
		Viewport.ViewportRegion = Gui->Image(2);
		Viewport.bViewportVisible = true;
		ResizeViewport();
		RoutePlacement();
		DrawGizmo();
		if (Options.Benchmark.empty())
		{
			RouteCamera(Acceptance.Policy().CameraDelta.value_or(InDelta), InEvents);
		}
		else
		{
			BenchmarkCamera();
		}
		RouteViewportPicking(InEvents);
	}
	Gui->EndWindow();
}

std::string FEditorPlugin::StatusText() const
{
	if (!Scene->GetStatus().AssetRefreshError.empty())
	{
		return "Asset refresh failed: " + Scene->GetStatus().AssetRefreshError;
	}
	if (!Error.empty())
	{
		return Error;
	}
	if (!Scene->GetStatus().Error.empty())
	{
		return "Scene error: " + Scene->GetStatus().Error;
	}
	if (!Scene->GetStatus().PublicationError.empty())
	{
		return "Scene error: " + Scene->GetStatus().PublicationError;
	}
	if (!SaveStatus.empty())
	{
		return SaveStatus + (IsDirty() ? "  |  Unsaved changes" : "");
	}
	if (Scene->GetStatus().FailedModels || Scene->GetStatus().FailedSkies)
	{
		return "Scene has failed resources  |  " + std::to_string(Scene->GetStatus().FailedModels) + " meshes, " +
		       std::to_string(Scene->GetStatus().FailedSkies) + " skies  |  Open another scene to retry";
	}
	if (CurrentPath.empty())
	{
		return "Ready  |  File > Open Scene  |  Click viewport to navigate";
	}
	const auto& Status = Scene->GetStatus();
	return (Status.bReady ? "Ready  |  " : "Loading  |  ") + std::to_string(Status.ReadyModels) + "/" +
	       std::to_string(Status.Models) + " meshes  |  " + CurrentPath +
	       "  |  F: frame selection   Home: frame scene   RMB + WASDQE: move   RMB drag: look   RMB + Wheel: speed   "
	       "Wheel: dolly";
}

FGuiDrawData FEditorPlugin::DrawGui(float InDelta, std::span<const FInputEvent> InEvents)
{
	Gui->BeginFrame(Window->LogicalSize(), Window->PixelSize(), std::clamp(InDelta, .001f, .1f), InEvents);
	RouteHistoryShortcuts(InEvents);
	bGizmoUsedMouse = false;
	bPlacementUsedMouse = false;
	Gui->BeginDisabled(!CaptureInteractionPolicy().AllowsScenePanels());
	DrawMenus();
	Acceptance.BeginSurface(EEditorSurface::Toolbar);
	DrawToolbar();
	DrawRenderSettings();
	Gui->StatusBar(StatusText());
	Gui->DockSpace({"Viewport", "Outliner", "Details", "Content Browser", "Place Object", .25f, {"Log"}}, bResetLayout);
	bResetLayout = false;
	UpdateReparentGesture(InEvents);
	DrawPlacementPanel();
	DrawViewport(InDelta, InEvents);
	if (!Viewport.bViewportVisible)
	{
		CancelPlacement();
		ViewportClick.reset();
		FinishGizmo();
		RouteCamera(InDelta, InEvents);
	}
	DrawOutliner();
	FinishReparentGesture();
	InspectorInteraction = 0;
	PendingInspectorEdit.reset();
	DrawDetails();
	if (Options.Benchmark.empty() && InspectorTransaction && InspectorTransaction->Interaction != InspectorInteraction)
	{
		SceneDocument.FinishInteraction();
	}
	DrawSceneBrowser();
	DrawLog();
	ImportPanel->Draw(*Gui);
	Gui->EndDisabled();
	DrawOpenDialog();
	DrawSaveDialog();
	DrawDiscardDialog();
	DrawPreferences();
	DrawAssetMessage();
	Context.Publish(FGuiPanelEvent{*Gui});
	Acceptance.DrawPanels();
	RouteDeleteShortcut(InEvents);
	RouteClipboardShortcuts(InEvents);
	RouteSelectAllShortcut(InEvents);
	RouteFrameSelectionShortcut(InEvents);
	DrawViewportOverlays();
	return Gui->Render();
}
} // namespace Hyperion
