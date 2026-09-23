#include "AssetOperations.h"
#include "Hyperion/AssetEditing/AssetPreview.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
namespace
{
struct FPreviewEdit
{
	std::string Document;
	std::uint64_t Generation{};
	FAssetPreviewSettings Settings;
	bool bFrame{};
};
} // namespace

template<> const FRecordDescriptor& RecordType<FPreviewEdit>()
{
	static const auto Type =
	    MakeRecord<FPreviewEdit>("automation.asset.preview.edit",
	                             {Member("document", &FPreviewEdit::Document, {.bRequired = true}),
	                              Member("generation", &FPreviewEdit::Generation, {.bRequired = true}),
	                              Member("settings", &FPreviewEdit::Settings), Member("frame", &FPreviewEdit::bFrame)});
	return Type;
}

void RegisterAssetPreviews(FOperationCatalog& InCatalog, IAssetPreviewWorkspace* InWorkspace)
{
	FOperationInfo Info;
	Info.Id = "asset.preview.get";
	Info.Owner = "automation-assets";
	Info.Summary = "Read the live asset tab preview state";
	Info.Description = "Null settings are unsupported by this asset type. Camera is a temporary 3D browsing camera. "
	                   "Shape: 0 sphere, 1 plane, 2 cube. Texture channel: 0 RGBA, 1 R, 2 G, 3 B, 4 A. Cube face: "
	                   "+X,-X,+Y,-Y,+Z,-Z. Preview readiness is separate from document readiness.";
	Info.bReadOnly = true;
	Info.Effects = "Reads temporary preview state.";
	Info.Completion = "Current Main snapshot.";
	const FAssetDocumentRequest QueryExample{"document-from-open"};
	Info.Example = WriteRecordWire(RecordType<FAssetDocumentRequest>(), &QueryExample);
	Info.Unavailable = InWorkspace ? "" : "This target has no asset preview workspace.";
	InCatalog.Register(MakeOperation<FAssetDocumentRequest, FAssetPreviewState>(
	    Info,
	    [InWorkspace](const auto& InRequest)
	    {
		    try
		    {
			    return InWorkspace->PreviewState(InRequest.Document);
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
	Info.Id = "asset.preview.set";
	Info.Summary = "Edit the live asset tab preview controls";
	Info.bReadOnly = false;
	Info.Description +=
	    " Patch only supported members. frame fits the view. Exposure is 0.05-8; yaw degrees -180 to 180; texture "
	    "exposureEv -12 to 12, zoom 0.01-64, pan in canvas pixels. Turn fit off to set manual zoom.";
	Info.Effects = "Changes temporary tab presentation without document history, generation or save.";
	Info.Completion = "Controls applied; query ready after resource preparation/rendering.";
	const FPreviewEdit Example{"document-from-open", 1};
	Info.Example = WriteRecordWire(RecordType<FPreviewEdit>(), &Example);
	InCatalog.Register(MakeOperation<FPreviewEdit, FAssetPreviewState>(
	    Info,
	    [InWorkspace](const auto& InRequest)
	    {
		    try
		    {
			    return InWorkspace->EditPreview(InRequest.Document, InRequest.Generation, InRequest.Settings,
			                                    InRequest.bFrame);
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}
} // namespace Hyperion
