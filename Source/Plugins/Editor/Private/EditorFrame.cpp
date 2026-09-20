#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
void FEditorPlugin::InitializeViewportCamera()
{
	if (bViewportCameraInitialized || !CanInitializeSceneBrowsingView(*Scene))
	{
		return;
	}
	ViewCamera =
	    MakeSceneBrowsingView(*Scene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
	bViewportCameraInitialized = true;
}

void FEditorPlugin::ResizeViewport()
{
	if (!bViewportVisible)
	{
		return;
	}
	const auto Logical = Window->LogicalSize();
	const auto Pixels = Window->PixelSize();
	const auto& Bounds = ViewportRegion.Bounds;
	const auto Limit = Device->GetCapabilities().MaxTextureDimension;
	const FSize Size{std::clamp(static_cast<std::uint32_t>(
	                                std::max(1.f, std::round((Bounds.Z - Bounds.X) * Pixels.Width / Logical.Width))),
	                            1u, Limit),
	                 std::clamp(static_cast<std::uint32_t>(
	                                std::max(1.f, std::round((Bounds.W - Bounds.Y) * Pixels.Height / Logical.Height))),
	                            1u, Limit)};
	if (Size.Width == ViewportSize.Width && Size.Height == ViewportSize.Height)
	{
		return;
	}
	ViewportSize = Size;
	ViewportTarget = {ERenderTargetKind::Texture,
	                  std::make_shared<const FMaterialTextureSource>(
	                      FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::Rgba8Unorm}),
	                  Session->GetResources().CreateScopeLifetime(), false};
}

void FEditorPlugin::RouteCamera(float InDelta, std::span<const FInputEvent> InEvents)
{
	if (Placement.IsActive() || bPlacementUsedMouse || Gizmo.IsDragging() || bGizmoUsedMouse || PreviewCamera ||
	    !bViewportCameraInitialized || !bViewportVisible || bOpenDialog || bSaveDialog || bDiscardDialog ||
	    Gui->IsEditingText() || !ViewportRegion.bFocused)
	{
		// Focus recovery still reaches the controller while GUI navigation is blocked.
		Camera.SuspendInput(InEvents);
		bCameraDragging = false;
		return;
	}
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::MouseButton && Event.Button == 1)
		{
			bCameraDragging = Event.bDown && (ViewportRegion.bHovered || bCameraDragging);
		}
		if (Event.Type == EEventType::Focus && !Event.bDown)
		{
			bCameraDragging = false;
			Camera.Reset();
		}
		if (Event.Type == EEventType::Key && Event.Key == EKey::Home && Event.bDown && !Event.bRepeat)
		{
			FitSceneCamera(ViewCamera, *Scene,
			               ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
		}
	}
	Camera.Input(ViewCamera, InEvents, !ViewportRegion.bHovered && !bCameraDragging, false);
	Camera.Advance(ViewCamera, InDelta);
}

void FEditorPlugin::Render(FGuiDrawData InGui, bool bInCapture)
{
	ResizeViewport();
	const auto Size = Window->PixelSize();
	FSceneViewRequest Request;
	Request.Width = ViewportSize.Width;
	Request.Height = ViewportSize.Height;
	Request.DepthConvention = EDepthConvention::Reversed;
	if (PreviewCamera)
	{
		Request.Camera = PreviewCamera;
		Request.bAllowCameraFallback = false;
	}
	else
	{
		Request.CameraOverride = ViewCamera;
	}
	const auto Seed =
	    Session->FreezeSceneFrame(Scene->GetToken(), static_cast<float>(double(ClockNanoseconds()) / 1e9));
	const auto Target = ViewportTarget;
	const bool bRenderScene = bViewportVisible;
	const auto Preview = FreezePlacementPreview();
	std::vector<FGuiTextureBinding> IconTextures;
	for (const auto& [Id, Icon] : PlacementIcons)
	{
		if (Icon.Source.Texture)
		{
			IconTextures.push_back({Icon.Texture, Icon.Source});
		}
	}
	FImage Capture;
	const bool bCaptureFrame = bInCapture || !PlacementCapture.empty();
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&, Data = std::move(InGui), Preview, IconTextures = std::move(IconTextures)]() mutable
	                          {
		                          FRenderGraph Graph;
		                          auto Textures = std::move(IconTextures);
		                          if (bRenderScene)
		                          {
			                          FScenePipelineSettings Settings;
			                          Settings.Exposure = Exposure;
			                          Pipeline->Configure(Settings);
			                          Pipeline->SetOutputTarget(Target);
			                          Pipeline->SetTransientGeometry(Preview);
			                          Pipeline->Build(Graph, Request, Seed, {}, {.13f, .13f, .13f, 1}, {}, true);
			                          Textures.push_back({2, Target});
		                          }
		                          GuiRenderer->BuildDeferred(Graph, std::move(Data), std::move(Textures), true);
		                          Capture =
		                              ExecuteGraph(std::move(Graph), Tasks, *Swapchain, Size,
		                                           !Options.bExercise && Options.Benchmark.empty(), bCaptureFrame);
		                          if (bRenderScene)
		                          {
			                          RenderStats = Pipeline->GetFrame().Statistics();
		                          }
	                          }));
	if (bInCapture)
	{
		if (!Options.Capture.parent_path().empty())
		{
			std::filesystem::create_directories(Options.Capture.parent_path());
		}
		SaveImage(Options.Capture, Capture);
	}
	if (!PlacementCapture.empty())
	{
		std::filesystem::create_directories(PlacementCapture.parent_path());
		SaveImage(PlacementCapture, Capture);
		PlacementCapture.clear();
	}
}
} // namespace Hyperion
