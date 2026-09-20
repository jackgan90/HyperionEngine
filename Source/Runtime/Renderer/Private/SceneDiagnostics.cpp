#include "Hyperion/Renderer/SceneBridge.h"
#include "SceneInstanceInternal.h"

namespace Hyperion
{
std::vector<FRenderPrimitiveHandle> FSceneInstance::ResolveRenderPrimitives(FSceneHandle InHandle) const
{
	Impl->RequireOpen();
	return Impl->Bridge->ResolveRenderPrimitives(InHandle);
}

std::shared_ptr<const FSceneComponentDiagnostics> FSceneRenderBridge::GetComponentDiagnostics(
    FSceneHandle InHandle, std::string_view InComponent) const
{
	Tasks.Require({EDomain::Main});
	const auto* Node = Scene.FindNode(InHandle);
	const auto* Component = Node ? Node->Components.Find(InComponent) : nullptr;
	const auto Attachment = Attachments.find(InHandle);
	if (!Component || Component->Type->CppType != typeid(FSceneModelComponent) || Attachment == Attachments.end() ||
	    !Metadata)
	{
		return {};
	}
	auto Snapshot = std::make_shared<FSceneComponentDiagnostics>();
	Snapshot->Object = InHandle;
	Snapshot->Component = InComponent;
	Snapshot->Publication = Metadata->Token;
	const auto& Model = *Attachment->second.Model;
	Snapshot->ExpectedPrimitiveRevision = Model.Revision;
	Snapshot->bApplied = Metadata->Token.LogicalRevision == Scene.GetRevision();
	for (std::size_t Index = 0; Index < Model.Bindings.size(); ++Index)
	{
		const auto Published = Model.Bindings[Index].GetDiagnostic();
		const auto& Draw = Published.LastDraw;
		const auto Section = Model.Instances.at(Index).Primitive;
		const auto& Primitive = Model.Asset->Primitives.at(Section);
		const bool bCurrent = Published.Status.Revision == Model.Revision;
		Snapshot->bApplied &= bCurrent;
		constexpr const char* Names[] = {"Pending creation", "Applied, awaiting resources", "Ready", "Removed",
		                                 "Failed"};
		Snapshot->Primitives.push_back({ModelPrimitiveId(*Model.Asset, Section), Primitive.Name,
		                                Published.Status.Revision, Published.AppliedWorld, Published.bAppliedVisible,
		                                Names[static_cast<unsigned>(Published.Status.State)], Draw.Frame, Draw.View,
		                                Draw.Usage, Draw.bReady && Draw.Revision == Published.Status.Revision,
		                                Published.Status.Error.empty() ? Draw.Error : Published.Status.Error,
		                                Published.Handle.Slot, Published.Handle.Generation});
	}
	return Snapshot;
}

std::shared_ptr<const FSceneComponentDiagnostics> FSceneInstance::GetComponentDiagnostics(
    FSceneHandle InHandle, std::string_view InComponent) const
{
	Impl->RequireOpen();
	return Impl->Bridge->GetComponentDiagnostics(InHandle, InComponent);
}

template<> const FRecordDescriptor& RecordType<FScenePrimitiveDiagnostic>()
{
	static const auto Type = MakeRecord<FScenePrimitiveDiagnostic>(
	    "hyperion.primitive-diagnostics",
	    {Member("primitive", &FScenePrimitiveDiagnostic::Primitive), Member("name", &FScenePrimitiveDiagnostic::Name),
	     Member("appliedRevision", &FScenePrimitiveDiagnostic::AppliedRevision),
	     Member("appliedWorld", &FScenePrimitiveDiagnostic::AppliedWorld),
	     Member("appliedVisible", &FScenePrimitiveDiagnostic::bAppliedVisible),
	     Member("status", &FScenePrimitiveDiagnostic::Status),
	     Member("lastDrawFrame", &FScenePrimitiveDiagnostic::LastDrawFrame),
	     Member("lastDrawView", &FScenePrimitiveDiagnostic::LastDrawView),
	     Member("lastDrawPass", &FScenePrimitiveDiagnostic::LastDrawPass),
	     Member("lastDrawReady", &FScenePrimitiveDiagnostic::bLastDrawReady),
	     Member("error", &FScenePrimitiveDiagnostic::Error),
	     Member("renderSlot", &FScenePrimitiveDiagnostic::RenderSlot),
	     Member("renderGeneration", &FScenePrimitiveDiagnostic::RenderGeneration)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneComponentDiagnostics>()
{
	static const auto Type = MakeRecord<FSceneComponentDiagnostics>(
	    "hyperion.component-diagnostics",
	    {Member("expectedRevision", &FSceneComponentDiagnostics::ExpectedPrimitiveRevision,
	            {.bPersistent = false, .Inspector = FPropertyPresentation{"Expected render revision", {}, true}}),
	     Member(
	         "applied", &FSceneComponentDiagnostics::bApplied,
	         {.bPersistent = false, .Inspector = FPropertyPresentation{"Render has applied this revision", {}, true}}),
	     Member("primitives", &FSceneComponentDiagnostics::Primitives,
	            {.bPersistent = false, .Inspector = FPropertyPresentation{"Render primitives", {}, true}})});
	return Type;
}
} // namespace Hyperion
