#include "D3D12PassAttachments.h"
#include "D3D12GraphicsState.h"
#include "Hyperion/Math/Bounds.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>
#include <cmath>
#include <set>

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
		const auto* Texture = Barrier.Target.Kind == ERenderTargetKind::Texture
		                          ? &NativeResource<FD3D12Texture>(Barrier.Target.Texture.Payload, &InState)
		                          : nullptr;
		if (Texture && !Texture->DepthViews && !Texture->ColorViews)
		{
			throw std::invalid_argument("Only sampled render target transitions are supported");
		}
		const auto Valid = [&](EResourceState InValue)
		{
			if (Barrier.Target.Kind == ERenderTargetKind::Backbuffer)
			{
				return InValue == EResourceState::Present || InValue == EResourceState::RenderTarget;
			}
			return (Texture && Texture->ColorViews ? InValue == EResourceState::RenderTarget
			                                       : InValue == EResourceState::DepthWrite) ||
			       (Texture && InValue == EResourceState::ShaderRead);
		};
		if (!Valid(Barrier.Before) || !Valid(Barrier.After))
		{
			throw std::invalid_argument("Invalid attachment resource transition");
		}
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE ColorView(const FD3D12DeviceState& InState, const FColorAttachment& InAttachment,
                                      const FD3D12FrameTargets& InFrame)
{
	return InAttachment.Target.Kind == ERenderTargetKind::Backbuffer
	           ? (InAttachment.GetFormat() == ERHIColorFormat::Rgba8Srgb ? InFrame.SrgbView : InFrame.ColorView)
	           : NativeResource<FD3D12Texture>(InAttachment.Target.Texture.Payload, &InState)
	                 .ColorViews->GetCPUDescriptorHandleForHeapStart();
}

FSize ValidateColor(const FD3D12DeviceState& InState, const FColorAttachment& InAttachment,
                    const FD3D12FrameTargets& InFrame)
{
	if (!IsFinite(FVec3{InAttachment.Clear.X, InAttachment.Clear.Y, InAttachment.Clear.Z}) ||
	    !std::isfinite(InAttachment.Clear.W) || InAttachment.Format >= ERHIColorFormat::Count ||
	    (InAttachment.bSrgb && InAttachment.Format != ERHIColorFormat::Rgba8Unorm))
	{
		throw std::invalid_argument("Invalid color clear or view format");
	}
	ValidateActions(InAttachment.Actions);
	ResolveTarget(InAttachment.Target, InState, InFrame);
	if (InAttachment.Target.Kind == ERenderTargetKind::Backbuffer)
	{
		if (InAttachment.GetFormat() != ERHIColorFormat::Rgba8Unorm &&
		    InAttachment.GetFormat() != ERHIColorFormat::Rgba8Srgb)
		{
			throw std::invalid_argument("Backbuffer requires RGBA8 color format");
		}
		return InFrame.Size;
	}
	if (InAttachment.Target.Kind != ERenderTargetKind::Texture)
	{
		throw std::invalid_argument("Invalid color attachment kind");
	}
	const auto& Texture = NativeResource<FD3D12Texture>(InAttachment.Target.Texture.Payload, &InState);
	if (!Texture.ColorViews || Texture.ColorFormat != InAttachment.GetFormat())
	{
		throw std::invalid_argument("Color texture format or render view mismatch");
	}
	const auto Info = Texture.GetInfo();
	return {Info.Width, Info.Height};
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
	for (const auto& Color : InCommands.GetColors())
	{
		if (IsDiscard(Color.Actions))
		{
			Discard(InList, ResolveTarget(Color.Target, InState, InFrame), Rect, 0);
		}
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
	const auto Colors = InCommands.GetColors();
	if (Colors.size() > InState.Capabilities.MaxColorTargets)
	{
		throw std::invalid_argument("Pass exceeds backend MRT capacity");
	}
	std::set<ID3D12Resource*> Written;
	bool bHaveSize = false;
	const auto CheckSize = [&](FSize InSize)
	{
		if (bHaveSize && (Size.Width != InSize.Width || Size.Height != InSize.Height))
		{
			throw std::invalid_argument("Pass attachment dimensions differ");
		}
		Size = InSize;
		bHaveSize = true;
	};
	for (const auto& Color : Colors)
	{
		CheckSize(ValidateColor(InState, Color, InFrame));
		if (!Written.insert(ResolveTarget(Color.Target, InState, InFrame)).second)
		{
			throw std::invalid_argument("Duplicate color attachment resource");
		}
	}
	if (InCommands.DepthStencil)
	{
		CheckSize(ValidateDepth(InState, *InCommands.DepthStencil, InFrame));
		if (!Written.insert(ResolveTarget(InCommands.DepthStencil->Target, InState, InFrame)).second)
		{
			throw std::invalid_argument("Aliased color/depth attachment");
		}
	}
	if (Colors.empty() && !InCommands.DepthStencil && !InCommands.GetDraws().empty())
	{
		throw std::invalid_argument("Draws require an explicit graphics attachment");
	}
	for (const auto& Texture : InCommands.SampledTextures)
	{
		const auto& NativeTexture = NativeResource<FD3D12Texture>(Texture.Payload, &InState);
		if ((!NativeTexture.DepthViews && !NativeTexture.ColorViews) || Written.contains(NativeTexture.Resource.Get()))
		{
			throw std::invalid_argument("Invalid or simultaneously writable sampled target");
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
	const auto Colors = InCommands.GetColors();
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, MaximumColorTargets> Rtvs{};
	for (std::size_t Index = 0; Index < Colors.size(); ++Index)
	{
		Rtvs[Index] = ColorView(InState, Colors[Index], InFrame);
	}
	const auto Dsv =
	    InCommands.DepthStencil ? DepthView(InState, *InCommands.DepthStencil, InFrame) : D3D12_CPU_DESCRIPTOR_HANDLE{};
	InList.OMSetRenderTargets(static_cast<UINT>(Colors.size()), Colors.empty() ? nullptr : Rtvs.data(), FALSE,
	                          InCommands.DepthStencil ? &Dsv : nullptr);
	const auto Rect = RenderRect(InCommands, InSize);
	for (std::size_t Index = 0; Index < Colors.size(); ++Index)
	{
		if (Colors[Index].Actions.Load == EAttachmentLoad::Clear)
		{
			const auto& Value = Colors[Index].Clear;
			const float Color[]{Value.X, Value.Y, Value.Z, Value.W};
			InList.ClearRenderTargetView(Rtvs[Index], Color, 1, &Rect);
		}
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
