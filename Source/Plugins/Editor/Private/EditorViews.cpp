#include "EditorApplication.h"
#include "Hyperion/RasterOptions/RasterOptions.h"
#include "Hyperion/Renderer/ViewportChoices.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Hyperion
{
namespace
{
void DrawCullingChoice(FGui& InGui, FEditorAcceptanceDriver& InAcceptance, FSceneViewportOptions& OutOptions)
{
	const auto Options = SceneCullingOptions();
	static const auto Labels = RasterOptionLabels(Options);
	auto Index = RasterOptionIndex(Options, ParseSceneCullingMode(*OutOptions.Culling));
	if (InGui.Combo("Culling", Labels, Index,
	                [&](std::size_t InIndex, FVec4 InBounds)
	                {
		                InAcceptance.ObserveIndexedWidget(EEditorWidget::CullingModeItem, InBounds,
		                                                  ToCullingWireValue(Options[InIndex].Id));
	                }))
	{
		OutOptions.Culling = ToCullingWireValue(RasterOptionIdentity(Options, Index));
	}
	InAcceptance.ObserveWidget(EEditorWidget::CullingMode, InGui.LastItemBounds());
}

void DrawOutlineChoice(FGui& InGui, FEditorAcceptanceDriver& InAcceptance, FSceneViewportOptions& OutOptions)
{
	const auto Options = OutlineOverlapOptions();
	static const auto Labels = RasterOptionLabels(Options);
	auto Index = RasterOptionIndex(Options, ParseOutlineOverlapMode(*OutOptions.OutlineMode));
	InGui.SetNextItemWidth(180);
	if (InGui.Combo("Selection outline", Labels, Index,
	                [&](std::size_t InIndex, FVec4 InBounds)
	                {
		                InAcceptance.ObserveIndexedWidget(EEditorWidget::OutlineModeItem, InBounds,
		                                                  ToOutlineWireValue(Options[InIndex].Id));
	                }))
	{
		OutOptions.OutlineMode = ToOutlineWireValue(RasterOptionIdentity(Options, Index));
	}
	InAcceptance.ObserveWidget(EEditorWidget::OutlineMode, InGui.LastItemBounds());
	InGui.Tooltip("Union outlines the selected group. Per object preserves every object's outline through overlaps.");
}
} // namespace

void FEditorPlugin::SetPreviewCamera(std::optional<FSceneHandle> InHandle)
{
	CancelPlacement();
	ViewportClick.reset();
	if (InHandle)
	{
		FinishInspectorEdit();
		SceneDocument.ReplaceSelection(FSceneSelection(InHandle));
	}
	Viewport.PreviewCamera = InHandle;
	Camera.Reset();
	Viewport.bCameraDragging = false;
}

bool FEditorPlugin::IsPreviewAvailable() const
{
	FSceneNodeView View;
	return Viewport.PreviewCamera && Scene->GetNodeView(*Viewport.PreviewCamera, View) && View.bEffectiveEnabled &&
	       View.Node->Camera();
}

void FEditorPlugin::SetInitialView()
{
	if (!Viewport.bViewportCameraInitialized || Viewport.PreviewCamera)
	{
		throw std::runtime_error("Return to the editor view before setting the initial view");
	}
	auto Settings = Scene->GetSettings();
	Settings.InitialView = Viewport.ViewCamera;
	CommitSettings(std::move(Settings));
}

void FEditorPlugin::ApplyEditorView(FSceneHandle InHandle)
{
	const auto* Node = Scene->FindNode(InHandle);
	if (!Viewport.bViewportCameraInitialized || !Node || !Node->Camera())
	{
		throw std::runtime_error("Select an available camera first");
	}
	auto Candidate = *Node;
	Candidate.Camera() = Viewport.ViewCamera.Lens;
	Candidate.Local() = Viewport.ViewCamera.World;
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
		Candidate.Local() = Multiply(ParentInverse, Viewport.ViewCamera.World);
	}
	CommitEdit(InHandle, std::move(Candidate), Scene->GetRevision());
}

void FEditorPlugin::CreateCameraFromView()
{
	FinishInspectorEdit();
	if (!Viewport.bViewportCameraInitialized || Viewport.PreviewCamera)
	{
		throw std::runtime_error("Return to the editor view before creating a camera");
	}
	FSceneNode Node;
	Node.Name = "Camera";
	Node.Local() = Viewport.ViewCamera.World;
	Node.Camera() = Viewport.ViewCamera.Lens;
	CommitCreate(std::move(Node));
}

void FEditorPlugin::DrawViewControls()
{
	if (Gui->IconButton("##ViewOptions", EGuiIcon::Options, "Viewport options - camera and view actions",
	                    bViewOptionsOpen))
	{
		Gui->OpenPopup("ViewportOptions");
	}
	Acceptance.ObserveWidget(EEditorWidget::ViewOptions, Gui->LastItemBounds());
	DrawCaptureButton();
	DrawHudButtons();
	DrawVisualizationControls();
	DrawProfilingOptions();
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
				if (Viewport.PreviewCamera == Handle)
				{
					Index = Labels.size();
				}
				Labels.push_back(Node->Name + " (" + Node->Id + ")");
				Cameras.push_back(Handle);
			}
		}
		if (Viewport.PreviewCamera && Index == 0)
		{
			Index = Labels.size();
			Labels.push_back("Unavailable camera");
			Cameras.push_back(Viewport.PreviewCamera);
		}
		Gui->SetNextItemWidth(InWidth);
		if (Gui->Combo("##ViewSource", Labels, Index))
		{
			SetPreviewCamera(Cameras.at(Index));
		}
		Gui->Tooltip(Viewport.PreviewCamera ? "View source - camera preview" : "View source - editor perspective");
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
	Gui->Text("Perspective");
	Gui->Text("View source");
	DrawViewSelector(240);
	std::ostringstream Speed;
	Speed << "Camera speed: " << std::fixed << std::setprecision(3) << Camera.GetMovementSpeed(Viewport.ViewCamera)
	      << " u/s";
	Gui->Text(Speed.str());
	Gui->Tooltip("Hold right mouse and scroll to adjust movement speed");
	auto ViewOptions = ViewportState().Options;
	Gui->Checkbox("Show light icons", *ViewOptions.LightMarkers);
	DrawCullingChoice(*Gui, Acceptance, ViewOptions);
	Gui->Checkbox("Freeze culling view", *ViewOptions.Frozen);
	Gui->Checkbox("Instance batching", *ViewOptions.InstanceBatching);
	Gui->Checkbox("Model bounds", *ViewOptions.ModelBounds);
	Gui->Checkbox("Light influence", *ViewOptions.LightBounds);
	DrawOutlineChoice(*Gui, Acceptance, ViewOptions);
	Gui->Checkbox("Smooth outlines (2x)", *ViewOptions.SmoothOutlines);
	try
	{
		SetViewportOptions(ViewOptions);
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
	}
	Acceptance.ObserveWidget(EEditorWidget::OutlineQuality, Gui->LastItemBounds());
	Gui->Separator();
	try
	{
		if (Viewport.PreviewCamera)
		{
			if (Gui->Button("Return to editor view"))
			{
				SetPreviewCamera({});
				Gui->ClosePopup();
			}
			Acceptance.ObserveWidget(EEditorWidget::ReturnToEditorView, Gui->LastItemBounds());
			Gui->TextWrapped(IsPreviewAvailable()
			                     ? "Camera preview: edit the selected camera's properties to change this view."
			                     : "Camera unavailable: disabled, removed, or missing its Camera component.");
		}
		else
		{
			if (Gui->Button("Set initial view", Viewport.bViewportCameraInitialized))
			{
				SetInitialView();
				Gui->ClosePopup();
			}
			Acceptance.ObserveWidget(EEditorWidget::InitialView, Gui->LastItemBounds());
			if (Gui->Button("Create camera from view", Viewport.bViewportCameraInitialized))
			{
				CreateCameraFromView();
				Gui->ClosePopup();
			}
			Acceptance.ObserveWidget(EEditorWidget::CreateCamera, Gui->LastItemBounds());
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
	Acceptance.ObserveWidget(EEditorWidget::PreviewCamera, Gui->LastItemBounds());
	Gui->SameLineIfFits("Apply editor view to camera");
	if (Gui->Button("Apply editor view to camera", Viewport.bViewportCameraInitialized))
	{
		ApplyEditorView(InHandle);
	}
	Acceptance.ObserveWidget(EEditorWidget::ApplyCamera, Gui->LastItemBounds());
}
} // namespace Hyperion
