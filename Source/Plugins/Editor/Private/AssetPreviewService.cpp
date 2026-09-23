#include "AssetWorkspace.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/SceneEditing/SceneDocument.h"
#include <cmath>

namespace Hyperion
{
FAssetPreviewSettings FAssetWorkspace::GetPreviewSettings(const FEntry& InEntry) const
{
	FAssetPreviewSettings Result;
	const auto& Type = InEntry.Document->Loaded().Header.TypeId;
	if (Type == RecordType<FTextureAsset>().Id)
	{
		Result.Mip = static_cast<std::uint32_t>(InEntry.Texture.Mip);
		Result.Face = static_cast<std::uint32_t>(InEntry.Texture.Face);
		Result.Channel = static_cast<std::uint32_t>(InEntry.Texture.Channel);
		if (ReadValue<EMaterialTextureEncoding>(InEntry.Document->Get("encoding")) == EMaterialTextureEncoding::Linear)
		{
			Result.ExposureEv = InEntry.Texture.Exposure;
		}
		Result.Zoom = InEntry.Texture.Zoom;
		Result.Pan = InEntry.Texture.Pan;
		Result.Fit = InEntry.Texture.bFit;
		Result.Checker = InEntry.Texture.bChecker;
	}
	else
	{
		Result.Camera = InEntry.Camera;
		Result.Exposure = InEntry.Exposure;
		if (Type == RecordType<FMaterialAsset>().Id)
		{
			Result.Shape = static_cast<std::uint32_t>(InEntry.Shape);
		}
		if (Type == RecordType<FSkyAsset>().Id)
		{
			Result.Yaw = InEntry.YawDegrees;
		}
	}
	return Result;
}

FAssetPreviewState FAssetWorkspace::PreviewState(std::string_view InDocument) const
{
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InDocument)
		{
			if (!Entry->Document)
			{
				throw FSceneEditError("busy", "Wait for the asset document to load");
			}
			const bool bReady = Entry->Preview && !Entry->Pending && Entry->Error.empty() &&
			                    Entry->PreparedGeneration == Entry->Document->PreviewGeneration() &&
			                    (Entry->Scene ? Entry->Scene->GetStatus().bReady
			                                  : Entry->Texture.Target.Texture &&
			                                        Entry->Texture.TargetRevision == Entry->Texture.Revision);
			return {Entry->DocumentId, Entry->Document->Generation(), GetPreviewSettings(*Entry), bReady, Entry->Error};
		}
	}
	throw FSceneEditError("not_found", "Unknown workspace document");
}

void FAssetWorkspace::FramePreview(FEntry& InEntry)
{
	if (InEntry.Scene)
	{
		FitSceneCamera(InEntry.Camera, *InEntry.Scene, 1.5f, true);
	}
	else
	{
		InEntry.Texture.bFit = true;
		InEntry.Texture.Pan = {};
	}
}

FAssetPreviewState FAssetWorkspace::EditPreview(std::string_view InDocument, std::uint64_t InGeneration,
                                                const FAssetPreviewSettings& InSettings, bool bInFrame)
{
	const auto State = PreviewState(InDocument);
	if (State.Generation != InGeneration)
	{
		throw FSceneEditError("stale_revision", "Asset changed before preview update");
	}
	if (IsBlocked() || !State.bReady)
	{
		throw FSceneEditError("busy", "Wait for preview readiness and finish modal operations");
	}
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InDocument)
		{
			if (Entry->GuiInteraction != 0 || Entry->HasPendingEdit())
			{
				throw FSceneEditError("busy", "Finish the active asset interaction before changing its preview");
			}
			SetPreviewSettings(*Entry, InSettings);
			if (bInFrame)
			{
				FramePreview(*Entry);
			}
			return PreviewState(InDocument);
		}
	}
	throw FSceneEditError("not_found", "Unknown workspace document");
}

void FAssetWorkspace::SetPreviewSettings(FEntry& InEntry, const FAssetPreviewSettings& InSettings)
{
	const auto Supported = GetPreviewSettings(InEntry);
	const auto& Type = RecordType<FAssetPreviewSettings>();
	for (const auto& Member : Type.Members)
	{
		const auto Requested = Member.Write(&InSettings);
		if (!std::holds_alternative<std::monostate>(Requested.Value) &&
		    std::holds_alternative<std::monostate>(Member.Write(&Supported).Value))
		{
			throw std::invalid_argument("Preview setting is unsupported by this asset type: " + Member.Id);
		}
	}
	if (InSettings.Camera)
	{
		ValidateSceneCameraView(*InSettings.Camera);
	}
	if ((InSettings.Shape && *InSettings.Shape > 2) ||
	    (InSettings.Exposure &&
	     (!std::isfinite(*InSettings.Exposure) || *InSettings.Exposure < .05f || *InSettings.Exposure > 8)) ||
	    (InSettings.Yaw && (!std::isfinite(*InSettings.Yaw) || std::abs(*InSettings.Yaw) > 180)))
	{
		throw std::invalid_argument("Shape must be 0-2, exposure 0.05-8, yaw -180 to 180 degrees");
	}
	if (Supported.Mip)
	{
		SetTexturePreview(InEntry, InSettings);
		return;
	}
	if (InSettings.Shape && *InSettings.Shape != InEntry.Shape)
	{
		InEntry.Shape = *InSettings.Shape;
		InEntry.PreparedGeneration = InEntry.RequestedGeneration = 0;
		InEntry.bCameraInitialized = false;
		InEntry.PreviewModel.reset();
	}
	if (InSettings.Camera)
	{
		InEntry.Camera = *InSettings.Camera;
		InEntry.Navigation.Reset();
		InEntry.bCameraInitialized = true;
	}
	InEntry.Exposure = InSettings.Exposure.value_or(InEntry.Exposure);
	if (InSettings.Yaw && InEntry.Scene)
	{
		const auto Handle = InEntry.Scene->FindHandle("preview-environment");
		auto Light = *InEntry.Scene->FindNode(Handle)->EnvironmentLight();
		Light.YawRadians = *InSettings.Yaw / 57.2957795f;
		InEntry.Scene->SetEnvironmentLight(Handle, std::move(Light));
		InEntry.YawDegrees = *InSettings.Yaw;
	}
}

void FAssetWorkspace::SetTexturePreview(FEntry& InEntry, const FAssetPreviewSettings& InSettings)
{
	const auto Texture =
	    InEntry.Preview ? InEntry.Preview->As<FTextureAsset>() : InEntry.Document->Loaded().As<FTextureAsset>();
	if ((InSettings.Mip && *InSettings.Mip >= Texture->Mips.size()) ||
	    (InSettings.Face && *InSettings.Face >= (Texture->Dimension == ETextureDimension::Cube ? 6u : 1u)) ||
	    (InSettings.Channel && *InSettings.Channel > 4) ||
	    (InSettings.ExposureEv && (!std::isfinite(*InSettings.ExposureEv) || std::abs(*InSettings.ExposureEv) > 12)) ||
	    (InSettings.Zoom && (!std::isfinite(*InSettings.Zoom) || *InSettings.Zoom < .01f || *InSettings.Zoom > 64)) ||
	    (InSettings.Pan && (!std::isfinite(InSettings.Pan->X) || !std::isfinite(InSettings.Pan->Y))))
	{
		throw std::invalid_argument("Invalid texture preview subresource, channel, exposure EV, zoom or pan");
	}
	auto& View = InEntry.Texture;
	const auto Before = std::tuple(View.Mip, View.Face, View.Channel, View.Exposure, View.bChecker);
	View.Mip = InSettings.Mip.value_or(static_cast<std::uint32_t>(View.Mip));
	View.Face = InSettings.Face.value_or(static_cast<std::uint32_t>(View.Face));
	View.Channel = InSettings.Channel.value_or(static_cast<std::uint32_t>(View.Channel));
	View.Exposure = InSettings.ExposureEv.value_or(View.Exposure);
	View.Zoom = InSettings.Zoom.value_or(View.Zoom);
	View.Pan = InSettings.Pan.value_or(View.Pan);
	View.bFit = InSettings.Fit.value_or(View.bFit);
	View.bChecker = InSettings.Checker.value_or(View.bChecker);
	if (Before != std::tuple(View.Mip, View.Face, View.Channel, View.Exposure, View.bChecker))
	{
		++View.Revision;
	}
}
} // namespace Hyperion
