#include "SceneViewerInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
void FSceneViewerPlugin::FImpl::UpdateCamera(FRenderFrame& InFrame)
{
	const FVec3 Direction{std::sin(Yaw) * std::cos(Pitch), std::sin(Pitch), std::cos(Yaw) * std::cos(Pitch)};
	InFrame.View.Eye = Add(Target, ScaleVector(Direction, Distance));
	InFrame.View.Camera = FRenderCamera{ScaleVector(Direction, -1), {0, 1, 0}, 1, Manifest->Near, Manifest->Far};
	const float Aspect = float(std::max(1u, InFrame.Size.Width)) / std::max(1u, InFrame.Size.Height);
	InFrame.View.ViewProjection =
	    Multiply(Perspective(1, Aspect, Manifest->Near, Manifest->Far, InFrame.View.DepthConvention),
	             LookAt(InFrame.View.Eye, Target));
	InFrame.View.CullingMode = Mode;
	InFrame.View.bInstanceBatching = bInstanceBatching;
	if (bFrozen)
	{
		InFrame.View.CullingViewProjection = FrozenView;
	}
	LastView = InFrame.View;
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
	FBounds Bounds;
	for (const auto Handle : P.Scene.GetHandles())
	{
		const auto Model = P.Scene.Find(Handle);
		if (Model->Data && Model->bVisible)
		{
			const auto WorldBounds = TransformBounds(Model->Data->Bounds, Model->World);
			if (IsUsable(WorldBounds))
			{
				Bounds = IsUsable(Bounds) ? UnionBounds(Bounds, WorldBounds) : WorldBounds;
			}
		}
	}
	if (IsUsable(Bounds))
	{
		P.Target = ScaleVector(Add(Bounds.Minimum, Bounds.Maximum), .5f);
		P.Radius = std::max(.01f, Length(Subtract(Bounds.Maximum, P.Target)));
		const float Aspect = float(P.LastView.Width) / std::max(1u, P.LastView.Height);
		P.Distance = P.Radius / std::sin(std::min(.5f, std::atan(std::tan(.5f) * Aspect))) * 1.12f;
	}
}

void FSceneViewerPlugin::DuplicateSelected()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Scene.GetModels().empty())
	{
		return;
	}
	const auto Source = P.Scene.GetModels()[P.Selected % P.Scene.GetModels().size()];
	auto Model = *P.Scene.Find(Source.Handle);
	Model.Name += " copy";
	Model.World = Multiply(Translation({2, 0, 0}), Model.World);
	P.Scene.Add(std::move(Model), Source.Asset);
	P.Selected = P.Scene.GetModels().size() - 1;
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
			Model.World = Translation(P.Target);
			P.Scene.Add(std::move(Model), Asset.Id);
			P.Selected = P.Scene.GetModels().size() - 1;
			return;
		}
	}
}

void FSceneViewerPlugin::RemoveSelected()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Scene.GetModels().empty())
	{
		return;
	}
	P.Selected %= P.Scene.GetModels().size();
	P.Scene.Remove(P.Scene.GetModels()[P.Selected].Handle);
	P.Selected = 0;
}

void FSceneViewerPlugin::ToggleSelected()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Scene.GetModels().empty())
	{
		return;
	}
	const auto Handle = P.Scene.GetModels()[P.Selected % P.Scene.GetModels().size()].Handle;
	auto Model = *P.Scene.Find(Handle);
	Model.bVisible = !Model.bVisible;
	P.Scene.Update(Handle, std::move(Model));
}

void FSceneViewerPlugin::MoveSelected(float InOffset)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	if (P.Scene.GetModels().empty())
	{
		return;
	}
	const auto Handle = P.Scene.GetModels()[P.Selected % P.Scene.GetModels().size()].Handle;
	auto Model = *P.Scene.Find(Handle);
	Model.World = Multiply(Translation({InOffset, 0, 0}), Model.World);
	P.Scene.Update(Handle, std::move(Model));
}

void FSceneViewerPlugin::Input(std::span<const FInputEvent> InEvents, bool bInMouseCaptured, bool bInKeyboardCaptured)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	for (const auto& Event : InEvents)
	{
		if (Event.Type == EEventType::Focus && !Event.bDown)
		{
			P.bDragging = false;
		}
		if (Event.Type == EEventType::MouseButton && Event.Button == 1)
		{
			P.bDragging = Event.bDown && !bInMouseCaptured;
		}
		if (Event.Type == EEventType::MouseMove)
		{
			if (P.bDragging)
			{
				P.Yaw -= (Event.X - P.LastMouse.X) * .006f;
				P.Pitch = std::clamp(P.Pitch + (Event.Y - P.LastMouse.Y) * .006f, -1.5f, 1.5f);
			}
			P.LastMouse = {Event.X, Event.Y};
		}
		if (Event.Type == EEventType::MouseWheel && !bInMouseCaptured)
		{
			P.Distance = std::clamp(P.Distance * std::pow(.85f, Event.Y), .02f, 100000.f);
		}
		if (Event.Type != EEventType::Key || !Event.bDown || bInKeyboardCaptured)
		{
			continue;
		}
		const FVec3 Right{std::cos(P.Yaw), 0, -std::sin(P.Yaw)};
		const FVec3 Forward{-std::sin(P.Yaw), 0, -std::cos(P.Yaw)};
		const float Step = std::max(.1f, P.Distance * .08f);
		switch (Event.Key)
		{
			case EKey::Left:
				P.Target = Add(P.Target, ScaleVector(Right, -Step));
				break;
			case EKey::Right:
				P.Target = Add(P.Target, ScaleVector(Right, Step));
				break;
			case EKey::Up:
				P.Target = Add(P.Target, ScaleVector(Forward, Step));
				break;
			case EKey::Down:
				P.Target = Add(P.Target, ScaleVector(Forward, -Step));
				break;
			case EKey::PageUp:
				P.Target.Y += Step;
				break;
			case EKey::PageDown:
				P.Target.Y -= Step;
				break;
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
}
} // namespace Hyperion
