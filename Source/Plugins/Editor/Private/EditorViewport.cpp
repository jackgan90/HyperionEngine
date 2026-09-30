#include "EditorViewport.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
void FEditorViewport::InitializeCamera(const FSceneInstance& InScene)
{
	if (bViewportCameraInitialized || !CanInitializeSceneBrowsingView(InScene))
	{
		return;
	}
	ViewCamera =
	    MakeSceneBrowsingView(InScene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1);
	bViewportCameraInitialized = true;
}

void FEditorViewport::Resize(FSize InLogical, FSize InPixels, std::uint32_t InLimit, FRenderSession& InSession,
                             FSize InFixed)
{
	if (!bViewportVisible)
	{
		return;
	}
	const auto Logical = InLogical;
	const auto Pixels = InPixels;
	const auto& Bounds = ViewportRegion.Bounds;
	const auto Limit = InLimit;
	FSize Size{std::clamp(static_cast<std::uint32_t>(
	                          std::max(1.f, std::round((Bounds.Z - Bounds.X) * Pixels.Width / Logical.Width))),
	                      1u, Limit),
	           std::clamp(static_cast<std::uint32_t>(
	                          std::max(1.f, std::round((Bounds.W - Bounds.Y) * Pixels.Height / Logical.Height))),
	                      1u, Limit)};
	if (InFixed.Width)
	{
		Size = InFixed;
		if (Size.Width > Limit || Size.Height > Limit)
		{
			throw std::invalid_argument("Benchmark viewport exceeds device limits");
		}
	}
	if (Size.Width == ViewportSize.Width && Size.Height == ViewportSize.Height)
	{
		return;
	}
	ViewportSize = Size;
	ViewportTarget = {ERenderTargetKind::Texture,
	                  std::make_shared<const FMaterialTextureSource>(
	                      FMaterialColorTexture{Size.Width, Size.Height, EMaterialColorFormat::Rgba8Unorm}),
	                  InSession.GetResources().CreateScopeLifetime(), false};
}

void FEditorViewport::FrameScene(const FSceneInstance& InScene)
{
	if (!InScene.GetStatus().bReady || PreviewCamera)
	{
		throw FSceneEditError("busy", "Frame scene requires a ready scene and the editor browsing view");
	}
	auto Candidate = ViewCamera;
	FitSceneCamera(Candidate, InScene, ViewportSize.Height ? float(ViewportSize.Width) / ViewportSize.Height : 1, true);
	Candidate.Lens.Far = std::max(ViewCamera.Lens.Far, Candidate.Lens.Far);
	ViewCamera = Candidate;
	Navigation.Reset();
}

void FEditorViewport::ResetNavigation()
{
	Navigation.Reset();
	bCameraDragging = false;
	bViewportCameraInitialized = false;
}

FSceneViewRequest FEditorViewport::MakeViewRequest(const FRenderSettings& InSettings, ESceneCullingMode InCulling,
                                                   const std::optional<FMat4>& InFrozenCulling, bool bInBatching) const
{
	FSceneViewRequest Request;
	Request.Width = ViewportSize.Width;
	Request.Height = ViewportSize.Height;
	Request.DepthConvention = GetDepthConvention(InSettings.bReversedZ);
	Request.CullingMode = InCulling;
	Request.CullingViewProjection = InFrozenCulling;
	Request.bInstanceBatching = bInBatching;
	if (PreviewCamera)
	{
		Request.Camera = PreviewCamera;
		Request.bAllowCameraFallback = false;
	}
	else
	{
		Request.CameraOverride = ViewCamera;
	}
	return Request;
}
} // namespace Hyperion
