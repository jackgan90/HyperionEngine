#include "D3D12PassAttachments.h"
#include "D3D12GraphicsState.h"
#include "Hyperion/Math/Bounds.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
ID3D12Resource* ResolveTarget(const FRenderTarget& InTarget, const FD3D12DeviceState& InState,
                              const FD3D12FrameTargets& InFrame)
{
	if (InTarget.Kind == ERenderTargetKind::Texture)
	{
		return NativeResource<FD3D12Texture>(InTarget.Texture.Payload, &InState).Resource.Get();
	}
	if (InTarget.Texture)
	{
		throw std::invalid_argument("Frame attachment cannot carry a texture payload");
	}
	if (InTarget.Kind == ERenderTargetKind::Backbuffer)
	{
		return InFrame.Backbuffer;
	}
	if (InTarget.Kind == ERenderTargetKind::FrameDepth)
	{
		return InFrame.Depth;
	}
	throw std::invalid_argument("Missing explicit attachment target");
}

void ValidateActions(const FAttachmentActions& InActions)
{
	if ((InActions.Load != EAttachmentLoad::Load && InActions.Load != EAttachmentLoad::Clear &&
	     InActions.Load != EAttachmentLoad::Discard) ||
	    (InActions.Store != EAttachmentStore::Store && InActions.Store != EAttachmentStore::Discard))
	{
		throw std::invalid_argument("Invalid attachment load/store action");
	}
}

FSize ValidateDepth(const FD3D12DeviceState& InState, const FDepthStencilAttachment& InAttachment,
                    const FD3D12FrameTargets& InFrame)
{
	auto Size = InFrame.Size;
	auto Format = InFrame.DepthFormat;
	if (InAttachment.Target.Kind == ERenderTargetKind::Texture)
	{
		const auto& Texture = NativeResource<FD3D12Texture>(InAttachment.Target.Texture.Payload, &InState);
		if (!Texture.DepthViews)
		{
			throw std::invalid_argument("Attachment texture has no depth view");
		}
		Size = Texture.DepthSize;
		Format = ERHIDepthFormat::D32;
	}
	else if (InAttachment.Target.Kind != ERenderTargetKind::FrameDepth)
	{
		throw std::invalid_argument("Invalid depth attachment target");
	}
	if ((!InAttachment.Depth && !InAttachment.Stencil) || InAttachment.Format != Format ||
	    Format == ERHIDepthFormat::None || (InAttachment.Stencil && Format != ERHIDepthFormat::D32S8) ||
	    !std::isfinite(InAttachment.ClearDepth) || InAttachment.ClearDepth < 0 || InAttachment.ClearDepth > 1)
	{
		throw std::invalid_argument("Invalid depth/stencil attachment format, aspects or clear value");
	}
	if (InAttachment.Depth)
	{
		ValidateActions(*InAttachment.Depth);
	}
	if (InAttachment.Stencil)
	{
		ValidateActions(*InAttachment.Stencil);
	}
	ResolveTarget(InAttachment.Target, InState, InFrame);
	return Size;
}

void ValidateTransitions(const FD3D12DeviceState& InState, const FPassCommands& InCommands,
                         const FD3D12FrameTargets& InFrame)
{
	for (const auto& Barrier : InCommands.Transitions)
	{
		ResolveTarget(Barrier.Target, InState, InFrame);
		const auto Valid = [&](EResourceState InValue)
		{
			if (Barrier.Target.Kind == ERenderTargetKind::Backbuffer)
			{
				return InValue == EResourceState::Present || InValue == EResourceState::RenderTarget;
			}
			return InValue == EResourceState::DepthWrite ||
			       (Barrier.Target.Kind == ERenderTargetKind::Texture && InValue == EResourceState::ShaderRead);
		};
		if (!Valid(Barrier.Before) || !Valid(Barrier.After))
		{
			throw std::invalid_argument("Invalid attachment resource transition");
		}
		if (Barrier.Target.Kind == ERenderTargetKind::Texture &&
		    !NativeResource<FD3D12Texture>(Barrier.Target.Texture.Payload, &InState).DepthViews)
		{
			throw std::invalid_argument("Only sampled depth texture transitions are supported");
		}
	}
}

D3D12_RECT RenderRect(const FPassCommands& InCommands, FSize InSize)
{
	const auto View = InCommands.Viewport.value_or(FViewport{0, 0, float(InSize.Width), float(InSize.Height)});
	return {static_cast<LONG>(std::floor(View.X)), static_cast<LONG>(std::floor(View.Y)),
	        static_cast<LONG>(std::ceil(View.X + View.Width)), static_cast<LONG>(std::ceil(View.Y + View.Height))};
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthView(const FD3D12DeviceState& InState, const FDepthStencilAttachment& InAttachment,
                                      const FD3D12FrameTargets& InFrame)
{
	return InAttachment.Target.Kind == ERenderTargetKind::FrameDepth
	           ? InFrame.DepthView
	           : NativeResource<FD3D12Texture>(InAttachment.Target.Texture.Payload, &InState)
	                 .DepthViews->GetCPUDescriptorHandleForHeapStart();
}

void Discard(ID3D12GraphicsCommandList& InList, ID3D12Resource* InResource, D3D12_RECT InRect, UINT InPlane)
{
	D3D12_DISCARD_REGION Region{1, &InRect, InPlane, 1};
	InList.DiscardResource(InResource, &Region);
}

void DiscardAttachments(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState,
                        const FPassCommands& InCommands, const FD3D12FrameTargets& InFrame, FSize InSize, bool bInStore)
{
	const auto IsDiscard = [bInStore](const FAttachmentActions& InActions)
	{
		return bInStore ? InActions.Store == EAttachmentStore::Discard : InActions.Load == EAttachmentLoad::Discard;
	};
	const auto Rect = RenderRect(InCommands, InSize);
	if (InCommands.Color && IsDiscard(InCommands.Color->Actions))
	{
		Discard(InList, ResolveTarget(InCommands.Color->Target, InState, InFrame), Rect, 0);
	}
	if (InCommands.DepthStencil)
	{
		const auto& Attachment = *InCommands.DepthStencil;
		auto Resource = ResolveTarget(Attachment.Target, InState, InFrame);
		if (Attachment.Depth && IsDiscard(*Attachment.Depth))
		{
			Discard(InList, Resource, Rect, 0);
		}
		if (Attachment.Stencil && IsDiscard(*Attachment.Stencil))
		{
			Discard(InList, Resource, Rect, 1);
		}
	}
}
} // namespace

FSize ValidatePassAttachments(const FD3D12DeviceState& InState, const FPassCommands& InCommands,
                              const FD3D12FrameTargets& InFrame)
{
	auto Size = InFrame.Size;
	if (InCommands.Color)
	{
		if (InCommands.Color->Target.Kind != ERenderTargetKind::Backbuffer ||
		    (!IsFinite(FVec3{InCommands.Color->Clear.X, InCommands.Color->Clear.Y, InCommands.Color->Clear.Z}) ||
		     !std::isfinite(InCommands.Color->Clear.W)))
		{
			throw std::invalid_argument("Unsupported color target or clear value");
		}
		ResolveTarget(InCommands.Color->Target, InState, InFrame);
		ValidateActions(InCommands.Color->Actions);
	}
	if (InCommands.DepthStencil)
	{
		Size = ValidateDepth(InState, *InCommands.DepthStencil, InFrame);
		if (InCommands.Color && (Size.Width != InFrame.Size.Width || Size.Height != InFrame.Size.Height))
		{
			throw std::invalid_argument("Color/depth attachment dimensions differ");
		}
	}
	if (!InCommands.Color && !InCommands.DepthStencil && !InCommands.GetDraws().empty())
	{
		throw std::invalid_argument("Draws require an explicit graphics attachment");
	}
	for (const auto& Texture : InCommands.SampledTextures)
	{
		if (!NativeResource<FD3D12Texture>(Texture.Payload, &InState).DepthViews ||
		    Texture == InCommands.GetDepthTexture())
		{
			throw std::invalid_argument("Invalid sampled depth resource");
		}
	}
	ValidateTransitions(InState, InCommands, InFrame);
	if (InCommands.Viewport)
	{
		ValidateViewport(*InCommands.Viewport, Size);
	}
	return Size;
}

void RecordPassBegin(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState,
                     const FPassCommands& InCommands, const FD3D12FrameTargets& InFrame, FSize InSize)
{
	for (const auto& Barrier : InCommands.Transitions)
	{
		Transition(&InList, ResolveTarget(Barrier.Target, InState, InFrame), Native(Barrier.Before),
		           Native(Barrier.After));
	}
	DiscardAttachments(InList, InState, InCommands, InFrame, InSize, false);
	const auto Rtv = InCommands.IsSrgb() ? InFrame.SrgbView : InFrame.ColorView;
	const auto Dsv =
	    InCommands.DepthStencil ? DepthView(InState, *InCommands.DepthStencil, InFrame) : D3D12_CPU_DESCRIPTOR_HANDLE{};
	InList.OMSetRenderTargets(InCommands.Color ? 1 : 0, InCommands.Color ? &Rtv : nullptr, FALSE,
	                          InCommands.DepthStencil ? &Dsv : nullptr);
	const auto Rect = RenderRect(InCommands, InSize);
	if (InCommands.Color && InCommands.Color->Actions.Load == EAttachmentLoad::Clear)
	{
		const auto& Value = InCommands.Color->Clear;
		const float Color[]{Value.X, Value.Y, Value.Z, Value.W};
		InList.ClearRenderTargetView(Rtv, Color, 1, &Rect);
	}
	if (InCommands.DepthStencil)
	{
		const auto& Depth = *InCommands.DepthStencil;
		const bool bClearDepth = Depth.Depth && Depth.Depth->Load == EAttachmentLoad::Clear;
		const bool bClearStencil = Depth.Stencil && Depth.Stencil->Load == EAttachmentLoad::Clear;
		if (bClearDepth || bClearStencil)
		{
			const auto Flags = static_cast<D3D12_CLEAR_FLAGS>((bClearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0) |
			                                                  (bClearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0));
			InList.ClearDepthStencilView(Dsv, Flags, Depth.ClearDepth, Depth.ClearStencil, 1, &Rect);
		}
	}
	const auto View = InCommands.Viewport.value_or(FViewport{0, 0, float(InSize.Width), float(InSize.Height)});
	D3D12_VIEWPORT Viewport{View.X, View.Y, View.Width, View.Height, View.MinDepth, View.MaxDepth};
	InList.RSSetViewports(1, &Viewport);
}

void RecordPassEnd(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState, const FPassCommands& InCommands,
                   const FD3D12FrameTargets& InFrame, FSize InSize)
{
	DiscardAttachments(InList, InState, InCommands, InFrame, InSize, true);
}
} // namespace Hyperion
