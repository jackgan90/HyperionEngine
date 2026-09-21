#include "EditorApplication.h"
#include "Hyperion/Gui/GuiContributions.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace Hyperion
{
namespace
{
bool Matches(std::string InText, std::string InFilter)
{
	const auto Lower = [](unsigned char InValue)
	{
		return static_cast<char>(std::tolower(InValue));
	};
	std::transform(InText.begin(), InText.end(), InText.begin(), Lower);
	std::transform(InFilter.begin(), InFilter.end(), InFilter.begin(), Lower);
	return InText.find(InFilter) != std::string::npos;
}

std::string KindName(ESceneNodeKind InKind)
{
	constexpr std::array Names{"Folder",    "Model",       "Camera",    "Directional Light",
	                           "Sky Light", "Point Light", "Spot Light"};
	return Names.at(static_cast<std::size_t>(InKind));
}

} // namespace

void FEditorPlugin::ShowOpenScene()
{
	RefreshContent();
	SponzaBounds = OpenButtonBounds = CancelButtonBounds = {};
	bRequestOpen = true;
	bOpenDialog = true;
	Camera.Reset();
	bCameraDragging = false;
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
	Gui->BeginDisabled(PendingRoot.has_value());
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
		FileMenuBounds = Gui->LastItemBounds();
		if (bFileOpen)
		{
			DrawRootMenu();
			Gui->Separator();
			if (Gui->MenuItem("Open Scene..."))
			{
				ShowOpenScene();
			}
			OpenMenuBounds = Gui->LastItemBounds();
			Gui->BeginDisabled(CurrentPath.empty() || !Scene->GetStatus().bReady || PendingSave.has_value());
			if (Gui->MenuItem("Save Scene"))
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
	EditMenuBounds = Gui->LastItemBounds();
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
		PreferencesMenuBounds = Gui->LastItemBounds();
		Gui->EndMenu();
	}
}

void FEditorPlugin::DrawWindowMenu()
{
	const bool bOpen = Gui->BeginMenu("Window");
	InspectionBounds["placement/window-menu"] = Gui->LastItemBounds();
	if (!bOpen)
	{
		return;
	}
	DrawApplicationScale();
	if (Gui->MenuItem("Place Object", nullptr, bShowPlacement))
	{
		bShowPlacement = true;
		bFocusPlacement = true;
	}
	InspectionBounds["placement/open-panel"] = Gui->LastItemBounds();
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
		if (Gui->Button("Frame Scene", Scene->GetStatus().bReady && !PreviewCamera))
		{
			FitSceneCamera(ViewCamera, *Scene,
			               ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
		}
		Gui->SameLine();
		Gui->Text("  |  Scene Editor");
	}
	Gui->EndToolbar();
}

void FEditorPlugin::DrawNode(FSceneHandle InHandle)
{
	struct FNodeVisit
	{
		FSceneHandle Handle;
		bool bEndTree{};
	};

	std::vector<FNodeVisit> Pending{{InHandle}};
	while (!Pending.empty())
	{
		const FNodeVisit Visit = Pending.back();
		Pending.pop_back();
		if (Visit.bEndTree)
		{
			Gui->EndTree();
			continue;
		}
		const auto* Node = Scene->FindNode(Visit.Handle);
		if (!Node)
		{
			continue;
		}
		const auto Children = Scene->GetChildren(Visit.Handle);
		Gui->NextRow();
		Gui->NextColumn();
		bool bClicked{};
		const bool bOpen = Gui->TreeItem(Node->Id.c_str(), Node->Name.c_str(), Children.empty(),
		                                 Selection.Contains(Visit.Handle), bClicked, !Options.bBenchmarkCollapsed);
		if (Options.bExercisePicking && Node->Id == "light-courtyard-3")
		{
			PickingLightBounds = Gui->LastItemBounds();
		}
		if (bClicked)
		{
			ClickObject(Visit.Handle, bOutlinerToggle);
		}
		if (Options.bExerciseMultiSelection)
		{
			MultiSelectionRows[Node->Id] = Gui->LastItemBounds();
		}
		Gui->NextColumn();
		Gui->Text(KindName(Node->GetKind()));
		if (bOpen)
		{
			// Keep the parent tree scope open until its children have been visited in order.
			Pending.push_back({Visit.Handle, true});
			for (auto Child = Children.rbegin(); Child != Children.rend(); ++Child)
			{
				Pending.push_back({*Child});
			}
		}
	}
}

void FEditorPlugin::DrawOutliner()
{
	if (!bShowOutliner)
	{
		return;
	}
	if (Gui->BeginWindow("Outliner", bShowOutliner))
	{
		const auto Pointer = Gui->PointerState();
		if (Pointer.bPressed)
		{
			bOutlinerToggle = Pointer.bCtrl;
		}
		if (!bSelectionInitialized && !Selection && Scene->GetStatus().bReady)
		{
			bSelectionInitialized = true;
			const auto Models = Scene->GetNodes(ESceneNodeKind::Model);
			if (!Models.empty())
			{
				Selection = Models.front();
			}
		}
		Gui->Text("Search objects");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##SearchObjects", Filter, false);
		Gui->Text(std::to_string(Scene->GetStatus().Nodes) + " objects" +
		          ("  |  " + std::to_string(Selection.All().size()) + " selected"));
		if (Gui->BeginTable("Objects", "Item Label", "Type"))
		{
			if (Filter.empty())
			{
				for (const auto Root : Scene->GetRoots())
				{
					DrawNode(Root);
				}
			}
			else
			{
				for (const auto Handle : Scene->GetNodes())
				{
					const auto* Node = Scene->FindNode(Handle);
					if (!Node || !Matches(Node->Name, Filter))
					{
						continue;
					}
					Gui->NextRow();
					Gui->NextColumn();
					if (Gui->Selectable((Node->Name + "##" + Node->Id).c_str(), Selection.Contains(Handle)))
					{
						ClickObject(Handle, bOutlinerToggle);
					}
					if (Options.bExerciseMultiSelection)
					{
						MultiSelectionRows[Node->Id] = Gui->LastItemBounds();
					}
					Gui->NextColumn();
					Gui->Text(KindName(Node->GetKind()));
				}
			}
			Gui->EndTable();
		}
	}
	Gui->EndWindow();
}

void FEditorPlugin::DrawDetails()
{
	if (!bShowDetails)
	{
		InspectorDrafts.Clear();
		return;
	}
	InspectorDrafts.Prepare(DocumentEpoch, Scene->GetRevision(), Selection.All());
	if (Gui->BeginWindow("Details", bShowDetails))
	{
		FSceneNodeView View;
		if (Selection && Scene->GetNodeView(*Selection, View))
		{
			try
			{
				if (Selection.All().size() > 1)
				{
					DrawSelectionInspector();
				}
				else
				{
					DrawComponentInspector(View);
				}
				// Commit after drawing so all widgets keep stable bounds during continuous editing.
				if (PendingInspectorEdit)
				{
					auto Edit = std::move(*PendingInspectorEdit);
					PendingInspectorEdit.reset();
					if (!Edit.Edits.empty())
					{
						CommitEdits(std::move(Edit.Edits), Edit.Revision, Edit.Interaction);
					}
					else
					{
						CommitEdit(Edit.Handle, std::move(Edit.Candidate), Edit.Revision, Edit.Interaction);
					}
				}
			}
			catch (const std::exception& Failure)
			{
				Error = Failure.what();
			}
			if (!Error.empty())
			{
				Gui->TextWrapped(Error);
			}
		}
		else
		{
			Gui->TextWrapped("Select an object in the Outliner to inspect its properties.");
		}
	}
	Gui->EndWindow();
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
			const auto Label = ContentRelativePath(Path);
			if (!Matches(Label, SceneFilter))
			{
				continue;
			}
			bool bDoubleClicked{};
			if (Gui->Selectable(Label.c_str(), OpenPath == Path, 0, &bDoubleClicked))
			{
				OpenPath = Path;
				bOpenSelected = bDoubleClicked;
			}
			if (Path.ends_with("/Sponza.hasset"))
			{
				SponzaBounds = Gui->LastItemBounds();
			}
		}
		if (ScenePaths.empty())
		{
			Gui->TextWrapped("No scenes found under the current asset root.");
		}
		Gui->EndScrollRegion();
		Gui->SetNextItemWidth(-1);
		auto DisplayPath = ContentRelativePath(OpenPath);
		if (Gui->InputText("##ScenePath", DisplayPath, false))
		{
			OpenPath =
			    DisplayPath.empty() || PathFromUtf8(DisplayPath).has_root_path() ? DisplayPath : "/Game/" + DisplayPath;
		}
		if (Gui->Button("Open", !OpenPath.empty()) || bOpenSelected)
		{
			OpenScene(OpenPath);
			bOpenDialog = false;
			Gui->ClosePopup();
		}
		OpenButtonBounds = Gui->LastItemBounds();
		Gui->SameLine();
		if (Gui->Button("Cancel"))
		{
			bOpenDialog = false;
			Gui->ClosePopup();
		}
		CancelButtonBounds = Gui->LastItemBounds();
		if (!CatalogError.empty())
		{
			Gui->TextWrapped(CatalogError);
		}
		Gui->EndModal();
	}
}

void FEditorPlugin::DrawViewport(float InDelta, std::span<const FInputEvent> InEvents)
{
	bViewportVisible = false;
	ViewportRegion = {};
	if (!bShowViewport)
	{
		return;
	}
	if (Gui->BeginWindow("Viewport", bShowViewport))
	{
		DrawGizmoToolbar();
		Gui->SameLine();
		DrawViewControls();
		ViewportRegion = Gui->Image(2);
		bViewportVisible = true;
		ResizeViewport();
		RoutePlacement();
		DrawGizmo();
		if (Options.Benchmark.empty())
		{
			RouteCamera(Options.bExercise ? 1.f / 60 : InDelta, InEvents);
		}
		else
		{
			BenchmarkCamera();
		}
		RouteViewportPicking(InEvents);
		DrawLightMarkers();
		DrawSelectionMarkers();
		DrawGizmoOverlay();
	}
	Gui->EndWindow();
}

std::string FEditorPlugin::StatusText() const
{
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
	       "  |  RMB + WASDQE: move   RMB drag: look   RMB + Wheel: speed   Wheel: dolly";
}

FGuiDrawData FEditorPlugin::DrawGui(float InDelta, std::span<const FInputEvent> InEvents)
{
	Gui->BeginFrame(Window->LogicalSize(), Window->PixelSize(), std::clamp(InDelta, .001f, .1f), InEvents);
	bGizmoUsedMouse = false;
	bPlacementUsedMouse = false;
	Gui->BeginDisabled(PendingRoot.has_value());
	DrawMenus();
	CaptureButtonBounds = {};
	DrawToolbar();
	Gui->StatusBar(StatusText());
	Gui->DockSpace({"Viewport", "Outliner", "Details", "Content Browser", "Place Object"}, bResetLayout);
	bResetLayout = false;
	DrawPlacementPanel();
	DrawViewport(InDelta, InEvents);
	if (!bViewportVisible)
	{
		CancelPlacement();
		ViewportClick.reset();
		FinishGizmo();
		RouteCamera(InDelta, InEvents);
	}
	DrawOutliner();
	InspectorInteraction = 0;
	PendingInspectorEdit.reset();
	DrawDetails();
	if (InspectorTransaction && InspectorTransaction->Interaction != InspectorInteraction)
	{
		InspectorTransaction.reset();
	}
	DrawSceneBrowser();
	Gui->EndDisabled();
	DrawOpenDialog();
	DrawSaveDialog();
	DrawDiscardDialog();
	DrawPreferences();
	DrawAssetMessage();
	Context.Publish(FGuiPanelEvent{*Gui});
	RouteDeleteShortcut(InEvents);
	return Gui->Render();
}
} // namespace Hyperion
