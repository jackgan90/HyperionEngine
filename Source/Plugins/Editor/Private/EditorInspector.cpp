#include "EditorApplication.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include "Hyperion/SceneEditing/SceneComponentEditPolicy.h"
#include <algorithm>

namespace Hyperion
{
bool FEditorPlugin::DrawComponent(const FSceneNodeView& InView, const FSceneComponent& InComponent,
                                  std::uint64_t InRevision)
{
	const auto& Node = *InView.Node;
	const auto Identity = Node.Id + "/" + InComponent.Id;
	bool bRemove = false;
	const auto RemoveTooltip = "Remove " + InComponent.Type->Label + " component";
	const bool bOpen = Gui->Section((InComponent.Type->Label + "##" + Identity).c_str(), true,
	                                InComponent.Type->bRequired ? nullptr : &bRemove, RemoveTooltip.c_str());
	if (!Options.ExerciseDocument.empty() || !Options.ExerciseRenderControls.empty())
	{
		InspectionBounds[InComponent.Type->Id + "/header"] = Gui->LastItemBounds();
	}
	if (bRemove)
	{
		auto Candidate = Node;
		Candidate.Components.Remove(InComponent.Id);
		CommitEdit(InView.Handle, std::move(Candidate), InRevision);
		return true;
	}
	if (!bOpen)
	{
		return false;
	}
	Gui->Indent();
	if (InComponent.Type->CppType == typeid(FSceneDirectionalLight))
	{
		if (!Context.Require<FRenderFeatureRegistry>().Contains("contact-shadows"))
		{
			Gui->TextWrapped("Contact shadows are unavailable in this session. Light properties can still be saved.");
		}
		else if (ParseSceneRenderPipeline(Rendering.Pipeline) != ESceneRenderPipeline::Deferred)
		{
			Gui->TextWrapped("Contact shadows require the Deferred pipeline.");
		}
	}
	if (Scene->GetRevision() != InRevision)
	{
		Gui->Unindent();
		return true;
	}
	auto& Record = InspectorDrafts.Single(InComponent);
	Gui->BeginLiveEdit();
	const bool bChanged = Gui->EditRecord(Record, Identity, (Options.ExerciseDocument.empty() && Options.ExerciseRenderControls.empty()) ?
	    std::function<void(std::string_view, FVec4)>{} : [&](std::string_view InField, FVec4 InBounds)
	    { InspectionBounds[InComponent.Type->Id + "/" + std::string(InField)] = InBounds; },
	    [&](std::string_view InField, FPropertyPresentation& InOutPresentation)
	    {
		    InspectLightProperty(InView, InComponent, InField, InOutPresentation);
		    InOutPresentation.bReadOnly |= IsSceneComponentFieldReadOnly(*InComponent.Type->Record, InField);
	    });
	const auto Edit = Gui->EndLiveEdit();
	Gui->Unindent();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bChanged)
	{
		// Even rejected edits must rebuild from authoritative values on the next frame.
		auto Edited = InspectorDrafts.TakeSingle(InComponent.Id);
		try
		{
			auto Candidate = Node;
			auto* Value = Candidate.Components.Find(InComponent.Id)->Edit();
			Edited->ApplyToCandidate(Value);
			ValidateSceneComponentEdit(*InComponent.Type->Record, InComponent.Get(), Value);
			PendingInspectorEdit =
			    FPendingInspectorEdit{InView.Handle, std::move(Candidate), InRevision, Edit.ChangedInteraction};
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
	return false;
}

void FEditorPlugin::DrawObjectMetadata(const FSceneNodeView& InView)
{
	const auto& Node = *InView.Node;
	const auto Revision = Scene->GetRevision();
	auto Name = Node.Name;
	bool bEnabled = Node.bEnabled;
	Gui->BeginLiveEdit();
	Gui->BeginPropertyRow("Object name");
	const bool bNameChanged = Gui->InputText("##value", Name);
	if (Options.bExerciseSelectionShortcuts)
	{
		InspectionBounds["shortcut/name"] = Gui->LastItemBounds();
	}
	Gui->EndPropertyRow();
	Gui->BeginPropertyRow("Object enabled", nullptr, "Disabling this object also disables its children.");
	const bool bEnabledChanged = Gui->Checkbox("##value", bEnabled);
	Gui->Tooltip("Disabling this object also disables its children.");
	Gui->EndPropertyRow();
	const auto Edit = Gui->EndLiveEdit();
	if (Edit.ActiveInteraction)
	{
		InspectorInteraction = Edit.ActiveInteraction;
	}
	if (bNameChanged || bEnabledChanged)
	{
		auto Candidate = Node;
		Candidate.Name = std::move(Name);
		Candidate.bEnabled = bEnabled;
		PendingInspectorEdit =
		    FPendingInspectorEdit{InView.Handle, std::move(Candidate), Revision, Edit.ChangedInteraction};
	}
	if (Node.bEnabled && !InView.bEffectiveEnabled)
	{
		Gui->TextWrapped("Inactive because a parent object is disabled.");
	}
}

void FEditorPlugin::DrawComponentInspector(const FSceneNodeView& InView)
{
	const auto& Node = *InView.Node;
	const auto Revision = Scene->GetRevision();
	DrawObjectMetadata(InView);
	if (Scene->GetRevision() != Revision)
	{
		return;
	}
	if (Node.Camera())
	{
		DrawCameraActions(InView.Handle);
		if (Scene->GetRevision() != Revision)
		{
			return;
		}
	}
	for (const auto& Component : Node.Components.All())
	{
		if (!Component.Get())
		{
			continue;
		}
		if (DrawComponent(InView, Component, Revision))
		{
			return;
		}
	}
	for (const auto& [Id, Envelope] : Node.Components.Unknown())
	{
		Gui->TextWrapped("Unavailable component: " + Id + ". Saving is blocked until its type is registered.");
	}
	if (Gui->BeginMenu("Add component"))
	{
		for (const auto& Type : SceneComponentRegistry().All())
		{
			const bool bPresent =
			    Type->bUnique && std::any_of(Node.Components.All().begin(), Node.Components.All().end(),
			                                 [&](const auto& InComponent)
			                                 {
				                                 return InComponent.Type == Type && InComponent.Get();
			                                 });
			if (!bPresent && CanAddDefaultSceneComponent(*Type) && Gui->MenuItem(Type->Label.c_str()))
			{
				Gui->EndMenu();
				auto Candidate = Node;
				AddDefaultSceneComponent(Candidate, Type->Id + "-" + std::to_string(Revision + 1), Type->Id);
				CommitEdit(InView.Handle, std::move(Candidate), Revision);
				return;
			}
		}
		Gui->EndMenu();
	}
}
} // namespace Hyperion
