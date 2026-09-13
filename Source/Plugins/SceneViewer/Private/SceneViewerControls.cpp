#include "Hyperion/Renderer/SceneNavigation.h"
#include "SceneViewerInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
void FSceneViewerPlugin::FImpl::UpdateCamera(FRenderFrame& InFrame)
{
	FSceneViewRequest Request;
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
	FitSceneCamera(P.Scene, float(P.LastView.Width) / std::max(1u, P.LastView.Height));
}

void FSceneViewerPlugin::DuplicateSelected()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (const auto Copy = P.Scene.DuplicateNode(P.Selected); Copy.Scene)
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
			FSceneModel Model{"Added model", Asset.Data};
			Model.World = Translation(GetSceneNavigationPivot(P.Scene));
			P.Selected = P.Scene.Add(std::move(Model), Asset.Id);
			return;
		}
	}
}

void FSceneViewerPlugin::RemoveSelected()
{
	Impl->Scene.RemoveSubtree(Impl->Selected);
	const auto Models = Impl->Scene.GetNodes(ESceneNodeKind::Model);
	Impl->Selected = Models.empty() ? FSceneHandle{} : Models.front();
}

void FSceneViewerPlugin::ToggleSelected()
{
	const auto Node = Impl->Scene.FindNode(Impl->Selected);
	if (!Node)
	{
		return;
	}
	if (Node->Model)
	{
		Impl->Scene.SetModelVisible(Impl->Selected, !Node->Model->bVisible);
	}
	else
	{
		Impl->Scene.SetEnabled(Impl->Selected, !Node->bEnabled);
	}
}

void FSceneViewerPlugin::MoveSelected(float InOffset)
{
	FSceneNodeView View;
	if (Impl->Scene.GetNodeView(Impl->Selected, View))
	{
		Impl->Scene.SetWorldTransform(Impl->Selected, Multiply(Translation({InOffset, 0, 0}), View.World));
	}
}

void FSceneViewerPlugin::AdvanceCamera(float InDeltaSeconds)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.bStopped)
	{
		return;
	}
	try
	{
		P.CameraController.Advance(P.Scene, InDeltaSeconds);
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
	if (P.bStopped)
	{
		return;
	}
	try
	{
		P.CameraController.Input(P.Scene, InEvents, bInMouseCaptured, bInKeyboardCaptured);
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
