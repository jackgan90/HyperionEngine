#include "EditorApplication.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Hyperion
{
void FEditorPlugin::SetPreviewCamera(std::optional<FSceneHandle> InHandle)
{
	CancelPlacement();
	ViewportClick.reset();
	if (InHandle)
	{
		FinishInspectorEdit();
		Selection = InHandle;
	}
	PreviewCamera = InHandle;
	Camera.Reset();
	bCameraDragging = false;
}

bool FEditorPlugin::IsPreviewAvailable() const
{
	FSceneNodeView View;
	return PreviewCamera && Scene->GetNodeView(*PreviewCamera, View) && View.bEffectiveEnabled && View.Node->Camera();
}

void FEditorPlugin::SetInitialView()
{
	if (!bViewportCameraInitialized || PreviewCamera)
	{
		throw std::runtime_error("Return to the editor view before setting the initial view");
	}
	auto Settings = Scene->GetSettings();
	Settings.InitialView = ViewCamera;
	CommitSettings(std::move(Settings));
}

void FEditorPlugin::ApplyEditorView(FSceneHandle InHandle)
{
	const auto* Node = Scene->FindNode(InHandle);
	if (!bViewportCameraInitialized || !Node || !Node->Camera())
	{
		throw std::runtime_error("Select an available camera first");
	}
	auto Candidate = *Node;
	Candidate.Camera() = ViewCamera.Lens;
	Candidate.Local() = ViewCamera.World;
	if (!Candidate.Parent().empty())
	{
		FSceneNodeView Parent;
		if (!Scene->GetNodeView(Scene->FindHandle(Candidate.Parent()), Parent))
		{
			throw std::runtime_error("Camera parent no longer exists");
		}
		const auto ParentInverse = Inverse(Parent.World);
		if (!IsAffine(ParentInverse))
		{
			throw std::runtime_error("Cannot apply a world view under a singular parent transform");
		}
		Candidate.Local() = Multiply(ParentInverse, ViewCamera.World);
	}
	CommitEdit(InHandle, std::move(Candidate), Scene->GetRevision());
}

void FEditorPlugin::CreateCameraFromView()
{
	FinishInspectorEdit();
	if (!bViewportCameraInitialized || PreviewCamera)
	{
		throw std::runtime_error("Return to the editor view before creating a camera");
	}
	FSceneNode Node;
	Node.Name = "Camera";
	Node.Local() = ViewCamera.World;
	Node.Camera() = ViewCamera.Lens;
	CommitCreate(std::move(Node));
}

void FEditorPlugin::DrawViewControls()
{
	if (Gui->IconButton("##ViewOptions", EGuiIcon::Options, "Viewport options - camera, exposure and view actions",
	                    bViewOptionsOpen))
	{
		Gui->OpenPopup("ViewportOptions");
	}
	InspectionBounds["view/options"] = Gui->LastItemBounds();
	Gui->SameLine();
	const float Width = Gui->AvailableWidth();
	if (Width >= 60)
	{
		DrawViewSelector(std::min(180.f, Width));
	}
	else
	{
		Gui->Text("");
	}
	DrawViewOptions();
}

void FEditorPlugin::DrawViewSelector(float InWidth)
{
	try
	{
		std::vector<std::string> Labels{"Editor view"};
		std::vector<std::optional<FSceneHandle>> Cameras{std::nullopt};
		std::size_t Index{};
		for (const auto Handle : Scene->GetNodes())
		{
			const auto* Node = Scene->FindNode(Handle);
			if (Node && Node->Camera())
			{
				if (PreviewCamera == Handle)
				{
					Index = Labels.size();
				}
				Labels.push_back(Node->Name + " (" + Node->Id + ")");
				Cameras.push_back(Handle);
			}
		}
		if (PreviewCamera && Index == 0)
		{
			Index = Labels.size();
			Labels.push_back("Unavailable camera");
			Cameras.push_back(PreviewCamera);
		}
		Gui->SetNextItemWidth(InWidth);
		if (Gui->Combo("##ViewSource", Labels, Index))
		{
			SetPreviewCamera(Cameras.at(Index));
		}
		Gui->Tooltip(PreviewCamera ? "View source - camera preview" : "View source - editor perspective");
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
}

void FEditorPlugin::DrawViewOptions()
{
	bViewOptionsOpen = Gui->BeginPopup("ViewportOptions");
	if (!bViewOptionsOpen)
	{
		return;
	}
	Gui->Text("Perspective / Lit");
	Gui->Text("View source");
	DrawViewSelector(240);
	std::ostringstream Speed;
	Speed << "Camera speed: " << std::fixed << std::setprecision(3) << Camera.GetMovementSpeed(ViewCamera) << " u/s";
	Gui->Text(Speed.str());
	Gui->Tooltip("Hold right mouse and scroll to adjust movement speed");
	Gui->SetNextItemWidth(180);
	Gui->Slider("Exposure", Exposure, .1f, 8);
	Gui->Checkbox("Show light icons", bShowLightMarkers);
	const std::array<std::string, 2> OutlineModes{"Union", "Per object"};
	std::size_t OutlineMode = static_cast<std::size_t>(OutlineSettings.Overlap);
	Gui->SetNextItemWidth(180);
	if (Gui->Combo("Selection outline", OutlineModes, OutlineMode))
	{
		OutlineSettings.Overlap = static_cast<EOutlineOverlapMode>(OutlineMode);
	}
	InspectionBounds["outline/mode"] = Gui->LastItemBounds();
	Gui->Tooltip("Union outlines the selected group. Per object preserves every object's outline through overlaps.");
	Gui->Checkbox("Smooth outlines (2x)", OutlineSettings.bSupersample);
	InspectionBounds["outline/quality"] = Gui->LastItemBounds();
	Gui->Separator();
	try
	{
		if (PreviewCamera)
		{
			if (Gui->Button("Return to editor view"))
			{
				SetPreviewCamera({});
				Gui->ClosePopup();
			}
			InspectionBounds["view/return"] = Gui->LastItemBounds();
			Gui->TextWrapped(IsPreviewAvailable()
			                     ? "Camera preview: edit the selected camera's properties to change this view."
			                     : "Camera unavailable: disabled, removed, or missing its Camera component.");
		}
		else
		{
			if (Gui->Button("Set initial view", bViewportCameraInitialized))
			{
				SetInitialView();
				Gui->ClosePopup();
			}
			InspectionBounds["view/initial"] = Gui->LastItemBounds();
			if (Gui->Button("Create camera from view", bViewportCameraInitialized))
			{
				CreateCameraFromView();
				Gui->ClosePopup();
			}
			InspectionBounds["view/create"] = Gui->LastItemBounds();
		}
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
	Gui->EndPopup();
}

void FEditorPlugin::DrawCameraActions(FSceneHandle InHandle)
{
	if (Gui->Button("Preview camera"))
	{
		SetPreviewCamera(InHandle);
	}
	InspectionBounds["view/preview"] = Gui->LastItemBounds();
	Gui->SameLineIfFits("Apply editor view to camera");
	if (Gui->Button("Apply editor view to camera", bViewportCameraInitialized))
	{
		ApplyEditorView(InHandle);
	}
	InspectionBounds["view/apply"] = Gui->LastItemBounds();
}
} // namespace Hyperion
