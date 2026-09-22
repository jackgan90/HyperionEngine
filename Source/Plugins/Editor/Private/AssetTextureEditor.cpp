#include "AssetPropertyWidgets.h"
#include "AssetWorkspace.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Hyperion
{
namespace
{
float DisplayChannel(float InLinear)
{
	const float Value = std::clamp(InLinear, 0.f, 1.f);
	return Value <= .0031308f ? Value * 12.92f : 1.055f * std::pow(Value, 1.f / 2.4f) - .055f;
}

FTextureAsset DisplayTexture(const FTextureAsset& InSource, std::size_t InMip, std::size_t InFace,
                             std::size_t InChannel, float InExposure, bool bInChecker,
                             FCancellationToken InCancellation)
{
	const auto& Mip = InSource.Mips.at(InMip);
	FMaterialTextureMip Output{Mip.Width, Mip.Height};
	Output.Bytes.resize(std::size_t(Mip.Width) * Mip.Height * 4);
	for (std::uint32_t Y = 0; Y < Mip.Height; ++Y)
	{
		InCancellation.Check();
		for (std::uint32_t X = 0; X < Mip.Width; ++X)
		{
			const auto Pixel = std::size_t(Y) * Mip.Width + X;
			auto Value = ReadTexturePixel(Mip, InSource.Format, InFace * std::size_t(Mip.Width) * Mip.Height + Pixel);
			const float Background = bInChecker ? (((X / 16 + Y / 16) & 1) ? .32f : .18f) : 0.f;
			if (InChannel)
			{
				const float Component = std::clamp(Value.at(InChannel - 1), 0.f, 1.f);
				Value = {Component, Component, Component, 1};
			}
			else
			{
				for (std::size_t C = 0; C < 3; ++C)
				{
					if (InSource.Encoding == EMaterialTextureEncoding::Linear)
					{
						Value[C] = DisplayChannel(Value[C] * std::exp2(InExposure));
					}
					Value[C] = std::clamp(Value[C], 0.f, 1.f) * std::clamp(Value[3], 0.f, 1.f) +
					           Background * (1 - std::clamp(Value[3], 0.f, 1.f));
				}
				Value[3] = 1;
			}
			WriteTexturePixel(Output, ETextureFormat::Rgba8Unorm, Pixel, Value);
		}
	}
	return BuildTextureAsset("Texture preview", EMaterialTextureEncoding::Srgb, std::move(Output));
}
} // namespace

void FAssetWorkspace::DrawTexture(FGui& InGui, FEntry& InEntry, std::span<const FInputEvent> InEvents)
{
	if (!InEntry.Preview)
	{
		InGui.Text("Preparing texture...");
		return;
	}
	const auto Texture = InEntry.Preview->As<FTextureAsset>();
	DrawTextureControls(InGui, InEntry, *Texture);
	PollTextureDisplay(InEntry, Texture);
	DrawTextureCanvas(InGui, InEntry, *Texture, InEvents);
}

void FAssetWorkspace::DrawTextureControls(FGui& InGui, FEntry& InEntry, const FTextureAsset& InTexture)
{
	const auto* Texture = &InTexture;
	auto& View = InEntry.Texture;
	std::vector<std::string> Mips;
	for (std::size_t I = 0; I < Texture->Mips.size(); ++I)
	{
		Mips.push_back(std::to_string(I) + " (" + std::to_string(Texture->Mips[I].Width) + " x " +
		               std::to_string(Texture->Mips[I].Height) + ")");
	}
	View.Mip = std::min(View.Mip, Mips.size() - 1);
	bool bChanged = AssetCombo(InGui, "Mip", Mips, View.Mip);
	const std::array<std::string, 5> Channels{"RGBA", "R", "G", "B", "A"};
	bChanged |= AssetCombo(InGui, "Channel", Channels, View.Channel);
	if (Texture->Dimension == ETextureDimension::Cube)
	{
		const std::array<std::string, 6> Faces{"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
		bChanged |= AssetCombo(InGui, "Face", Faces, View.Face);
	}
	else
	{
		View.Face = 0;
	}
	bChanged |= AssetCheckbox(InGui, "Checkerboard", View.bChecker);
	InGui.SameLine();
	if (InGui.Button("Fit"))
	{
		View.bFit = true;
		View.Pan = {};
	}
	InGui.SameLine();
	if (InGui.Button("1:1"))
	{
		View.bFit = false;
		View.Zoom = 1;
		View.Pan = {};
	}
	if (Texture->Encoding == EMaterialTextureEncoding::Linear)
	{
		bChanged |= InGui.Slider("Exposure EV (preview)", View.Exposure, -12, 12);
	}
	if (bChanged)
	{
		++View.Revision;
	}
}

void FAssetWorkspace::PollTextureDisplay(FEntry& InEntry, const std::shared_ptr<const FTextureAsset>& InTexture)
{
	const auto Texture = InTexture;
	auto& View = InEntry.Texture;
	try
	{
		if (View.Pending && View.Pending->Ready())
		{
			const auto Display = View.Pending->GetReady();
			View.Pending.reset();
			if (View.PreparedRevision == View.Revision)
			{
				View.PendingTarget = {ERenderTargetKind::Texture,
				                      std::make_shared<const FMaterialTextureSource>(Display),
				                      Session.GetResources().CreateScopeLifetime()};
			}
		}
		if (View.Upload && View.Upload->Ready())
		{
			const bool bReady = *View.Upload->GetReady();
			View.Upload.reset();
			if (bReady)
			{
				if (View.PreparedRevision == View.Revision)
				{
					View.Target = View.PendingTarget;
					View.TargetRevision = View.PreparedRevision;
				}
				View.PendingTarget = {};
			}
		}
		if (View.PendingTarget.Texture && !View.Upload)
		{
			View.Upload =
			    DispatchAsync<bool>(Tasks, {EDomain::Rhi, 0},
			                        [Endpoint = Session.GetResources().GetPreparation(), Source = View.PendingTarget]
			                        {
				                        const std::array Sources{Source.Texture};
				                        return Endpoint.PrepareTextures(Sources, Source.Lifetime);
			                        });
		}
		if (!View.Pending && !View.PendingTarget.Texture && !View.Upload && View.PreparedRevision != View.Revision)
		{
			View.PreparedRevision = View.Revision;
			View.Pending = DispatchAsync<FTextureAsset>(
			    Tasks, {EDomain::Worker},
			    [Texture, Mip = View.Mip, Face = View.Face, Channel = View.Channel, Exposure = View.Exposure,
			     bChecker = View.bChecker, Token = InEntry.Cancellation]
			    {
				    return DisplayTexture(*Texture, Mip, Face, Channel, Exposure, bChecker, Token);
			    },
			    InEntry.Cancellation);
		}
	}
	catch (const std::exception& Failure)
	{
		View.Pending.reset();
		InEntry.Error = Failure.what();
	}
}

void FAssetWorkspace::DrawTextureCanvas(FGui& InGui, FEntry& InEntry, const FTextureAsset& InTexture,
                                        std::span<const FInputEvent> InEvents)
{
	const auto* Texture = &InTexture;
	auto& View = InEntry.Texture;
	const auto& Mip = Texture->Mips[View.Mip];
	InEntry.Region = InGui.Image(0);
	Bounds["canvas"] = InEntry.Region.Bounds;
	const auto B = InEntry.Region.Bounds;
	if (View.bFit)
	{
		View.Zoom = std::min((B.Z - B.X) / Mip.Width, (B.W - B.Y) / Mip.Height);
	}
	for (const auto& Event : InEvents)
	{
		if (InEntry.Region.bHovered && Event.Type == EEventType::MouseWheel)
		{
			View.bFit = false;
			View.Zoom = std::clamp(View.Zoom * std::pow(1.2f, Event.Y), .01f, 64.f);
		}
	}
	const auto Pointer = InGui.PointerState();
	if (Pointer.bPressed && InEntry.Region.bHovered)
	{
		View.bDragging = true;
		View.LastPointer = Pointer.Position;
		InGui.CaptureImagePointer(true);
	}
	if (View.bDragging)
	{
		View.Pan.X += Pointer.Position.X - View.LastPointer.X;
		View.Pan.Y += Pointer.Position.Y - View.LastPointer.Y;
		View.LastPointer = Pointer.Position;
		if (!Pointer.bDown || Pointer.bCancel)
		{
			View.bDragging = false;
			InGui.CaptureImagePointer(false);
		}
	}
	const float X = (B.X + B.Z - Mip.Width * View.Zoom) * .5f + View.Pan.X;
	const float Y = (B.Y + B.W - Mip.Height * View.Zoom) * .5f + View.Pan.Y;
	if (View.Target.Texture && View.TargetRevision == View.Revision)
	{
		InGui.DrawImageOverlay(InEntry.TextureId, B, {X, Y, X + Mip.Width * View.Zoom, Y + Mip.Height * View.Zoom});
	}
	const int Px = static_cast<int>(std::floor((Pointer.Position.X - X) / View.Zoom));
	const int Py = static_cast<int>(std::floor((Pointer.Position.Y - Y) / View.Zoom));
	if (InEntry.Region.bHovered && Px >= 0 && Py >= 0 && Px < static_cast<int>(Mip.Width) &&
	    Py < static_cast<int>(Mip.Height))
	{
		const auto Pixel = ReadTexturePixel(
		    Mip, Texture->Format, View.Face * std::size_t(Mip.Width) * Mip.Height + std::size_t(Py) * Mip.Width + Px);
		std::ostringstream Text;
		Text << Px << ", " << Py << " | RGBA: " << Pixel[0] << ", " << Pixel[1] << ", " << Pixel[2] << ", " << Pixel[3];
		InGui.Tooltip(Text.str().c_str());
	}
}

void FAssetWorkspace::DrawTextureProperties(FGui& InGui, FEntry& InEntry)
{
	const auto Texture =
	    InEntry.Preview ? InEntry.Preview->As<FTextureAsset>() : InEntry.Document->Loaded().As<FTextureAsset>();
	AssetInfo(InGui, "Dimension", Texture->Dimension == ETextureDimension::Cube ? "Cube (6 faces)" : "2D");
	AssetInfo(InGui, "Size", std::to_string(Texture->Mips[0].Width) + " x " + std::to_string(Texture->Mips[0].Height));
	AssetInfo(InGui, "Mip count", std::to_string(Texture->Mips.size()));
	constexpr std::array Formats{"RGBA8 UNORM", "RGBA16 FLOAT", "RGBA32 FLOAT"};
	AssetInfo(InGui, "Format", Formats.at(static_cast<std::size_t>(Texture->Format)));
	std::size_t Bytes{};
	for (const auto& Mip : Texture->Mips)
	{
		Bytes += Mip.Bytes.size();
	}
	AssetInfo(InGui, "Pixel payload bytes", std::to_string(Bytes));
	std::size_t Encoding =
	    static_cast<std::size_t>(ReadValue<EMaterialTextureEncoding>(InEntry.Document->Get("encoding")));
	const std::array<std::string, 2> Encodings{"Linear", "sRGB"};
	const bool bEditable =
	    Texture->Dimension == ETextureDimension::Texture2D && Texture->Format == ETextureFormat::Rgba8Unorm;
	InGui.BeginDisabled(!bEditable || InEntry.Pending.has_value());
	if (AssetCombo(InGui, "Encoding", Encodings, Encoding,
	               [&](std::size_t InIndex, FVec4)
	               {
		               ObserveProperty(InGui, "encoding/" + Encodings[InIndex]);
	               }))
	{
		try
		{
			InEntry.EncodingGeneration = InEntry.Document->Generation();
			InEntry.EncodingEdit = DispatchAsync<FArchiveNode>(
			    Tasks, {EDomain::Worker},
			    [Texture, Encoding, Name = ReadValue<std::string>(InEntry.Document->Get("name"))]
			    {
				    auto Draft = WriteValue(BuildTextureAsset(Name, static_cast<EMaterialTextureEncoding>(Encoding),
				                                              Texture->Mips.front()));
				    ShareAssetBulk(Draft);
				    return Draft;
			    },
			    InEntry.Cancellation);
		}
		catch (const std::exception& Failure)
		{
			InEntry.Document->Error = Failure.what();
		}
	}
	ObserveProperty(InGui, "encoding");
	InGui.EndDisabled();
	if (!bEditable)
	{
		InGui.TextWrapped("Encoding is fixed for floating-point textures and baked cube maps.");
	}
}
} // namespace Hyperion
