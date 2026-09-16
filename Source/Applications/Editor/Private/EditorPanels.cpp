#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>

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
	constexpr std::array Names{"Folder",    "Static Mesh", "Camera",    "Directional Light",
	                           "Sky Light", "Point Light", "Spot Light"};
	return Names.at(static_cast<std::size_t>(InKind));
}

std::string Number(float InValue, int InPrecision = 2)
{
	std::ostringstream Stream;
	Stream << std::fixed << std::setprecision(InPrecision) << InValue;
	return Stream.str();
}
} // namespace

void FEditorApplication::ShowOpenScene()
{
	bRequestOpen = true;
	bOpenDialog = true;
	Camera.Reset();
	bCameraDragging = false;
	if (OpenPath.empty() && !ScenePaths.empty())
	{
		OpenPath = ScenePaths.front();
	}
}

void FEditorApplication::DrawMenus()
{
	if (Gui->BeginMenuBar())
	{
		Gui->Text(" HYPERION ");
		const bool bFileOpen = Gui->BeginMenu("File");
		FileMenuBounds = Gui->LastItemBounds();
		if (bFileOpen)
		{
			if (Gui->MenuItem("Open Scene..."))
			{
				ShowOpenScene();
			}
			OpenMenuBounds = Gui->LastItemBounds();
			Gui->Separator();
			if (Gui->MenuItem("Exit"))
			{
				Window->RequestClose();
			}
			Gui->EndMenu();
		}
		if (Gui->BeginMenu("Window"))
		{
			if (Gui->MenuItem("Viewport", nullptr, bShowViewport))
			{
				bShowViewport = !bShowViewport;
			}
			if (Gui->MenuItem("Outliner", nullptr, bShowOutliner))
			{
				bShowOutliner = !bShowOutliner;
			}
			if (Gui->MenuItem("Details", nullptr, bShowDetails))
			{
				bShowDetails = !bShowDetails;
			}
			if (Gui->MenuItem("Content Browser", nullptr, bShowBrowser))
			{
				bShowBrowser = !bShowBrowser;
			}
			Gui->Separator();
			if (Gui->MenuItem("Reset Layout"))
			{
				bResetLayout = true;
				bShowViewport = bShowOutliner = bShowDetails = bShowBrowser = true;
			}
			Gui->EndMenu();
		}
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
		          (CurrentPath.empty() ? std::string("Untitled") : std::filesystem::path(CurrentPath).stem().string()));
		Gui->EndMenuBar();
	}
}

void FEditorApplication::DrawToolbar()
{
	if (Gui->BeginToolbar())
	{
		if (Gui->Button("Open Scene"))
		{
			ShowOpenScene();
		}
		Gui->SameLine();
		if (Gui->Button("Frame Scene", Scene->GetStatus().bReady && !CurrentPath.empty()))
		{
			FitSceneCamera(*Scene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
		}
		Gui->SameLine();
		Gui->Text("  |  Scene Editor");
	}
	Gui->EndToolbar();
}

void FEditorApplication::DrawNode(FSceneHandle InHandle)
{
	const auto* Node = Scene->FindNode(InHandle);
	if (!Node)
	{
		return;
	}
	const auto Children = Scene->GetChildren(InHandle);
	Gui->NextRow();
	Gui->NextColumn();
	bool bClicked{};
	const bool bOpen =
	    Gui->TreeItem(Node->Id.c_str(), Node->Name.c_str(), Children.empty(), Selection == InHandle, bClicked);
	if (bClicked)
	{
		Selection = InHandle;
	}
	Gui->NextColumn();
	Gui->Text(KindName(Node->GetKind()));
	if (bOpen)
	{
		for (const auto Child : Children)
		{
			DrawNode(Child);
		}
		Gui->EndTree();
	}
}

void FEditorApplication::DrawOutliner()
{
	if (!bShowOutliner)
	{
		return;
	}
	if (Gui->BeginWindow("Outliner", bShowOutliner))
	{
		if (!Selection && Scene->GetStatus().bReady)
		{
			const auto Models = Scene->GetNodes(ESceneNodeKind::Model);
			if (!Models.empty())
			{
				Selection = Models.front();
			}
		}
		Gui->Text("Search objects");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##SearchObjects", Filter, false);
		Gui->Text(std::to_string(Scene->GetNodes().size()) + " objects" + (Selection ? "  |  1 selected" : ""));
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
					if (Gui->Selectable((Node->Name + "##" + Node->Id).c_str(), Selection == Handle))
					{
						Selection = Handle;
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

void FEditorApplication::DrawDetails()
{
	if (!bShowDetails)
	{
		return;
	}
	if (Gui->BeginWindow("Details", bShowDetails))
	{
		FSceneNodeView View;
		if (Selection && Scene->GetNodeView(*Selection, View))
		{
			const auto& Node = *View.Node;
			Gui->Text(Node.Name);
			Gui->Text(KindName(Node.GetKind()));
			Gui->Separator();
			if (Gui->Section("Transform"))
			{
				Gui->Property("Location X", Number(View.World.Values[12]));
				Gui->Property("Location Y", Number(View.World.Values[13]));
				Gui->Property("Location Z", Number(View.World.Values[14]));
			}
			if (Gui->Section("Object"))
			{
				Gui->Property("Enabled", View.bEffectiveEnabled ? "Yes" : "No");
				Gui->Property("Type", KindName(Node.GetKind()));
				Gui->TextWrapped("ID: " + Node.Id);
			}
			if (Node.Model && Gui->Section("Static Mesh"))
			{
				Gui->TextWrapped(Node.Model->Asset);
				Gui->Property("Visible", Node.Model->bVisible ? "Yes" : "No");
				Gui->Property("Resource", Node.Model->Data ? "Ready" : "Loading");
			}
			if (Node.Camera && Gui->Section("Camera"))
			{
				Gui->Property("Near plane", Number(Node.Camera->Near));
				Gui->Property("Far plane", Number(Node.Camera->Far));
				Gui->Property("Focus distance", Number(Node.Camera->FocusDistance));
			}
		}
		else
		{
			Gui->TextWrapped("Select an object in the Outliner to inspect its properties.");
		}
	}
	Gui->EndWindow();
}

void FEditorApplication::DrawSceneBrowser()
{
	if (!bShowBrowser)
	{
		return;
	}
	if (Gui->BeginWindow("Content Browser", bShowBrowser))
	{
		Gui->Text("Scenes");
		Gui->SameLine();
		if (Gui->Button("Browse / Open..."))
		{
			ShowOpenScene();
		}
		Gui->Separator();
		for (const auto& Path : ScenePaths)
		{
			const auto Label = std::filesystem::path(Path).stem().string() + "##Content" + Path;
			if (Gui->Selectable(Label.c_str(), OpenPath == Path))
			{
				OpenPath = Path;
			}
		}
	}
	Gui->EndWindow();
}

void FEditorApplication::DrawOpenDialog()
{
	if (bRequestOpen)
	{
		Gui->OpenPopup("Open Scene");
		bRequestOpen = false;
	}
	if (Gui->BeginModal("Open Scene", bOpenDialog))
	{
		Gui->Text("Choose a scene from mounted content");
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##FilterScenes", SceneFilter, false);
		Gui->BeginScrollRegion("SceneList", 250);
		for (const auto& Path : ScenePaths)
		{
			if (!Matches(Path, SceneFilter))
			{
				continue;
			}
			if (Gui->Selectable(Path.c_str(), OpenPath == Path))
			{
				OpenPath = Path;
			}
			if (Path.ends_with("/Sponza.hasset"))
			{
				SponzaBounds = Gui->LastItemBounds();
			}
		}
		if (ScenePaths.empty())
		{
			Gui->TextWrapped("No catalog scenes found. Enter a mounted scene path below.");
		}
		Gui->EndScrollRegion();
		Gui->SetNextItemWidth(-1);
		Gui->InputText("##ScenePath", OpenPath, false);
		if (Gui->Button("Open", !OpenPath.empty()))
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

void FEditorApplication::DrawViewport()
{
	bViewportVisible = false;
	ViewportRegion = {};
	if (!bShowViewport)
	{
		return;
	}
	if (Gui->BeginWindow("Viewport", bShowViewport))
	{
		const float Speed = Camera.GetMovementSpeed(*Scene);
		Gui->Text("Perspective  |  Camera Speed " + (Speed > 0 ? Number(Speed, 3) + " u/s" : "--") + "  |  Lit");
		Gui->SameLine();
		Gui->SetNextItemWidth(120);
		Gui->Slider("Exposure", Exposure, .1f, 8);
		Gui->SameLine();
		Gui->Text(CurrentPath.empty() ? "Open a scene to begin" : std::filesystem::path(CurrentPath).stem().string());
		ViewportRegion = Gui->Image(2);
		bViewportVisible = true;
	}
	Gui->EndWindow();
}

std::string FEditorApplication::StatusText() const
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

FGuiDrawData FEditorApplication::DrawGui(float InDelta, std::span<const FInputEvent> InEvents)
{
	Gui->BeginFrame(Window->LogicalSize(), Window->PixelSize(), InDelta, InEvents);
	DrawMenus();
	DrawToolbar();
	Gui->StatusBar(StatusText());
	Gui->DockSpace({"Viewport", "Outliner", "Details", "Content Browser"}, bResetLayout);
	bResetLayout = false;
	DrawViewport();
	DrawOutliner();
	DrawDetails();
	DrawSceneBrowser();
	DrawOpenDialog();
	return Gui->Render();
}
} // namespace Hyperion
