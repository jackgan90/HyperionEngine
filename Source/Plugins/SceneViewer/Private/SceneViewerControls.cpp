#include "Hyperion/Renderer/SceneNavigation.h"
#include "SceneViewerInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
FSceneViewportState FSceneViewerPlugin::ViewportState() const
{
	const auto& P = *Impl;
	FSceneViewportOptions Options;
	Options.Culling = static_cast<std::uint32_t>(P.Mode);
	Options.Frozen = P.bFrozen;
	if (!P.bForceOrdinary)
	{
		Options.InstanceBatching = P.bInstanceBatching;
	}
	Options.ModelBounds = P.bBounds;
	Options.LightBounds = P.bLightBounds;
	Options.Animate = P.bAnimate;
	return {P.ViewCamera, {}, Options, P.CameraController.GetMovementSpeed(P.ViewCamera), P.bViewInitialized, false};
}

void FSceneViewerPlugin::SetViewportCamera(const FSceneCameraView& InCamera)
{
	ValidateSceneCameraView(InCamera);
	Impl->ViewCamera = InCamera;
	Impl->bViewInitialized = true;
	Impl->CameraController.Reset();
}

void FSceneViewerPlugin::FrameScene()
{
	Fit();
}

void FSceneViewerPlugin::SetViewportOptions(const FSceneViewportOptions& InOptions)
{
	ValidateViewportOptions(InOptions, ViewportState().Options);
	auto& P = *Impl;
	if (InOptions.Culling)
	{
		SetCullingMode(static_cast<ESceneCullingMode>(*InOptions.Culling));
	}
	if (InOptions.Frozen)
	{
		SetFrozen(*InOptions.Frozen);
	}
	P.bInstanceBatching = InOptions.InstanceBatching.value_or(P.bInstanceBatching);
	P.bBounds = InOptions.ModelBounds.value_or(P.bBounds);
	P.bLightBounds = InOptions.LightBounds.value_or(P.bLightBounds);
	P.bAnimate = InOptions.Animate.value_or(P.bAnimate);
}

void FSceneViewerPlugin::FImpl::UpdateCamera(FRenderFrame& InFrame)
{
	const auto CurrentManifest = Scene.GetManifest();
	if (CurrentManifest != ViewManifest)
	{
		ViewManifest = CurrentManifest;
		bViewInitialized = false;
		ViewCamera = {};
		CameraController.Reset();
	}
	if (!bViewInitialized && CanInitializeSceneBrowsingView(Scene))
	{
		ViewCamera = MakeSceneBrowsingView(Scene, float(InFrame.Size.Width) / std::max(1u, InFrame.Size.Height));
		CameraController.Reset();
		bViewInitialized = true;
	}
	FSceneViewRequest Request;
	Request.CameraOverride = ViewCamera;
	Request.Width = InFrame.Size.Width;
	Request.Height = InFrame.Size.Height;
	Request.DepthConvention = InFrame.View.DepthConvention;
	Request.CullingMode = Mode;
	Request.bInstanceBatching = bInstanceBatching;
	if (bFrozen)
	{
		Request.CullingViewProjection = FrozenView;
	}
	InFrame.SceneView = std::move(Request);
}

void FSceneViewerPlugin::SetRenderedView(const std::optional<FRenderView>& InView)
{
	Impl->Tasks.Require({EDomain::Main});
	Impl->LastView = InView.value_or(FRenderView{});
}

void FSceneViewerPlugin::SetCullingMode(ESceneCullingMode InMode)
{
	Impl->Tasks.Require({EDomain::Main});
	Impl->Mode = InMode;
}

void FSceneViewerPlugin::SetFrozen(bool bInFrozen)
{
	Impl->Tasks.Require({EDomain::Main});
	if (bInFrozen && !Impl->bFrozen)
	{
		Impl->FrozenView = Impl->LastView.ViewProjection;
	}
	Impl->bFrozen = bInFrozen;
}

void FSceneViewerPlugin::Fit()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	FitSceneCamera(P.ViewCamera, P.Scene, float(P.LastView.Width) / std::max(1u, P.LastView.Height), true);
}

void FSceneViewerPlugin::DuplicateSelected()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (const auto Copy = P.Document.CommitDuplicate(P.Selected); Copy.Scene)
	{
		P.Selected = Copy;
	}
}

void FSceneViewerPlugin::AddModel()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	for (const auto& Asset : P.Scene.GetAssets())
	{
		if (Asset.Data && Asset.Error.empty())
		{
			const auto Pose = ExtractScenePose(P.ViewCamera.World);
			PlaceObject({P.Document.Id(), P.Scene.GetRevision(), Asset.Id,
			             Add(Pose.Eye, ScaleVector(Pose.Forward, P.ViewCamera.Lens.FocusDistance))});
			return;
		}
	}
}

FPlacementCatalog FSceneViewerPlugin::PlacementCatalog() const
{
	FPlacementCatalog Result;
	for (const auto& Asset : Impl->Scene.GetAssets())
	{
		Result.Items.push_back({Asset.Id,
		                        Asset.Id,
		                        {"Loaded models"},
		                        Asset.Error.empty() && !Asset.Data ? "Preparing model" : Asset.Error});
	}
	return Result;
}

std::optional<FSceneNodeInfo> FSceneViewerPlugin::PlaceObject(const FScenePlacementRequest& InRequest)
{
	auto& P = *Impl;
	P.Document.RequireIdle(InRequest.Document, InRequest.Revision);
	if (!IsFinite(InRequest.Position))
	{
		throw std::invalid_argument("Position must be finite");
	}
	for (const auto& Asset : P.Scene.GetAssets())
	{
		if (Asset.Id != InRequest.Object)
		{
			continue;
		}
		if (!Asset.Error.empty())
		{
			throw FSceneEditError("load_failed", Asset.Error);
		}
		if (!Asset.Data)
		{
			return {};
		}
		FSceneNode Node;
		Node.Name = "Added model";
		Node.Model() = FSceneModelComponent{Asset.Id, Asset.Data};
		Node.Local() = Translation(InRequest.Position);
		const auto Handle = P.Document.CommitCreate(std::move(Node), false);
		return DescribeSceneNode(P.Document, {P.Document.Id(), Handle});
	}
	throw FSceneEditError("not_found", "Query the current scene placement catalog");
}

void FSceneViewerPlugin::RemoveSelected()
{
	Impl->Document.CommitDelete();
}

void FSceneViewerPlugin::ToggleSelected()
{
	const auto Source = Impl->Scene.FindNode(Impl->Selected);
	if (!Source)
	{
		return;
	}
	auto Node = *Source;
	if (Node.Model())
	{
		Node.Model()->bVisible = !Node.Model()->bVisible;
	}
	else
	{
		Node.bEnabled = !Node.bEnabled;
	}
	Impl->Document.CommitEdits({{Impl->Selected, std::move(Node)}}, Impl->Scene.GetRevision());
}

void FSceneViewerPlugin::MoveSelected(float InOffset)
{
	FSceneNodeView View;
	if (Impl->Scene.GetNodeView(Impl->Selected, View))
	{
		auto Node = *View.Node;
		FSceneNodeView Parent;
		const auto ParentHandle = Impl->Scene.FindHandle(Node.Parent());
		const auto ParentWorld = Impl->Scene.GetNodeView(ParentHandle, Parent) ? Parent.World : Identity();
		Node.Local() = Multiply(Inverse(ParentWorld), Multiply(Translation({InOffset, 0, 0}), View.World));
		Impl->Document.CommitEdits({{Impl->Selected, std::move(Node)}}, Impl->Scene.GetRevision());
	}
}

void FSceneViewerPlugin::AdvanceCamera(float InDeltaSeconds)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bStopped || !P.bViewInitialized)
	{
		return;
	}
	try
	{
		P.CameraController.Advance(P.ViewCamera, InDeltaSeconds);
	}
	catch (const std::exception& Failure)
	{
		P.EditError = Failure.what();
	}
}

void FSceneViewerPlugin::Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bStopped || !P.bViewInitialized)
	{
		P.CameraController.SuspendInput(InEvents);
		return;
	}
	try
	{
		P.CameraController.Input(P.ViewCamera, InEvents, bInMouseCaptured, bInKeyboardCaptured);
	}
	catch (const std::exception& Failure)
	{
		P.EditError = Failure.what();
	}
	for (const auto& Event : InEvents)
	{
		try
		{
			if (Event.Type != EEventType::Key || !Event.bDown || bInKeyboardCaptured)
			{
				continue;
			}
			switch (Event.Key)
			{
				case EKey::Home:
					Fit();
					break;
				case EKey::Insert:
					DuplicateSelected();
					break;
				case EKey::Delete:
					RemoveSelected();
					break;
				case EKey::Space:
					ToggleSelected();
					break;
				case EKey::C:
					SetCullingMode(static_cast<ESceneCullingMode>((static_cast<int>(P.Mode) + 1) % 3));
					break;
				default:
					break;
			}
		}
		catch (const std::exception& Failure)
		{
			P.EditError = Failure.what();
		}
	}
}
} // namespace Hyperion
