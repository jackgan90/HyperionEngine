#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Hyperion/Renderer/RenderOutput.h"
#include "Hyperion/Renderer/ShadowControls.h"
#include "SceneOperations.h"

namespace Hyperion
{
namespace
{
struct FDiagnosticsRequest
{
	FSceneHandle Handle;
	std::string Component;
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FDiagnosticsPage
{
	std::vector<FScenePrimitiveDiagnostic> Primitives;
	std::uint64_t Scene{};
	std::uint64_t Epoch{};
	std::uint64_t Publication{};
	std::uint64_t Revision{};
	bool bApplied{};
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
};
} // namespace

template<> const FRecordDescriptor& RecordType<FDiagnosticsRequest>()
{
	static const auto Type = MakeRecord<FDiagnosticsRequest>(
	    "automation.render.component.query",
	    {Member("handle", &FDiagnosticsRequest::Handle, {.bRequired = true}),
	     Member("component", &FDiagnosticsRequest::Component, {.bRequired = true}),
	     Member("offset", &FDiagnosticsRequest::Offset), Member("limit", &FDiagnosticsRequest::Limit)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FDiagnosticsPage>()
{
	static const auto Type = MakeRecord<FDiagnosticsPage>(
	    "automation.render.component.page",
	    {Member("primitives", &FDiagnosticsPage::Primitives), Member("scene", &FDiagnosticsPage::Scene),
	     Member("epoch", &FDiagnosticsPage::Epoch), Member("publication", &FDiagnosticsPage::Publication),
	     Member("revision", &FDiagnosticsPage::Revision), Member("applied", &FDiagnosticsPage::bApplied),
	     Member("total", &FDiagnosticsPage::Total), Member("nextOffset", &FDiagnosticsPage::Next)});
	return Type;
}

void RegisterShadowControls(FOperationCatalog& InCatalog, IShadowControls* InShadows)
{
	FOperationInfo Info;
	Info.Id = "render.shadows.get";
	Info.Owner = "automation-scene";
	Info.Summary = "Read Viewer cascaded shadow controls";
	Info.Description = "Temporary render settings shared with the directional-shadow GUI. Scene light properties use "
	                   "scene component operations.";
	Info.bReadOnly = true;
	Info.Effects = "Reads render controls.";
	Info.Completion = "Current Main snapshot.";
	Info.Unavailable = InShadows ? "" : "Host has no shadow controls provider.";
	InCatalog.Register(MakeOperation<FSceneInfoRequest, FCascadedShadowSettings>(Info,
	                                                                             [InShadows](const auto&)
	                                                                             {
		                                                                             return InShadows->ShadowControls();
	                                                                             }));
	Info.Id = "render.shadows.set";
	Info.Summary = "Replace Viewer cascaded shadow controls";
	Info.Description += " Read first. Resolution 1024/2048, positive distance (GUI slider 5-200), splitLambda 0-1, "
	                    "normalOffset/receiverBias 0-2, blendFraction .01-.25, fadeFraction .01-.5, debugMode 0-5.";
	Info.bReadOnly = false;
	Info.Effects = "Changes subsequent frame shadow rendering; no scene or configuration write.";
	const FCascadedShadowSettings Example;
	Info.Example = WriteRecordWire(RecordType<FCascadedShadowSettings>(), &Example);
	InCatalog.Register(
	    MakeOperation<FCascadedShadowSettings, FCascadedShadowSettings>(Info,
	                                                                    [InShadows](const auto& InRequest)
	                                                                    {
		                                                                    InShadows->SetShadowControls(InRequest);
		                                                                    return InShadows->ShadowControls();
	                                                                    }));
}

void RegisterRenderDiagnostics(FOperationCatalog& InCatalog, IRenderDiagnostics* InDiagnostics)
{
	FOperationInfo Info;
	Info.Id = "render.statistics";
	Info.Owner = "automation-scene";
	Info.Summary = "Read completed rendering and device statistics";
	Info.Description =
	    "Uses the same completed pipeline snapshots as host diagnostics: views, culling, batches, local lighting, "
	    "depth, outlines, allocations and GPU pass timing. GPU timing may lag; gpuTimingFrame identifies its sample.";
	Info.bReadOnly = true;
	Info.Effects = "Reads counters without modifying the scene.";
	Info.Completion = "Current completed-frame snapshot.";
	Info.Unavailable = InDiagnostics ? "" : "Renderer diagnostics provider unavailable.";
	InCatalog.Register(
	    MakeOperation<FSceneInfoRequest, FRenderDiagnostics>(Info,
	                                                         [InDiagnostics](const auto&)
	                                                         {
		                                                         return InDiagnostics->RenderDiagnostics();
	                                                         }));
	Info.Id = "render.component_diagnostics";
	Info.Summary = "Inspect component publication, primitive readiness and draw failures";
	Info.Description = "Same published diagnostics as the Inspector. Limit 1-100 primitives. Compare publication "
	                   "tokens across pages; restart if the scene changes.";
	const FDiagnosticsRequest Example{{1, 0, 1}, "hyperion.staticmesh"};
	Info.Example = WriteRecordWire(RecordType<FDiagnosticsRequest>(), &Example);
	InCatalog.Register(MakeOperation<FDiagnosticsRequest, FDiagnosticsPage>(
	    Info,
	    [InDiagnostics](const auto& InRequest)
	    {
		    if (!InRequest.Limit || InRequest.Limit > 100)
		    {
			    throw std::invalid_argument("Limit must be 1-100");
		    }
		    FDiagnosticsPage Page;
		    FSceneComponentDiagnostics Value;
		    try
		    {
			    Value = InDiagnostics->ComponentDiagnostics(InRequest.Handle, InRequest.Component);
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
		    Page.Scene = Value.Publication.LogicalSceneIdentity;
		    Page.Epoch = Value.Publication.AttachmentEpoch;
		    Page.Publication = Value.Publication.PublicationSerial;
		    Page.Revision = Value.Publication.LogicalRevision;
		    Page.bApplied = Value.bApplied;
		    Page.Total = Value.Primitives.size();
		    const auto Begin = std::min(std::size_t(InRequest.Offset), Value.Primitives.size());
		    const auto End = std::min(Begin + InRequest.Limit, Value.Primitives.size());
		    if (End < Page.Total)
		    {
			    Page.Next = static_cast<std::uint32_t>(End);
		    }
		    Page.Primitives = {Value.Primitives.begin() + Begin, Value.Primitives.begin() + End};
		    return Page;
	    }));
}

void RegisterRenderOutput(FOperationCatalog& InCatalog, IRenderOutput* InOutput)
{
	FOperationInfo Info;
	Info.Id = "render.screenshot";
	Info.Owner = "automation-scene";
	Info.Summary = "Capture a subsequent completed target frame to PNG";
	Info.Description = "Uses the application's existing render graph readback. Requires a ready scene/preview and a "
	                   "drawable window. Captures the main window or Editor asset window including GUI. Paths are on "
	                   "the target machine; no bulk pixels cross the transport.";
	Info.Effects = "Writes the explicitly named PNG; existing destinations require overwrite=true.";
	Info.Completion =
	    "GPU readback and PNG write completed. Result contains target-local path, dimensions, frame and byte count.";
	Info.Unavailable = InOutput ? "" : "This target has no renderer output provider.";
	const FImageOutputRequest Example{"out/captures/Agent.png"};
	Info.Example = WriteRecordWire(RecordType<FImageOutputRequest>(), &Example);
	InCatalog.Register(MakeAsyncOperation<FImageOutputRequest, FImageArtifact>(
	    Info,
	    [InOutput](const auto& InRequest)
	    {
		    try
		    {
			    auto Pending = InOutput->RequestImage(InRequest);
			    return TPendingOperation<FImageArtifact>{[InOutput, Pending]()
			                                             {
				                                             try
				                                             {
					                                             return InOutput->PollImage(Pending);
				                                             }
				                                             catch (const FSceneEditError& Error)
				                                             {
					                                             throw FAutomationError(Error.Code, Error.what());
				                                             }
			                                             }};
		    }
		    catch (const FSceneEditError& Error)
		    {
			    throw FAutomationError(Error.Code, Error.what());
		    }
	    }));
}
} // namespace Hyperion
