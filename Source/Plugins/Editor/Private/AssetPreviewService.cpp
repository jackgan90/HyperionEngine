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
		Result.Channel = ToAssetPreviewChannelWireValue(InEntry.Texture.Channel);
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
			Result.Shape = ToAssetPreviewShapeWireValue(InEntry.Shape);
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
				throw FSceneEditError(SceneEditErrors::Busy, "Wait for the asset document to load");
			}
			const bool bReady = Entry->Preview && !Entry->Pending && Entry->Error.empty() &&
			                    Entry->PreparedGeneration == Entry->Document->PreviewGeneration() &&
			                    (Entry->Scene ? Entry->Scene->GetStatus().bReady
			                                  : Entry->Texture.Target.Texture &&
			                                        Entry->Texture.TargetRevision == Entry->Texture.Revision);
			return {Entry->DocumentId, Entry->Document->Generation(), GetPreviewSettings(*Entry), bReady, Entry->Error};
		}
	}
	throw FSceneEditError(SceneEditErrors::NotFound, "Unknown workspace document");
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
		throw FSceneEditError(SceneEditErrors::StaleRevision, "Asset changed before preview update");
	}
	if (IsBlocked() || !State.bReady)
	{
		throw FSceneEditError(SceneEditErrors::Busy, "Wait for preview readiness and finish modal operations");
	}
	for (const auto& Entry : Entries)
	{
		if (Entry->DocumentId == InDocument)
		{
			if (Entry->GuiInteraction != 0 || Entry->HasPendingEdit())
			{
				throw FSceneEditError(SceneEditErrors::Busy,
				                      "Finish the active asset interaction before changing its preview");
			}
			SetPreviewSettings(*Entry, InSettings);
			if (bInFrame)
			{
				FramePreview(*Entry);
			}
			return PreviewState(InDocument);
		}
	}
	throw FSceneEditError(SceneEditErrors::NotFound, "Unknown workspace document");
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
	const auto Shape = InSettings.Shape ? ParseAssetPreviewShape(*InSettings.Shape) : InEntry.Shape;
	if ((InSettings.Exposure &&
	     (!std::isfinite(*InSettings.Exposure) || *InSettings.Exposure < .05f || *InSettings.Exposure > 8)) ||
	    (InSettings.Yaw && (!std::isfinite(*InSettings.Yaw) || std::abs(*InSettings.Yaw) > 180)))
	{
		throw std::invalid_argument("Exposure must be 0.05-8, yaw -180 to 180 degrees");
	}
	if (Supported.Mip)
	{
		SetTexturePreview(InEntry, InSettings);
		return;
	}
	if (Shape != InEntry.Shape)
	{
		InEntry.Shape = Shape;
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
		Light.YawDegrees = *InSettings.Yaw;
		InEntry.Scene->SetEnvironmentLight(Handle, std::move(Light));
		InEntry.YawDegrees = *InSettings.Yaw;
	}
}

void FAssetWorkspace::SetTexturePreview(FEntry& InEntry, const FAssetPreviewSettings& InSettings)
{
	const auto Texture =
	    InEntry.Preview ? InEntry.Preview->As<FTextureAsset>() : InEntry.Document->Loaded().As<FTextureAsset>();
	const auto Channel = InSettings.Channel ? ParseAssetPreviewChannel(*InSettings.Channel) : InEntry.Texture.Channel;
	if ((InSettings.Mip && *InSettings.Mip >= Texture->Mips.size()) ||
	    (InSettings.Face && *InSettings.Face >= (Texture->Dimension == ETextureDimension::Cube ? 6u : 1u)) ||
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
	View.Channel = Channel;
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
