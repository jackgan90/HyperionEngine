#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace Hyperion
{
void FEditorPlugin::InitializeViewportCamera()
{
	Viewport.InitializeCamera(*Scene);
}

void FEditorPlugin::ResizeViewport()
{
	Viewport.Resize(Window->LogicalSize(), Window->PixelSize(), Device->GetCapabilities().MaxTextureDimension, *Session,
	                Options.BenchmarkViewport);
}

void FEditorPlugin::RouteCamera(float InDelta, std::span<const FInputEvent> InEvents)
{
	if (!CaptureInteractionPolicy().AllowsCameraNavigation())
	{
		// Focus recovery still reaches the controller while GUI navigation is blocked.
		Camera.SuspendInput(InEvents);
		Viewport.bCameraDragging = false;
		return;
	}
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::MouseButton && Event.Button == InputButtons::Right)
		{
			Viewport.bCameraDragging = Event.bDown && (Viewport.ViewportRegion.bHovered || Viewport.bCameraDragging);
		}
		if (Event.Type == EEventType::Focus && !Event.bDown)
		{
			Viewport.bCameraDragging = false;
			Camera.Reset();
		}
		if (Event.Type == EEventType::Key && Event.Key == EKey::Home && Event.bDown && !Event.bRepeat)
		{
			if (Scene->GetStatus().bReady)
			{
				FrameScene();
			}
		}
	}
	Camera.Input(Viewport.ViewCamera, InEvents, !Viewport.ViewportRegion.bHovered && !Viewport.bCameraDragging, false);
	Camera.Advance(Viewport.ViewCamera, InDelta);
}

void FEditorPlugin::Render(FGuiDrawData InGui, bool bInCapture)
{
	ResizeViewport();
	const auto Size = Window->PixelSize();
	const auto FrameSettings = Rendering;
	const bool bVsync = FrameSettings.bVsync && Acceptance.Policy().bUseVsync && Options.Benchmark.empty();
	const auto Request = Viewport.MakeViewRequest(FrameSettings, CullingMode, FrozenCullingView, bInstanceBatching);
	const auto& Status = Scene->GetStatus();
	const bool bRenderScene = Viewport.bViewportVisible && Status.Error.empty() && Status.PublicationError.empty();
	if (!bRenderScene)
	{
		// Scene IO can fail after GUI construction. Keep the window chrome, without sampling an unavailable target.
		std::erase_if(InGui.Commands,
		              [](const FGuiCommand& InCommand)
		              {
			              return InCommand.TextureId == 2;
		              });
	}
	const auto Seed = bRenderScene ? Session->FreezeSceneFrame(Scene->GetToken(),
	                                                           static_cast<float>(double(ClockNanoseconds()) / 1e9))
	                               : nullptr;
	const auto Target = Viewport.ViewportTarget;
	const auto Outline = MakeSelectionOutline(Seed);
	const auto Preview = FreezePlacementPreview();
	Acceptance.ObservePlacement(Preview);
	std::vector<FGuiTextureBinding> IconTextures;
	for (const auto& [Id, Icon] : PlacementIcons)
	{
		if (Icon.Source.Texture)
		{
			IconTextures.push_back({Icon.Texture, Icon.Source});
		}
	}
	FImage Capture;
	const auto AuxiliaryCapture = Acceptance.MainCapture();
	const bool bAgentCapture = PendingImage && PendingImage->Request.Window == "main";
	const bool bCaptureFrame = bInCapture || !AuxiliaryCapture.empty() || bAgentCapture;
	const auto Surface = Window->Surface();
	const bool bCaptureRdc = std::exchange(bCaptureRequested, false);
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&, Data = std::move(InGui), Preview, Outline, IconTextures = std::move(IconTextures),
	                           Surface, bCaptureRdc, bVsync, Request, FrameSettings]() mutable
	                          {
		                          FRenderGraph Graph;
		                          auto Textures = std::move(IconTextures);
		                          if (bRenderScene)
		                          {
			                          const auto Settings = MakePipelineSettings(FrameSettings);
			                          Pipeline->Configure(Settings);
			                          Pipeline->SetOutputTarget(Target);
			                          Pipeline->SetTransientGeometry(Preview);
			                          Pipeline->SetSelectionOutline(Outline);
			                          auto Shadows = FrameSettings.Shadows;
			                          const float PreviewSize = float(std::min({256u, Request.Width, Request.Height}));
			                          Shadows.PreviewViewport = {0, 0, PreviewSize, PreviewSize};
			                          Pipeline->Build(Graph, Request, Seed, Shadows, {.13f, .13f, .13f, 1}, {}, true);
			                          Textures.push_back({2, Target});
		                          }
		                          GuiRenderer->BuildDeferred(Graph, std::move(Data), std::move(Textures), true);
		                          Capture = ExecuteEditorGraph(std::move(Graph), Size, bCaptureFrame, Surface,
		                                                       bCaptureRdc, bVsync);
		                          if (bRenderScene)
		                          {
			                          RenderStats = Pipeline->GetFrame().Statistics();
		                          }
	                          }));
	CompleteFrameCapture(Capture, bInCapture, bAgentCapture, AuxiliaryCapture);
}

std::shared_ptr<FSelectionOutlineRequest> FEditorPlugin::MakeSelectionOutline(
    const std::shared_ptr<const FSceneFrameSeed>& InSeed) const
{
	auto Outline = std::make_shared<FSelectionOutlineRequest>();
	if (InSeed)
	{
		Outline->Publication = InSeed->GetToken();
	}
	Outline->Settings = OutlineSettings;
	for (const auto Handle : Acceptance.OutlineSelection(Selection.All()))
	{
		Outline->Objects.push_back(Scene->ResolveRenderPrimitives(Handle));
	}
	return Outline;
}

void FEditorPlugin::CompleteFrameCapture(const FImage& InImage, bool bInCapture, bool bInAgentCapture,
                                         const std::filesystem::path& InAuxiliaryCapture)
{
	if (bInCapture)
	{
		if (!Options.Capture.parent_path().empty())
		{
			std::filesystem::create_directories(Options.Capture.parent_path());
		}
		SaveImage(Options.Capture, InImage);
	}
	if (bInAgentCapture)
	{
		CompleteImageOutput(*PendingImage, InImage, FrameCount);
		PendingImage.reset();
	}
	Acceptance.CompleteCapture(InImage, InAuxiliaryCapture);
}
} // namespace Hyperion
