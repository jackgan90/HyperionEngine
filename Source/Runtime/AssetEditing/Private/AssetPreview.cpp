#include "Hyperion/AssetEditing/AssetPreview.h"

namespace Hyperion
{
template<> const FRecordDescriptor& RecordType<FAssetPreviewSettings>()
{
	static const auto Type = MakeRecord<FAssetPreviewSettings>(
	    "hyperion.asset.preview.settings",
	    {Member("camera", &FAssetPreviewSettings::Camera), Member("shape", &FAssetPreviewSettings::Shape),
	     Member("exposure", &FAssetPreviewSettings::Exposure), Member("yaw", &FAssetPreviewSettings::Yaw),
	     Member("mip", &FAssetPreviewSettings::Mip), Member("face", &FAssetPreviewSettings::Face),
	     Member("channel", &FAssetPreviewSettings::Channel), Member("exposureEv", &FAssetPreviewSettings::ExposureEv),
	     Member("zoom", &FAssetPreviewSettings::Zoom), Member("pan", &FAssetPreviewSettings::Pan),
	     Member("fit", &FAssetPreviewSettings::Fit), Member("checker", &FAssetPreviewSettings::Checker)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FAssetPreviewState>()
{
	static const auto Type = MakeRecord<FAssetPreviewState>(
	    "hyperion.asset.preview.state",
	    {Member("document", &FAssetPreviewState::Document), Member("generation", &FAssetPreviewState::Generation),
	     Member("settings", &FAssetPreviewState::Settings), Member("ready", &FAssetPreviewState::bReady),
	     Member("error", &FAssetPreviewState::Error)});
	return Type;
}
} // namespace Hyperion
